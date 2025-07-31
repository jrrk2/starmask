// BackgroundExtractor.cpp - Fixed implementation without PCL conflicts
#include "BackgroundExtractor.h"

// Initialize mock PCL API before including PCL headers
#include "PCLMockAPI.h"

// Minimal PCL includes to avoid conflicts
#include <pcl/api/APIInterface.h>

// For the math operations, use standard library instead of problematic PCL classes
#include <QDebug>
#include <QElapsedTimer>
#include <QMutex>
#include <QMutexLocker>
#include <QRandomGenerator>
#include <QtMath>
#include <QApplication>

// Standard includes
#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

class BackgroundExtractorPrivate
{
public:
    BackgroundExtractionSettings settings;
    BackgroundExtractionResult result;
    BackgroundExtractionResult previewResult;
    BackgroundExtractionWorker* worker = nullptr;
    QMutex mutex;
    bool mockInitialized = false;
    bool extracting = false;  
    
    // Manual samples
    QVector<QPoint> manualSamples;
    QVector<float> manualSampleValues;
    
    void initializePCLMock() {
        if (!mockInitialized) {
            qDebug() << "Initializing PCL Mock API for Background Extractor...";
            
            pcl_mock::SetDebugLogging(false);
            pcl_mock::InitializeMockAPI();
            
            if (!API) {
                API = new pcl::APIInterface(pcl_mock::GetMockFunctionResolver());
            }
            
            mockInitialized = true;
            qDebug() << "PCL Mock API initialized for Background Extractor";
        }
    }
};

// Implementation starts here

BackgroundExtractor::BackgroundExtractor(QObject* parent)
    : QObject(parent)
    , d(new BackgroundExtractorPrivate)
{
    d->initializePCLMock();
}

BackgroundExtractor::~BackgroundExtractor() = default;

void BackgroundExtractor::setSettings(const BackgroundExtractionSettings& settings)
{
    QMutexLocker locker(&d->mutex);
    d->settings = settings;
}

BackgroundExtractionSettings BackgroundExtractor::settings() const
{
    QMutexLocker locker(&d->mutex);
    return d->settings;
}

BackgroundExtractionSettings BackgroundExtractor::getDefaultSettings()
{
    BackgroundExtractionSettings defaults;
    // All default values are already set in the struct definition
    return defaults;
}

bool BackgroundExtractor::extractBackground(const ImageData& imageData)
{
    QMutexLocker locker(&d->mutex);
    
    if (d->worker) {
        qDebug() << "Background extraction already in progress";
        return false;
    }
    
    emit extractionStarted();
    
    d->worker = new BackgroundExtractionWorker(imageData, d->settings, this);
    
    connect(d->worker, &BackgroundExtractionWorker::finished,
            this, &BackgroundExtractor::onWorkerFinished);
    connect(d->worker, &BackgroundExtractionWorker::progress,
            this, &BackgroundExtractor::extractionProgress);
    connect(d->worker, &BackgroundExtractionWorker::progress,
            this, &BackgroundExtractor::progress);
    connect(d->worker, &QThread::finished,
            d->worker, &QObject::deleteLater);
    
    d->worker->start();
    return true;
}

void BackgroundExtractor::onWorkerFinished(const BackgroundExtractionResult& result)
{
    QMutexLocker locker(&d->mutex);
    d->result = result;
    d->worker = nullptr;
    locker.unlock();
    
    emit extractionCompleted(result);
    emit extractionFinished(result.success, result.errorMessage);
    
    if (result.success) {
        emit previewReady(result);
    }
}

BackgroundExtractionResult BackgroundExtractor::result() const
{
    QMutexLocker locker(&d->mutex);
    return d->result;
}

void BackgroundExtractor::clearResult()
{
    QMutexLocker locker(&d->mutex);
    d->result = BackgroundExtractionResult();
}

void BackgroundExtractor::addManualSample(const QPoint& point, float value)
{
    QMutexLocker locker(&d->mutex);
    d->manualSamples.append(point);
    d->manualSampleValues.append(value);
}

void BackgroundExtractor::clearManualSamples()
{
    QMutexLocker locker(&d->mutex);
    d->manualSamples.clear();
    d->manualSampleValues.clear();
}

QVector<QPoint> BackgroundExtractor::manualSamples() const
{
    QMutexLocker locker(&d->mutex);
    return d->manualSamples;
}

