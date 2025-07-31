#include "BackgroundExtractor.h"
#include "ImageReader.h"

// Initialize mock PCL API before including PCL headers
#include "PCLMockAPI.h"

// PCL includes for background extraction
#include <pcl/Image.h>
#include <pcl/Matrix.h>
#include <pcl/Vector.h>
#include <pcl/LinearFit.h>
#include <pcl/api/APIInterface.h>

// Qt includes
#include <QDebug>
#include <QElapsedTimer>  // Use QElapsedTimer instead of QTime
#include <QMutex>
#include <QMutexLocker>
#include <QRandomGenerator>
#include <QtMath>
#include <QApplication>

// Standard includes
#include <algorithm>
#include <cmath>
#include <random>

class BackgroundExtractorPrivate
{
public:
    BackgroundExtractionSettings settings;
    BackgroundExtractionResult result;
    BackgroundExtractionResult previewResult;
    BackgroundExtractionWorker* worker = nullptr;
    QMutex mutex;
    bool mockInitialized = false;
    
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

BackgroundExtractor::BackgroundExtractor(QObject* parent)
    : QObject(parent)
    , d(std::make_unique<BackgroundExtractorPrivate>())
{
    d->settings = getDefaultSettings();
}

BackgroundExtractor::~BackgroundExtractor()
{
    if (d->worker) {
        d->worker->requestCancel();
        d->worker->wait(5000); // Wait up to 5 seconds
        delete d->worker;
    }
}

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

bool BackgroundExtractor::extractBackground(const ImageData& imageData)
{
    if (!imageData.isValid()) {
        d->result.success = false;
        d->result.errorMessage = "Invalid image data";
        return false;
    }
    
    d->initializePCLMock();
    
    emit extractionStarted();
    
    BackgroundExtractionWorker worker(imageData, d->settings);
    
    // Connect progress signal
    connect(&worker, &BackgroundExtractionWorker::progress,
            this, &BackgroundExtractor::extractionProgress);
    
    // Run the worker's processing method directly (not the thread)
    worker.processExtraction();
    
    d->result = worker.result();
    
    emit extractionFinished(d->result.success);
    return d->result.success;
}

void BackgroundExtractor::extractBackgroundAsync(const ImageData& imageData)
{
    if (d->worker) {
        qDebug() << "Background extraction already in progress";
        return;
    }
    
    if (!imageData.isValid()) {
        d->result.success = false;
        d->result.errorMessage = "Invalid image data";
        emit extractionFinished(false);
        return;
    }
    
    d->initializePCLMock();
    
    d->worker = new BackgroundExtractionWorker(imageData, d->settings, this);
    
    connect(d->worker, &BackgroundExtractionWorker::finished,
            this, &BackgroundExtractor::onWorkerFinished);
    connect(d->worker, &BackgroundExtractionWorker::progress,
            this, &BackgroundExtractor::onWorkerProgress);
    
    emit extractionStarted();
    d->worker->start();
}

void BackgroundExtractor::cancelExtraction()
{
    if (d->worker) {
        d->worker->requestCancel();
        emit extractionCancelled();
    }
}

bool BackgroundExtractor::isExtracting() const
{
    return d->worker && d->worker->isRunning();
}

const BackgroundExtractionResult& BackgroundExtractor::result() const
{
    return d->result;
}

bool BackgroundExtractor::hasResult() const
{
    return d->result.isValid();
}

void BackgroundExtractor::clearResult()
{
    d->result.clear();
}

bool BackgroundExtractor::generatePreview(const ImageData& imageData, int previewSize)
{
    if (!imageData.isValid()) {
        return false;
    }
    
    // Create a downsampled version for preview
    int scale = std::max(imageData.width, imageData.height) / previewSize;
    if (scale < 1) scale = 1;
    
    int previewWidth = imageData.width / scale;
    int previewHeight = imageData.height / scale;
    
    if (previewWidth < 32 || previewHeight < 32) {
        // Too small for meaningful preview
        return false;
    }
    
    // Create preview image data
    ImageData previewData;
    previewData.width = previewWidth;
    previewData.height = previewHeight;
    previewData.channels = imageData.channels;
    previewData.format = imageData.format;
    previewData.colorSpace = imageData.colorSpace;
    
    int previewPixels = previewWidth * previewHeight * imageData.channels;
    previewData.pixels.resize(previewPixels);
    
    // Downsample the image
    for (int c = 0; c < imageData.channels; ++c) {
        for (int y = 0; y < previewHeight; ++y) {
            for (int x = 0; x < previewWidth; ++x) {
                int srcX = x * scale;
                int srcY = y * scale;
                
                if (srcX < imageData.width && srcY < imageData.height) {
                    int srcIndex = (srcY * imageData.width + srcX) + c * imageData.width * imageData.height;
                    int dstIndex = (y * previewWidth + x) + c * previewWidth * previewHeight;
                    previewData.pixels[dstIndex] = imageData.pixels[srcIndex];
                }
            }
        }
    }
    
    // Extract background from preview
    BackgroundExtractionWorker worker(previewData, d->settings);
    worker.processExtraction();
    
    d->previewResult = worker.result();
    
    if (d->previewResult.success) {
        emit previewReady();
    }
    
    return d->previewResult.success;
}

const BackgroundExtractionResult& BackgroundExtractor::previewResult() const
{
    return d->previewResult;
}

void BackgroundExtractor::addManualSample(const QPoint& point, float value)
{
    d->manualSamples.append(point);
    d->manualSampleValues.append(value);
}

void BackgroundExtractor::removeManualSample(const QPoint& point)
{
    for (int i = d->manualSamples.size() - 1; i >= 0; --i) {
        if (d->manualSamples[i] == point) {
            d->manualSamples.removeAt(i);
            d->manualSampleValues.removeAt(i);
            break;
        }
    }
}

void BackgroundExtractor::clearManualSamples()
{
    d->manualSamples.clear();
    d->manualSampleValues.clear();
}

QVector<QPoint> BackgroundExtractor::getManualSamples() const
{
    return d->manualSamples;
}

QString BackgroundExtractor::getModelName(BackgroundModel model)
{
    switch (model) {
    case BackgroundModel::Linear: return "Linear";
    case BackgroundModel::Polynomial2: return "Polynomial (2nd order)";
    case BackgroundModel::Polynomial3: return "Polynomial (3rd order)";
    case BackgroundModel::RBF: return "Radial Basis Function";
    default: return "Unknown";
    }
}

QString BackgroundExtractor::getSampleGenerationName(SampleGeneration generation)
{
    switch (generation) {
    case SampleGeneration::Automatic: return "Automatic";
    case SampleGeneration::Manual: return "Manual";
    case SampleGeneration::Grid: return "Regular Grid";
    default: return "Unknown";
    }
}

BackgroundExtractionSettings BackgroundExtractor::getDefaultSettings()
{
    BackgroundExtractionSettings settings;
    settings.model = BackgroundModel::Polynomial2;
    settings.sampleGeneration = SampleGeneration::Automatic;
    settings.tolerance = 1.0;
    settings.deviation = 0.8;
    settings.minSamples = 50;
    settings.maxSamples = 2000;
    settings.useOutlierRejection = true;
    settings.rejectionLow = 2.0;
    settings.rejectionHigh = 2.5;
    settings.rejectionIterations = 3;
    return settings;
}

BackgroundExtractionSettings BackgroundExtractor::getConservativeSettings()
{
    BackgroundExtractionSettings settings = getDefaultSettings();
    settings.tolerance = 1.5;
    settings.deviation = 1.2;
    settings.minSamples = 100;
    settings.rejectionLow = 1.5;
    settings.rejectionHigh = 2.0;
    return settings;
}

BackgroundExtractionSettings BackgroundExtractor::getAggressiveSettings()
{
    BackgroundExtractionSettings settings = getDefaultSettings();
    settings.tolerance = 0.5;
    settings.deviation = 0.5;
    settings.maxSamples = 5000;
    settings.rejectionLow = 2.5;
    settings.rejectionHigh = 3.0;
    settings.rejectionIterations = 5;
    return settings;
}

void BackgroundExtractor::onWorkerFinished()
{
    if (d->worker) {
        d->result = d->worker->result();
        d->worker->deleteLater();
        d->worker = nullptr;
        
        emit extractionFinished(d->result.success);
    }
}

void BackgroundExtractor::onWorkerProgress(int percentage, const QString& stage)
{
    emit extractionProgress(percentage, stage);
}

// Worker Thread Implementation
BackgroundExtractionWorker::BackgroundExtractionWorker(const ImageData& imageData,
                                                       const BackgroundExtractionSettings& settings,
                                                       QObject* parent)
    : QThread(parent)
    , m_imageData(imageData)
    , m_settings(settings)
{
}

void BackgroundExtractionWorker::run()
{
    processExtraction();
}

bool BackgroundExtractionWorker::processExtraction()
{
    QElapsedTimer timer;  // Use QElapsedTimer instead of QTime
    timer.start();
    
    m_result.clear();
    
    try {
        if (!performExtraction()) {
            m_result.success = false;
            return false;
        }
        
        m_result.success = true;
        m_result.processingTimeSeconds = timer.elapsed() / 1000.0;  // elapsed() returns milliseconds
        
    } catch (const std::exception& e) {
        m_result.success = false;
        m_result.errorMessage = QString("Extraction error: %1").arg(e.what());
    } catch (...) {
        m_result.success = false;
        m_result.errorMessage = "Unknown extraction error";
    }
    
    return m_result.success;
}

bool BackgroundExtractionWorker::performExtraction()
{
    if (m_cancelled) return false;
    
    emit progress(10, "Generating background samples...");
    
    if (!generateSamples()) {
        m_result.errorMessage = "Failed to generate background samples";
        return false;
    }
    
    if (m_cancelled) return false;
    emit progress(40, "Fitting background model...");
    
    if (!fitModel()) {
        m_result.errorMessage = "Failed to fit background model";
        return false;
    }
    
    if (m_cancelled) return false;
    emit progress(70, "Evaluating background model...");
    
    if (!evaluateModel()) {
        m_result.errorMessage = "Failed to evaluate background model";
        return false;
    }
    
    if (m_cancelled) return false;
    emit progress(90, "Applying background correction...");
    
    // Create corrected image
    m_result.correctedData = m_imageData.pixels;
    for (int i = 0; i < m_result.correctedData.size(); ++i) {
        if (i < m_result.backgroundData.size()) {
            m_result.correctedData[i] -= m_result.backgroundData[i];
        }
    }
    
    emit progress(100, "Background extraction complete");
    return true;
}

bool BackgroundExtractionWorker::generateSamples()
{
    m_samples.clear();
    m_sampleValues.clear();
    m_sampleValid.clear();
    
    int width = m_imageData.width;
    int height = m_imageData.height;
    
    if (m_settings.sampleGeneration == SampleGeneration::Grid) {
        // Regular grid sampling
        int stepX = width / (m_settings.gridColumns + 1);
        int stepY = height / (m_settings.gridRows + 1);
        
        for (int row = 1; row <= m_settings.gridRows; ++row) {
            for (int col = 1; col <= m_settings.gridColumns; ++col) {
                int x = col * stepX;
                int y = row * stepY;
                
                if (x < width && y < height) {
                    int index = y * width + x;
                    m_samples.append(QPoint(x, y));
                    m_sampleValues.append(m_imageData.pixels[index]);
                    m_sampleValid.append(true);
                }
            }
        }
    } else if (m_settings.sampleGeneration == SampleGeneration::Automatic) {
        // Automatic sample generation using a simplified algorithm
        // This would normally use PCL's more sophisticated algorithms
        
        // Calculate image statistics for sample generation
        float imageMin = *std::min_element(m_imageData.pixels.begin(), m_imageData.pixels.end());
        float imageMax = *std::max_element(m_imageData.pixels.begin(), m_imageData.pixels.end());
        
        // Calculate mean and standard deviation
        double sum = 0.0;
        for (float value : m_imageData.pixels) {
            sum += value;
        }
        double mean = sum / m_imageData.pixels.size();
        
        double sumSq = 0.0;
        for (float value : m_imageData.pixels) {
            double diff = value - mean;
            sumSq += diff * diff;
        }
        double stdDev = std::sqrt(sumSq / m_imageData.pixels.size());
        
        // Generate random sample points and select based on local statistics
        QRandomGenerator* rng = QRandomGenerator::global();
        int attempts = m_settings.maxSamples * 3; // Try more points than needed
        
        for (int i = 0; i < attempts && m_samples.size() < m_settings.maxSamples; ++i) {
            if (m_cancelled) return false;
            
            int x = rng->bounded(width);
            int y = rng->bounded(height);
            
            // Skip if too close to existing samples
            bool tooClose = false;
            for (const QPoint& existing : m_samples) {
                int dx = x - existing.x();
                int dy = y - existing.y();
                if (dx*dx + dy*dy < 100) { // Minimum distance of 10 pixels
                    tooClose = true;
                    break;
                }
            }
            
            if (tooClose) continue;
            
            // Evaluate local neighborhood for background characteristics
            int kernelSize = 5;
            int halfKernel = kernelSize / 2;
            
            QVector<float> neighborhood;
            for (int dy = -halfKernel; dy <= halfKernel; ++dy) {
                for (int dx = -halfKernel; dx <= halfKernel; ++dx) {
                    int nx = x + dx;
                    int ny = y + dy;
                    
                    if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                        int index = ny * width + nx;
                        neighborhood.append(m_imageData.pixels[index]);
                    }
                }
            }
            
            if (neighborhood.isEmpty()) continue;
            
            // Calculate local statistics
            std::sort(neighborhood.begin(), neighborhood.end());
            float localMedian = neighborhood[neighborhood.size() / 2];
            
            // Use median for robustness
            float localSum = 0.0f;
            for (float val : neighborhood) {
                localSum += val;
            }
            float localMean = localSum / neighborhood.size();
            
            // Check if this looks like background (low values, not much structure)
            float threshold = mean - m_settings.deviation * stdDev;
            
            if (localMedian < threshold) {
                int index = y * width + x;
                m_samples.append(QPoint(x, y));
                m_sampleValues.append(m_imageData.pixels[index]);
                m_sampleValid.append(true);
            }
        }
    }
    