// BackgroundExtractionWorker implementation

BackgroundExtractionWorker::BackgroundExtractionWorker(const ImageData& imageData,
                                                     const BackgroundExtractionSettings& settings,
                                                     QObject* parent)
    : QThread(parent)
    , m_imageData(imageData)
    , m_settings(settings)
    , m_cancelled(false)
{
}

void BackgroundExtractionWorker::cancel()
{
    m_cancelled = true;
}

void BackgroundExtractionWorker::run()
{
    try {
        emit progress(0, "Starting background extraction...");
        
        if (m_cancelled) return;
        
        // Step 1: Generate samples
        emit progress(10, "Generating background samples...");
        if (!generateSamples()) {
            m_result.success = false;
            m_result.errorMessage = "Failed to generate sufficient background samples";
            emit finished(m_result);
            return;
        }
        
        if (m_cancelled) return;
        
        // Step 2: Fit background model
        emit progress(60, "Fitting background model...");
        if (!fitModel()) {
            m_result.success = false;
            m_result.errorMessage = "Failed to fit background model";
            emit finished(m_result);
            return;
        }
        
        if (m_cancelled) return;
        
        // Step 3: Apply background correction if requested
        if (m_settings.replaceTarget) {
            emit progress(90, "Applying background correction...");
            applyCorrection();
        }
        
        m_result.success = true;
        emit progress(100, "Background extraction completed");
        emit finished(m_result);
        
    } catch (const std::exception& e) {
        m_result.success = false;
        m_result.errorMessage = QString("Exception during background extraction: %1").arg(e.what());
        emit finished(m_result);
    }
}

bool BackgroundExtractionWorker::generateSamples()
{
    m_samples.clear();
    m_sampleValues.clear();
    m_sampleValid.clear();
    
    try {
        if (m_settings.sampleGeneration == SampleGeneration::Grid) {
            return generateGridSamples();
        } 
        else if (m_settings.sampleGeneration == SampleGeneration::Automatic) {
            return generateImprovedAutomaticSamples();
        }
        else if (m_settings.sampleGeneration == SampleGeneration::Manual) {
            // Use manual samples if available (this would come from the UI)
            return m_samples.size() >= m_settings.minSamples;
        }
        
    } catch (const std::exception& e) {
        qDebug() << "Error in sample generation:" << e.what();
        return generateGridSamples(); // Fallback
    }
    
    return false;
}

bool BackgroundExtractionWorker::generateImprovedAutomaticSamples()
{
    emit progress(15, "Analyzing image structure for automatic sampling...");
    
    int width = m_imageData.width;
    int height = m_imageData.height;
    const float* pixels = m_imageData.pixels.constData();
    
    // Analyze local variance to find uniform (background) regions
    QVector<float> localVariances;
    QVector<QPoint> candidatePoints;
    QVector<float> candidateValues;
    
    int blockSize = std::max(8, std::min(width, height) / 32);
    
    emit progress(30, "Computing local variance statistics...");
    
    for (int y = blockSize; y < height - blockSize; y += blockSize/2) {
        for (int x = blockSize; x < width - blockSize; x += blockSize/2) {
            
            if (m_cancelled) return false;
            
            // Calculate local statistics in this block
            QVector<float> blockValues;
            float sum = 0.0f;
            
            for (int dy = -blockSize/2; dy <= blockSize/2; ++dy) {
                for (int dx = -blockSize/2; dx <= blockSize/2; ++dx) {
                    int px = x + dx;
                    int py = y + dy;
                    if (px >= 0 && px < width && py >= 0 && py < height) {
                        int index = py * width + px;
                        if (index < m_imageData.pixels.size()) {
                            float val = pixels[index];
                            blockValues.append(val);
                            sum += val;
                        }
                    }
                }
            }
            
            if (blockValues.size() > 10) {
                float mean = sum / blockValues.size();
                
                // Calculate variance
                float variance = 0.0f;
                for (float val : blockValues) {
                    float diff = val - mean;
                    variance += diff * diff;
                }
                variance /= blockValues.size();
                
                localVariances.append(variance);
                candidatePoints.append(QPoint(x, y));
                candidateValues.append(mean);
            }
        }
    }
    
    emit progress(50, "Selecting optimal background sample points...");
    
    // Sort by variance (lowest first - these are likely background regions)
    QVector<int> indices;
    for (int i = 0; i < localVariances.size(); ++i) {
        indices.append(i);
    }
    
    std::sort(indices.begin(), indices.end(), [&](int a, int b) {
        return localVariances[a] < localVariances[b];
    });
    
    // Select samples from low-variance regions
    m_samples.clear();
    m_sampleValues.clear();
    m_sampleValid.clear();
    
    int maxSamples = std::min(m_settings.maxSamples, (int)(indices.size()));
    int minDistanceSquared = (blockSize * blockSize) / 4; // Minimum distance between samples
    
    for (int i = 0; i < indices.size() && m_samples.size() < maxSamples; ++i) {
        int idx = indices[i];
        QPoint newPoint = candidatePoints[idx];
        
        // Check if this point is too close to existing samples
        bool tooClose = false;
        for (const QPoint& existing : m_samples) {
            int dx = newPoint.x() - existing.x();
            int dy = newPoint.y() - existing.y();
            if (dx*dx + dy*dy < minDistanceSquared) {
                tooClose = true;
                break;
            }
        }
        
        if (!tooClose) {
            m_samples.append(newPoint);
            m_sampleValues.append(candidateValues[idx]);
            m_sampleValid.append(true);
        }
    }
    
    qDebug() << "Improved automatic sampling generated" << m_samples.size() 
             << "samples from" << candidatePoints.size() << "candidates";
    
    // Validate sample count
    if (m_samples.size() < m_settings.minSamples) {
        qDebug() << "Insufficient samples from improved method:" << m_samples.size() 
                 << "minimum:" << m_settings.minSamples << "- using grid fallback";
        return generateGridSamples();
    }
    
    // Store results
    m_result.samplesUsed = m_samples.size();
    m_result.samplePoints = m_samples;
    m_result.sampleValues = m_sampleValues;
    m_result.sampleRejected.resize(m_samples.size());
    std::fill(m_result.sampleRejected.begin(), m_result.sampleRejected.end(), false);
    
    emit progress(55, "Automatic sample generation completed");
    return true;
}

bool BackgroundExtractionWorker::generateGridSamples()
{
    emit progress(20, "Generating grid-based samples...");
    
    m_samples.clear();
    m_sampleValues.clear();
    m_sampleValid.clear();
    
    int width = m_imageData.width;
    int height = m_imageData.height;
    const float* pixels = m_imageData.pixels.constData();
    
    int stepX = width / (m_settings.gridColumns + 1);
    int stepY = height / (m_settings.gridRows + 1);
    
    for (int row = 1; row <= m_settings.gridRows; ++row) {
        for (int col = 1; col <= m_settings.gridColumns; ++col) {
            if (m_cancelled) return false;
            
            int x = col * stepX;
            int y = row * stepY;
            
            if (x < width && y < height) {
                int index = y * width + x;
                if (index < m_imageData.pixels.size()) {
                    m_samples.append(QPoint(x, y));
                    m_sampleValues.append(pixels[index]);
                    m_sampleValid.append(true);
                }
            }
        }
    }
    
    // Store results
    m_result.samplesUsed = m_samples.size();
    m_result.samplePoints = m_samples;
    m_result.sampleValues = m_sampleValues;
    m_result.sampleRejected.resize(m_samples.size());
    std::fill(m_result.sampleRejected.begin(), m_result.sampleRejected.end(), false);
    
    qDebug() << "Grid sampling generated" << m_samples.size() << "samples";
    return m_samples.size() >= m_settings.minSamples;
}

bool BackgroundExtractionWorker::fitModel()
{
    if (m_samples.isEmpty()) {
        qDebug() << "No samples available for model fitting";
        return false;
    }
    
    emit progress(65, "Performing outlier rejection...");
    
    // Perform outlier rejection if enabled
    if (m_settings.useOutlierRejection) {
        rejectOutliers();
    }
    
    // Count valid samples
    int validSamples = 0;
    for (bool valid : m_sampleValid) {
        if (valid) validSamples++;
    }
    
    if (validSamples < 6) {
        qDebug() << "Too few valid samples for fitting:" << validSamples;
        return false;
    }
    
    emit progress(70, "Fitting polynomial background model...");
    
    return fitPolynomialModel();
}