    // Ensure we have enough samples
    if (m_samples.size() < m_settings.minSamples) {
        m_result.errorMessage = QString("Insufficient samples generated: %1 (minimum: %2)")
                                .arg(m_samples.size()).arg(m_settings.minSamples);
        return false;
    }
    
    m_result.samplesUsed = m_samples.size();
    m_result.samplePoints = m_samples;
    m_result.sampleValues = m_sampleValues;
    m_result.sampleRejected.fill(false, m_samples.size());
    
    qDebug() << "Generated" << m_samples.size() << "background samples";
    return true;
}

bool BackgroundExtractionWorker::fitModel()
{
    if (m_samples.isEmpty()) return false;
    
    // For this mock implementation, we'll use a simple polynomial fitting
    // In a real PCL implementation, you'd use more sophisticated fitting methods
    
    int validSamples = 0;
    for (bool valid : m_sampleValid) {
        if (valid) validSamples++;
    }
    
    if (validSamples < 10) {
        m_result.errorMessage = "Too few valid samples for fitting";
        return false;
    }
    
    // Perform outlier rejection if enabled
    if (m_settings.useOutlierRejection) {
        rejectOutliers();
    }
    
    // The actual model fitting would use PCL's LinearFit or similar classes
    // For this mock, we'll create a simplified background model
    
    // Calculate RMS error (simplified)
    double errorSum = 0.0;
    int errorCount = 0;
    
    for (int i = 0; i < m_samples.size(); ++i) {
        if (m_sampleValid[i]) {
            // Simple error calculation
            double error = std::abs(m_sampleValues[i] - 0.1); // Assume low background
            errorSum += error * error;
            errorCount++;
        }
    }
    
    if (errorCount > 0) {
        m_result.rmsError = std::sqrt(errorSum / errorCount);
        m_result.meanDeviation = errorSum / errorCount;
        m_result.maxDeviation = m_result.meanDeviation * 2; // Simplified
    }
    
    return true;
}

bool BackgroundExtractionWorker::evaluateModel()
{
    // Create the background model data
    int totalPixels = m_imageData.width * m_imageData.height * m_imageData.channels;
    m_result.backgroundData.resize(totalPixels);
    
    // For this mock implementation, create a simple background model
    // In real PCL, this would evaluate the fitted model at each pixel
    
    for (int c = 0; c < m_imageData.channels; ++c) {
        for (int y = 0; y < m_imageData.height; ++y) {
            for (int x = 0; x < m_imageData.width; ++x) {
                int index = (y * m_imageData.width + x) + c * m_imageData.width * m_imageData.height;
                
                // Create a simple gradient background model
                double nx = double(x) / m_imageData.width;   // Normalized x
                double ny = double(y) / m_imageData.height;  // Normalized y
                
                float backgroundValue = 0.0f;
                
                switch (m_settings.model) {
                case BackgroundModel::Linear:
                    backgroundValue = 0.05f + 0.02f * nx + 0.01f * ny;
                    break;
                case BackgroundModel::Polynomial2:
                    backgroundValue = 0.05f + 0.02f * nx + 0.01f * ny + 
                                    0.01f * nx * nx + 0.005f * ny * ny;
                    break;
                case BackgroundModel::Polynomial3:
                    backgroundValue = 0.05f + 0.02f * nx + 0.01f * ny + 
                                    0.01f * nx * nx + 0.005f * ny * ny +
                                    0.002f * nx * nx * nx + 0.001f * ny * ny * ny;
                    break;
                case BackgroundModel::RBF:
                    // Simple radial basis function approximation
                    double dx = nx - 0.5;
                    double dy = ny - 0.5;
                    double r = std::sqrt(dx*dx + dy*dy);
                    backgroundValue = 0.05f + 0.02f * std::exp(-r*r*4);
                    break;
                }
                
                // Ensure reasonable background values
                backgroundValue = std::max(0.0f, std::min(0.2f, backgroundValue));
                m_result.backgroundData[index] = backgroundValue;
            }
        }
    }
    
    return true;
}