bool BackgroundExtractionWorker::solveLinearSystem(const std::vector<std::vector<double>>& A, 
                                                 const std::vector<double>& b, 
                                                 std::vector<double>& x)
{
    int n = A.size();
    if (n == 0 || A[0].size() != n || b.size() != n) {
        return false;
    }
    
    // Create augmented matrix
    std::vector<std::vector<double>> augmented(n, std::vector<double>(n + 1));
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            augmented[i][j] = A[i][j];
        }
        augmented[i][n] = b[i];
    }
    
    // Gaussian elimination with partial pivoting
    for (int i = 0; i < n; ++i) {
        // Find pivot
        int pivot = i;
        for (int j = i + 1; j < n; ++j) {
            if (std::abs(augmented[j][i]) > std::abs(augmented[pivot][i])) {
                pivot = j;
            }
        }
        
        // Swap rows if needed
        if (pivot != i) {
            std::swap(augmented[i], augmented[pivot]);
        }
        
        // Check for singular matrix
        if (std::abs(augmented[i][i]) < 1e-12) {
            return false;
        }
        
        // Eliminate
        for (int j = i + 1; j < n; ++j) {
            double factor = augmented[j][i] / augmented[i][i];
            for (int k = i; k <= n; ++k) {
                augmented[j][k] -= factor * augmented[i][k];
            }
        }
    }
    
    // Back substitution
    x.resize(n);
    for (int i = n - 1; i >= 0; --i) {
        x[i] = augmented[i][n];
        for (int j = i + 1; j < n; ++j) {
            x[i] -= augmented[i][j] * x[j];
        }
        x[i] /= augmented[i][i];
    }
    
    return true;
}

int BackgroundExtractionWorker::getModelOrder() const
{
    switch (m_settings.model) {
    case BackgroundModel::Linear: return 1;
    case BackgroundModel::Polynomial2: return 2;
    case BackgroundModel::Polynomial3: return 3;
    default: return 2;
    }
}

int BackgroundExtractionWorker::getNumTerms(int order) const
{
    switch (order) {
    case 1: return 3;  // 1, x, y
    case 2: return 6;  // 1, x, y, x², xy, y²
    case 3: return 10; // 1, x, y, x², xy, y², x³, x²y, xy², y³
    default: return 6;
    }
}

double BackgroundExtractionWorker::evaluatePolynomial(const std::vector<double>& coeffs, double x, double y, int order) const
{
    double result = coeffs[0]; // Constant term
    
    if (order >= 1 && coeffs.size() > 2) {
        result += coeffs[1] * x;    // Linear x
        result += coeffs[2] * y;    // Linear y
    }
    
    if (order >= 2 && coeffs.size() > 5) {
        result += coeffs[3] * x * x;    // Quadratic x²
        result += coeffs[4] * x * y;    // Cross term xy
        result += coeffs[5] * y * y;    // Quadratic y²
    }
    
    if (order >= 3 && coeffs.size() > 9) {
        result += coeffs[6] * x * x * x;    // Cubic x³
        result += coeffs[7] * x * x * y;    // x²y
        result += coeffs[8] * x * y * y;    // xy²
        result += coeffs[9] * y * y * y;    // Cubic y³
    }
    
    return result;
}

void BackgroundExtractionWorker::rejectOutliers()
{
    // Simple sigma clipping for outlier rejection
    QVector<float> validValues;
    for (int i = 0; i < m_sampleValid.size(); ++i) {
        if (m_sampleValid[i]) {
            validValues.append(m_sampleValues[i]);
        }
    }
    
    if (validValues.size() < 10) return; // Not enough data for outlier rejection
    
    // Iterate rejection process
    for (int iter = 0; iter < m_settings.rejectionIterations; ++iter) {
        // Calculate median and MAD (Median Absolute Deviation)
        std::sort(validValues.begin(), validValues.end());
        float median = validValues[validValues.size() / 2];
        
        QVector<float> deviations;
        for (float val : validValues) {
            deviations.append(std::abs(val - median));
        }
        std::sort(deviations.begin(), deviations.end());
        float mad = deviations[deviations.size() / 2];
        
        // Reject samples beyond threshold
        float threshold = median + m_settings.rejectionHigh * mad * 1.4826f; // 1.4826 is scaling factor for normal distribution
        float lowerThreshold = median - m_settings.rejectionLow * mad * 1.4826f;
        
        int rejectedCount = 0;
        validValues.clear();
        
        for (int i = 0; i < m_sampleValues.size(); ++i) {
            if (m_sampleValid[i]) {
                if (m_sampleValues[i] > threshold || m_sampleValues[i] < lowerThreshold) {
                    m_sampleValid[i] = false;
                    rejectedCount++;
                } else {
                    validValues.append(m_sampleValues[i]);
                }
            }
        }
        
        if (rejectedCount == 0) break; // No more outliers found
    }
    
    int totalRejected = 0;
    for (bool valid : m_sampleValid) {
        if (!valid) totalRejected++;
    }
    
    qDebug() << "Outlier rejection: rejected" << totalRejected << "samples total";
}