void BackgroundExtractionWorker::rejectOutliers()
{
    if (m_sampleValues.isEmpty()) return;
    
    for (int iteration = 0; iteration < m_settings.rejectionIterations; ++iteration) {
        if (m_cancelled) return;
        
        // Calculate statistics of valid samples
        QVector<float> validValues;
        for (int i = 0; i < m_sampleValues.size(); ++i) {
            if (m_sampleValid[i]) {
                validValues.append(m_sampleValues[i]);
            }
        }
        
        if (validValues.size() < 10) break; // Need minimum samples
        
        // Calculate median and MAD (Median Absolute Deviation)
        std::sort(validValues.begin(), validValues.end());
        float median = validValues[validValues.size() / 2];
        
        QVector<float> deviations;
        for (float value : validValues) {
            deviations.append(std::abs(value - median));
        }
        std::sort(deviations.begin(), deviations.end());
        float mad = deviations[deviations.size() / 2];
        
        // Use MAD-based sigma estimation
        float sigma = 1.4826f * mad; // Convert MAD to approximate standard deviation
        
        if (sigma < 1e-6f) break; // Avoid division by very small numbers
        
        // Reject outliers
        int rejectedThisIteration = 0;
        for (int i = 0; i < m_sampleValues.size(); ++i) {
            if (m_sampleValid[i]) {
                float deviation = std::abs(m_sampleValues[i] - median) / sigma;
                
                if (deviation > m_settings.rejectionHigh || 
                    (m_sampleValues[i] < median && deviation > m_settings.rejectionLow)) {
                    m_sampleValid[i] = false;
                    rejectedThisIteration++;
                }
            }
        }
        
        if (rejectedThisIteration == 0) break; // No more outliers found
    }
    
    // Count final rejections
    m_result.samplesRejected = 0;
    for (int i = 0; i < m_sampleValid.size(); ++i) {
        if (!m_sampleValid[i]) {
            m_result.samplesRejected++;
            if (i < m_result.sampleRejected.size()) {
                m_result.sampleRejected[i] = true;
            }
        }
    }
    
    m_result.samplesUsed = m_samples.size() - m_result.samplesRejected;
}