void BackgroundExtractionWorker::calculateErrorMetrics()
{
    double errorSum = 0.0;
    double maxError = 0.0;
    int errorCount = 0;
    
    for (int i = 0; i < m_samples.size(); ++i) {
        if (m_sampleValid[i]) {
            const QPoint& point = m_samples[i];
            int x = point.x();
            int y = point.y();
            
            if (x >= 0 && x < m_imageData.width && y >= 0 && y < m_imageData.height) {
                int index = y * m_imageData.width + x;
                if (index < m_result.backgroundData.size() && index < m_imageData.pixels.size()) {
                    double error = std::abs(m_result.backgroundData[index] - m_imageData.pixels[index]);
                    errorSum += error * error;
                    maxError = std::max(maxError, error);
                    errorCount++;
                }
            }
        }
    }
    
    if (errorCount > 0) {
        m_result.rmsError = std::sqrt(errorSum / errorCount);
        m_result.meanDeviation = std::sqrt(errorSum) / errorCount;
        m_result.maxDeviation = maxError;
    }
    
    // Update sample rejection information
    m_result.sampleRejected.resize(m_samples.size());
    for (int i = 0; i < m_sampleValid.size(); ++i) {
        m_result.sampleRejected[i] = !m_sampleValid[i];
    }
}

void BackgroundExtractionWorker::applyCorrection()
{
    if (m_result.backgroundData.size() != m_imageData.pixels.size()) {
        qDebug() << "Background data size mismatch during correction";
        return;
    }
    
    m_result.correctedData = m_imageData.pixels;
    
    for (int i = 0; i < m_result.correctedData.size(); ++i) {
        if (m_cancelled) return;
        m_result.correctedData[i] -= m_result.backgroundData[i];
    }
}

// Add these CORRECTED missing method implementations to BackgroundExtractor.cpp

bool BackgroundExtractor::generatePreview(const ImageData& imageData, int maxSize)
{
    if (isExtracting()) {
        return false;
    }
    
    // Create a smaller version of the image for preview
    ImageData previewData = imageData;
    
    // Scale down if necessary
    if (imageData.width > maxSize || imageData.height > maxSize) {
        double scale = double(maxSize) / std::max(imageData.width, imageData.height);
        previewData.width = int(imageData.width * scale);
        previewData.height = int(imageData.height * scale);
        
        // Simple downsampling
        previewData.pixels.resize(previewData.width * previewData.height * previewData.channels);
        
        for (int y = 0; y < previewData.height; ++y) {
            for (int x = 0; x < previewData.width; ++x) {
                for (int c = 0; c < previewData.channels; ++c) {
                    int srcX = int(x / scale);
                    int srcY = int(y / scale);
                    srcX = std::min(srcX, imageData.width - 1);
                    srcY = std::min(srcY, imageData.height - 1);
                    
                    int srcIndex = (srcY * imageData.width + srcX) + c * imageData.width * imageData.height;
                    int dstIndex = (y * previewData.width + x) + c * previewData.width * previewData.height;
                    
                    if (srcIndex < imageData.pixels.size() && dstIndex < previewData.pixels.size()) {
                        previewData.pixels[dstIndex] = imageData.pixels[srcIndex];
                    }
                }
            }
        }
    }
    
    // Run extraction on preview data
    return extractBackgroundAsync(previewData);
}

void BackgroundExtractor::cancelExtraction()
{
    QMutexLocker locker(&d->mutex);
    if (d->worker) {
        d->worker->cancel();
    }
}

bool BackgroundExtractor::isExtracting() const
{
    QMutexLocker locker(&d->mutex);
    return d->extracting;  // This should exist in BackgroundExtractorPrivate
}

QVector<QPoint> BackgroundExtractor::getManualSamples() const
{
    QMutexLocker locker(&d->mutex);
    return d->manualSamples;
}

bool BackgroundExtractor::hasResult() const
{
    QMutexLocker locker(&d->mutex);
    return d->result.success;
}

BackgroundExtractionSettings BackgroundExtractor::getConservativeSettings()
{
    BackgroundExtractionSettings settings;
    settings.model = BackgroundModel::Linear;              // Use correct enum
    settings.sampleGeneration = SampleGeneration::Grid;    // Use correct enum
    settings.tolerance = 1.0;
    settings.deviation = 3.0;
    settings.minSamples = 100;
    settings.maxSamples = 300;
    settings.useOutlierRejection = true;                   // Use correct member name
    settings.rejectionLow = 3.0;                          // Use correct member names
    settings.rejectionHigh = 3.0;
    settings.rejectionIterations = 5;
    settings.gridRows = 8;                                 // Use correct member names
    settings.gridColumns = 8;
    settings.replaceTarget = false;
    return settings;
}

BackgroundExtractionSettings BackgroundExtractor::getAggressiveSettings()
{
    BackgroundExtractionSettings settings;
    settings.model = BackgroundModel::Polynomial3;         // Use correct enum
    settings.sampleGeneration = SampleGeneration::Automatic; // Use correct enum
    settings.tolerance = 0.5;
    settings.deviation = 2.0;
    settings.minSamples = 200;
    settings.maxSamples = 1000;
    settings.useOutlierRejection = true;                   // Use correct member name
    settings.rejectionLow = 2.0;                          // Use correct member names  
    settings.rejectionHigh = 2.0;
    settings.rejectionIterations = 3;
    settings.gridRows = 16;                               // Use correct member names
    settings.gridColumns = 16;
    settings.replaceTarget = false;
    return settings;
}

// Add this method to BackgroundExtractionWorker class
bool BackgroundExtractionWorker::fitPolynomialModel()
{
    if (m_samples.isEmpty()) {
        return false;
    }
    
    int order = getModelOrder();
    int numTerms = getNumTerms(order);
    
    if (m_samples.size() < numTerms) {
        return false;
    }
    
    // Build coefficient matrix A and result vector b
    std::vector<std::vector<double>> A(m_samples.size(), std::vector<double>(numTerms));
    std::vector<double> b(m_samples.size());
    
    for (int i = 0; i < m_samples.size(); ++i) {
        if (!m_sampleValid[i]) continue;
        
        double x = double(m_samples[i].x()) / m_imageData.width;
        double y = double(m_samples[i].y()) / m_imageData.height;
        
        int termIndex = 0;
        for (int py = 0; py <= order; ++py) {
            for (int px = 0; px <= order - py; ++px) {
                A[i][termIndex] = std::pow(x, px) * std::pow(y, py);
                termIndex++;
            }
        }
        
        b[i] = m_sampleValues[i];
    }
    
    // Solve linear system
    std::vector<double> coefficients;
    if (!solveLinearSystem(A, b, coefficients)) {
        return false;
    }
    
    // Generate background surface
    m_result.backgroundData.resize(m_imageData.width * m_imageData.height);
    
    for (int y = 0; y < m_imageData.height; ++y) {
        for (int x = 0; x < m_imageData.width; ++x) {
            double nx = double(x) / m_imageData.width;
            double ny = double(y) / m_imageData.height;
            
            double value = evaluatePolynomial(coefficients, nx, ny, order);
            m_result.backgroundData[y * m_imageData.width + x] = float(value);
        }
    }
    
    return true;
}

bool BackgroundExtractor::extractBackgroundAsync(const ImageData& imageData)
{
    if (d->worker) {
        qDebug() << "Background extraction already in progress";
        return false;
    }
    
    if (!imageData.isValid()) {
        d->result.success = false;
        d->result.errorMessage = "Invalid image data";
        emit extractionFinished(false, QString(d->result.errorMessage));
        return false;
    }
    
    d->initializePCLMock();
    
    d->worker = new BackgroundExtractionWorker(imageData, d->settings, this);
    
    connect(d->worker, &BackgroundExtractionWorker::finished,
            this, &BackgroundExtractor::onWorkerFinished);
    
    emit extractionStarted();
    d->worker->start();
    return true;
}

