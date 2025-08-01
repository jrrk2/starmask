// BackgroundExtractor.cpp - Enhanced implementation with per-channel support
#include "BackgroundExtractor.h"

#include "PCLMockAPI.h"
#include <pcl/api/APIInterface.h>

#include <QDebug>
#include <QElapsedTimer>
#include <QMutex>
#include <QMutexLocker>
#include <QRandomGenerator>
#include <QtMath>
#include <QApplication>

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>
#include <Eigen/Dense>

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
    
    // Manual samples (now per-channel)
    QVector<QVector<QPoint>> channelManualSamples;
    QVector<QVector<float>> channelManualSampleValues;
    
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
    
    void ensureChannelCapacity(int channels) {
        while (channelManualSamples.size() < channels) {
            channelManualSamples.append(QVector<QPoint>());
            channelManualSampleValues.append(QVector<float>());
        }
    }
};

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
    defaults.channelMode = ChannelMode::PerChannel;
    defaults.useChannelCorrelation = true;
    defaults.shareGoodSamples = true;
    return defaults;
}

BackgroundExtractionSettings BackgroundExtractor::getPerChannelSettings()
{
    BackgroundExtractionSettings settings;
    settings.channelMode = ChannelMode::PerChannel;
    settings.model = BackgroundModel::Polynomial2;
    settings.tolerance = 1.0;
    settings.deviation = 2.5;
    settings.minSamples = 100;
    settings.maxSamples = 1000;
    settings.useOutlierRejection = true;
    settings.rejectionLow = 2.0;
    settings.rejectionHigh = 2.5;
    settings.rejectionIterations = 3;
    settings.useChannelCorrelation = true;
    settings.shareGoodSamples = true;
    settings.correlationThreshold = 0.7;
    return settings;
}

BackgroundExtractionSettings BackgroundExtractor::getAstronomyRGBSettings()
{
    BackgroundExtractionSettings settings = getPerChannelSettings();
    
    // Astronomy-specific optimizations
    settings.model = BackgroundModel::Polynomial3; // Handle complex gradients
    settings.sampleGeneration = SampleGeneration::Automatic;
    settings.tolerance = 0.8;  // More sensitive to gradients
    settings.deviation = 2.0;  // Stricter outlier detection
    settings.minSamples = 200; // More samples for accuracy
    settings.maxSamples = 2000;
    
    // Per-channel settings optimized for RGB astrophotography
    settings.usePerChannelSettings = true;
    settings.channelTolerances = {0.8, 1.0, 1.2};     // R, G, B - red often has more gradient
    settings.channelDeviations = {2.0, 2.2, 2.5};     // Blue typically noisier
    settings.channelMinSamples = {150, 200, 250};      // More samples for noisier channels
    settings.channelMaxSamples = {1500, 2000, 2500};
    
    // RGB weighting for luminance calculation (if needed)
    settings.channelWeights = {0.299, 0.587, 0.114}; // Standard RGB to luminance weights
    
    settings.useChannelCorrelation = true;
    settings.shareGoodSamples = false; // Each channel has different characteristics
    settings.correlationThreshold = 0.5; // Lower threshold for astronomical images
    
    return settings;
}

BackgroundExtractionSettings BackgroundExtractor::getLuminanceOnlySettings()
{
    BackgroundExtractionSettings settings;
    settings.channelMode = ChannelMode::LuminanceOnly;
    settings.model = BackgroundModel::Polynomial2;
    settings.tolerance = 1.0;
    settings.deviation = 3.0;
    settings.minSamples = 300;  // More samples since we're processing luminance
    settings.maxSamples = 1500;
    settings.useOutlierRejection = true;
    settings.rejectionLow = 2.5;
    settings.rejectionHigh = 3.0;
    settings.rejectionIterations = 4;
    
    // Standard luminance weights
    settings.channelWeights = {0.299, 0.587, 0.114};
    
    return settings;
}

BackgroundExtractionSettings BackgroundExtractor::getConservativeSettings()
{
    BackgroundExtractionSettings settings = getPerChannelSettings();
    settings.model = BackgroundModel::Linear;
    settings.tolerance = 1.5;
    settings.deviation = 3.5;
    settings.rejectionLow = 3.0;
    settings.rejectionHigh = 3.5;
    settings.shareGoodSamples = true; // More conservative approach
    return settings;
}

BackgroundExtractionSettings BackgroundExtractor::getAggressiveSettings()
{
    BackgroundExtractionSettings settings = getPerChannelSettings();
    settings.model = BackgroundModel::Polynomial3;
    settings.tolerance = 0.6;
    settings.deviation = 1.8;
    settings.rejectionLow = 1.5;
    settings.rejectionHigh = 2.0;
    settings.minSamples = 300;
    settings.maxSamples = 3000;
    settings.shareGoodSamples = false; // Let each channel be independent
    return settings;
}

bool BackgroundExtractor::extractBackgroundAsync(const ImageData& imageData)
{
    QMutexLocker locker(&d->mutex);
    
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
    
    // Ensure we have enough manual sample containers for all channels
    d->ensureChannelCapacity(imageData.channels);
    
    d->worker = new BackgroundExtractionWorker(imageData, d->settings, this);
    
    connect(d->worker, &BackgroundExtractionWorker::finished,
            this, &BackgroundExtractor::onWorkerFinished);
    connect(d->worker, &BackgroundExtractionWorker::progress,
            this, &BackgroundExtractor::extractionProgress);
    connect(d->worker, &BackgroundExtractionWorker::progress,
            this, &BackgroundExtractor::progress);
    connect(d->worker, &BackgroundExtractionWorker::channelProgress,
            this, &BackgroundExtractor::channelProgress);
    connect(d->worker, &BackgroundExtractionWorker::channelCompleted,
            this, &BackgroundExtractor::channelCompleted);
    connect(d->worker, &QThread::finished,
            d->worker, &QObject::deleteLater);
    
    d->extracting = true;
    emit extractionStarted();
    d->worker->start();
    return true;
}

void BackgroundExtractor::onWorkerFinished(const BackgroundExtractionResult& result)
{
    QMutexLocker locker(&d->mutex);
    d->result = result;
    d->worker = nullptr;
    d->extracting = false;
    locker.unlock();
    
    emit extractionCompleted(result);
    emit extractionFinished(result.success, result.errorMessage);
    
    if (result.success) {
        emit previewReady(result);
    }
}

// Per-channel manual sample management
void BackgroundExtractor::addManualSampleForChannel(int channel, const QPoint& point, float value)
{
    QMutexLocker locker(&d->mutex);
    d->ensureChannelCapacity(channel + 1);
    
    if (channel >= 0 && channel < d->channelManualSamples.size()) {
        d->channelManualSamples[channel].append(point);
        d->channelManualSampleValues[channel].append(value);
    }
}

void BackgroundExtractor::clearManualSamplesForChannel(int channel)
{
    QMutexLocker locker(&d->mutex);
    if (channel >= 0 && channel < d->channelManualSamples.size()) {
        d->channelManualSamples[channel].clear();
        d->channelManualSampleValues[channel].clear();
    }
}

QVector<QPoint> BackgroundExtractor::getManualSamplesForChannel(int channel) const
{
    QMutexLocker locker(&d->mutex);
    if (channel >= 0 && channel < d->channelManualSamples.size()) {
        return d->channelManualSamples[channel];
    }
    return QVector<QPoint>();
}

ChannelResult BackgroundExtractor::getChannelResult(int channel) const
{
    QMutexLocker locker(&d->mutex);
    return d->result.getChannelResult(channel);
}

QVector<float> BackgroundExtractor::getChannelBackgroundData(int channel, int width, int height) const
{
    QMutexLocker locker(&d->mutex);
    return d->result.getChannelBackgroundData(channel, width, height);
}

QVector<float> BackgroundExtractor::getChannelCorrectedData(int channel, int width, int height) const
{
    QMutexLocker locker(&d->mutex);
    if (channel >= 0 && channel < d->result.channelResults.size()) {
        return d->result.channelResults[channel].correctedData;
    }
    return QVector<float>();
}

// Channel analysis utilities
QVector<double> BackgroundExtractor::analyzeChannelCorrelations(const ImageData& imageData) const
{
    QVector<double> correlations;
    
    if (imageData.channels < 2) {
        return correlations;
    }
    
    int pixelsPerChannel = imageData.width * imageData.height;
    
    // Calculate correlations between consecutive channels
    for (int ch = 0; ch < imageData.channels - 1; ++ch) {
        double sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0, sumY2 = 0;
        int count = 0;
        
        // Sample every 10th pixel for efficiency
        for (int i = 0; i < pixelsPerChannel; i += 10) {
            int idx1 = i + ch * pixelsPerChannel;
            int idx2 = i + (ch + 1) * pixelsPerChannel;
            
            if (idx1 < imageData.pixels.size() && idx2 < imageData.pixels.size()) {
                double x = imageData.pixels[idx1];
                double y = imageData.pixels[idx2];
                
                sumX += x;
                sumY += y;
                sumXY += x * y;
                sumX2 += x * x;
                sumY2 += y * y;
                count++;
            }
        }
        
        if (count > 10) {
            double meanX = sumX / count;
            double meanY = sumY / count;
            double covariance = (sumXY / count) - (meanX * meanY);
            double stdX = std::sqrt((sumX2 / count) - (meanX * meanX));
            double stdY = std::sqrt((sumY2 / count) - (meanY * meanY));
            
            if (stdX > 0 && stdY > 0) {
                double correlation = covariance / (stdX * stdY);
                correlations.append(correlation);
            } else {
                correlations.append(0.0);
            }
        } else {
            correlations.append(0.0);
        }
    }
    
    return correlations;
}

QVector<double> BackgroundExtractor::estimateChannelBackgroundLevels(const ImageData& imageData) const
{
    QVector<double> levels;
    
    int pixelsPerChannel = imageData.width * imageData.height;
    
    for (int ch = 0; ch < imageData.channels; ++ch) {
        QVector<float> channelData;
        
        // Extract channel data
        for (int i = 0; i < pixelsPerChannel; ++i) {
            int idx = i + ch * pixelsPerChannel;
            if (idx < imageData.pixels.size()) {
                channelData.append(imageData.pixels[idx]);
            }
        }
        
        if (!channelData.isEmpty()) {
            // Calculate median as background estimate
            std::sort(channelData.begin(), channelData.end());
            double median = channelData[channelData.size() / 2];
            levels.append(median);
        } else {
            levels.append(0.0);
        }
    }
    
    return levels;
}

QString BackgroundExtractor::getChannelAnalysisReport(const ImageData& imageData) const
{
    QString report = "Channel Analysis Report:\n";
    
    QVector<double> correlations = analyzeChannelCorrelations(imageData);
    QVector<double> backgroundLevels = estimateChannelBackgroundLevels(imageData);
    
    report += QString("Image: %1×%2×%3\n\n")
              .arg(imageData.width)
              .arg(imageData.height)
              .arg(imageData.channels);
    
    // Background levels
    report += "Estimated Background Levels:\n";
    for (int ch = 0; ch < backgroundLevels.size(); ++ch) {
        report += QString("  Channel %1: %2\n")
                  .arg(ch)
                  .arg(backgroundLevels[ch], 0, 'f', 6);
    }
    report += "\n";
    
    // Channel correlations
    if (!correlations.isEmpty()) {
        report += "Channel Correlations:\n";
        for (int ch = 0; ch < correlations.size(); ++ch) {
            report += QString("  Ch%1-Ch%2: %3\n")
                      .arg(ch)
                      .arg(ch + 1)
                      .arg(correlations[ch], 0, 'f', 3);
        }
        report += "\n";
    }
    
    // Recommendations
    report += "Recommendations:\n";
    if (imageData.channels >= 3) {
        double avgCorrelation = 0;
        for (double corr : correlations) {
            avgCorrelation += std::abs(corr);
        }
        avgCorrelation /= correlations.size();
        
        if (avgCorrelation > 0.8) {
            report += "  - High channel correlation detected\n";
            report += "  - Consider using combined processing or luminance-only mode\n";
        } else if (avgCorrelation < 0.3) {
            report += "  - Low channel correlation detected\n";
            report += "  - Per-channel processing recommended\n";
            report += "  - Disable sample sharing between channels\n";
        } else {
            report += "  - Moderate channel correlation\n";
            report += "  - Per-channel processing with sample sharing recommended\n";
        }
    }
    
    // Check for significant background differences
    if (backgroundLevels.size() >= 3) {
        double minLevel = *std::min_element(backgroundLevels.begin(), backgroundLevels.end());
        double maxLevel = *std::max_element(backgroundLevels.begin(), backgroundLevels.end());
        
        if (maxLevel > 0 && (maxLevel - minLevel) / maxLevel > 0.5) {
            report += "  - Significant background level differences detected\n";
            report += "  - This may indicate color cast or channel-specific gradients\n";
            report += "  - Per-channel processing strongly recommended\n";
        }
    }
    
    return report;
}

// Legacy compatibility methods
bool BackgroundExtractor::extractBackground(const ImageData& imageData)
{
    return extractBackgroundAsync(imageData);
}

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
        
        for (int c = 0; c < previewData.channels; ++c) {
            for (int y = 0; y < previewData.height; ++y) {
                for (int x = 0; x < previewData.width; ++x) {
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
    return d->extracting;
}

BackgroundExtractionResult BackgroundExtractor::result() const
{
    QMutexLocker locker(&d->mutex);
    return d->result;
}

bool BackgroundExtractor::hasResult() const
{
    QMutexLocker locker(&d->mutex);
    return d->result.success;
}

void BackgroundExtractor::clearResult()
{
    QMutexLocker locker(&d->mutex);
    d->result = BackgroundExtractionResult();
}

// Legacy manual sample methods (use first channel)
void BackgroundExtractor::addManualSample(const QPoint& point, float value)
{
    addManualSampleForChannel(0, point, value);
}

void BackgroundExtractor::clearManualSamples()
{
    QMutexLocker locker(&d->mutex);
    for (auto& samples : d->channelManualSamples) {
        samples.clear();
    }
    for (auto& values : d->channelManualSampleValues) {
        values.clear();
    }
}

QVector<QPoint> BackgroundExtractor::manualSamples() const
{
    return getManualSamplesForChannel(0);
}

QVector<QPoint> BackgroundExtractor::getManualSamples() const
{
    return getManualSamplesForChannel(0);
}

// BackgroundExtractionWorker Implementation

BackgroundExtractionWorker::BackgroundExtractionWorker(const ImageData& imageData,
                                                     const BackgroundExtractionSettings& settings,
                                                     QObject* parent)
    : QThread(parent)
    , m_imageData(imageData)
    , m_settings(settings)
    , m_cancelled(false)
{
    // Initialize per-channel containers
    m_channelSamples.resize(imageData.channels);
    m_channelSampleValues.resize(imageData.channels);
    m_channelSampleValid.resize(imageData.channels);
}

void BackgroundExtractionWorker::cancel()
{
    m_cancelled = true;
}

void BackgroundExtractionWorker::run()
{
    QElapsedTimer timer;
    timer.start();
    
    try {
        emit progress(0, "Starting background extraction...");
        
        if (m_cancelled) return;
        
        // Initialize result structure
        m_result.channelResults.resize(m_imageData.channels);
        m_result.usedChannelMode = m_settings.channelMode;
        
        bool success = false;
        
        switch (m_settings.channelMode) {
        case ChannelMode::PerChannel:
            success = performPerChannelExtraction();
            break;
        case ChannelMode::Combined:
            success = performCombinedExtraction();
            break;
        case ChannelMode::LuminanceOnly:
            success = performLuminanceOnlyExtraction();
            break;
        default:
            success = performPerChannelExtraction();
            break;
        }
        
        if (m_cancelled) return;
        
        if (success) {
            combineChannelResults();
            m_result.success = true;
            m_result.processingTimeSeconds = timer.elapsed() / 1000.0;
            emit progress(100, "Background extraction completed");
        } else {
            m_result.success = false;
            m_result.errorMessage = "Background extraction failed";
        }
        
        emit finished(m_result);
        
    } catch (const std::exception& e) {
        m_result.success = false;
        m_result.errorMessage = QString("Exception during background extraction: %1").arg(e.what());
        emit finished(m_result);
    }
}

bool BackgroundExtractionWorker::performPerChannelExtraction()
{
    emit progress(10, "Starting per-channel background extraction");
    
    // Analyze channel correlations if enabled
    QVector<double> correlations;
    if (m_settings.useChannelCorrelation) {
        correlations = calculateChannelCorrelations();
        m_result.channelCorrelations = correlations;
    }
    
    bool allChannelsSuccessful = true;
    
    // Process each channel
    for (int ch = 0; ch < m_imageData.channels; ++ch) {
        if (m_cancelled) return false;
        
        emit channelProgress(ch, 0, QString("Processing channel %1").arg(ch));
        
        ChannelResult& channelResult = m_result.channelResults[ch];
        channelResult.channel = ch;
        
        // Extract background for this channel
        bool channelSuccess = extractChannelBackground(ch, channelResult);
        
        if (channelSuccess) {
            emit channelCompleted(ch, channelResult);
            emit channelProgress(ch, 100, QString("Channel %1 completed").arg(ch));
        } else {
            allChannelsSuccessful = false;
            channelResult.success = false;
            channelResult.errorMessage = QString("Failed to extract background for channel %1").arg(ch);
            emit channelProgress(ch, 100, QString("Channel %1 failed").arg(ch));
        }
        
        // Update overall progress
        int overallProgress = 10 + (80 * (ch + 1)) / m_imageData.channels;
        emit progress(overallProgress, QString("Processed %1/%2 channels").arg(ch + 1).arg(m_imageData.channels));
    }
    
    // Share good samples between channels if enabled
    if (m_settings.shareGoodSamples && m_settings.useChannelCorrelation) {
        emit progress(90, "Sharing samples between correlated channels");
        shareGoodSamplesBetweenChannels();
        
        // Apply cross-channel validation
        applyCrossChannelValidation();
    }
    
    return allChannelsSuccessful;
}

bool BackgroundExtractionWorker::extractChannelBackground(int channel, ChannelResult& result)
{
    QElapsedTimer channelTimer;
    channelTimer.start();
    
    // Extract channel data
    QVector<float> channelData = extractChannelData(channel);
    if (channelData.isEmpty()) {
        result.errorMessage = "Empty channel data";
        return false;
    }
    
    // Calculate channel statistics
    std::sort(channelData.begin(), channelData.end());
    result.channelMin = channelData.first();
    result.channelMax = channelData.last();
    result.channelMean = std::accumulate(channelData.begin(), channelData.end(), 0.0) / channelData.size();
    
    double variance = 0.0;
    for (float val : channelData) {
        variance += (val - result.channelMean) * (val - result.channelMean);
    }
    result.channelStdDev = std::sqrt(variance / channelData.size());
    result.backgroundLevel = channelData[channelData.size() / 10]; // 10th percentile as background estimate
    
    // Generate samples for this channel
    QVector<QPoint> samples;
    QVector<float> sampleValues;
    
    emit channelProgress(channel, 20, QString("Generating samples for channel %1").arg(channel));
    
    if (!generateChannelSpecificSamples(channel, samples, sampleValues)) {
        result.errorMessage = "Failed to generate sufficient samples";
        return false;
    }
    
    emit channelProgress(channel, 50, QString("Fitting model for channel %1").arg(channel));
    
    // Fit model for this channel
    if (!fitChannelPolynomialModel(channel, samples, sampleValues, result)) {
        result.errorMessage = "Failed to fit background model";
        return false;
    }
    
    emit channelProgress(channel, 80, QString("Applying correction for channel %1").arg(channel));
    
    // Apply correction
    result.correctedData.resize(m_imageData.width * m_imageData.height);
    for (int i = 0; i < result.correctedData.size(); ++i) {
        int channelIndex = i + channel * m_imageData.width * m_imageData.height;
        if (channelIndex < m_imageData.pixels.size() && i < result.backgroundData.size()) {
            result.correctedData[i] = m_imageData.pixels[channelIndex] - result.backgroundData[i];
        }
    }
    
    result.success = true;
    result.processingTimeSeconds = channelTimer.elapsed() / 1000.0;
    result.samplesUsed = samples.size();
    result.samplePoints = samples;
    result.sampleValues = sampleValues;
    
    // Calculate gradient strength (measure of background complexity)
    double gradientSum = 0.0;
    int gradientCount = 0;
    for (int y = 1; y < m_imageData.height - 1; ++y) {
        for (int x = 1; x < m_imageData.width - 1; ++x) {
            if (gradientCount % 100 == 0) { // Sample every 100th pixel
                int idx = y * m_imageData.width + x;
                if (idx < result.backgroundData.size()) {
                    double dx = result.backgroundData[idx + 1] - result.backgroundData[idx - 1];
                    double dy = result.backgroundData[idx + m_imageData.width] - result.backgroundData[idx - m_imageData.width];
                    gradientSum += std::sqrt(dx * dx + dy * dy);
                    gradientCount++;
                }
            }
        }
    }
    result.gradientStrength = gradientCount > 0 ? gradientSum / gradientCount : 0.0;
    
    return true;
}

QVector<float> BackgroundExtractionWorker::extractChannelData(int channel) const
{
    QVector<float> channelData;
    int pixelsPerChannel = m_imageData.width * m_imageData.height;
    
    channelData.reserve(pixelsPerChannel);
    
    for (int i = 0; i < pixelsPerChannel; ++i) {
        int index = i + channel * pixelsPerChannel;
        if (index < m_imageData.pixels.size()) {
            channelData.append(m_imageData.pixels[index]);
        }
    }
    
    return channelData;
}

bool BackgroundExtractionWorker::generateChannelSpecificSamples(int channel, QVector<QPoint>& samples, QVector<float>& values)
{
    // Get channel-specific settings
    double tolerance = m_settings.tolerance;
    double deviation = m_settings.deviation;
    int minSamples = m_settings.minSamples;
    int maxSamples = m_settings.maxSamples;
    
    if (m_settings.usePerChannelSettings && channel < m_settings.channelTolerances.size()) {
        tolerance = m_settings.channelTolerances[channel];
        deviation = m_settings.channelDeviations[channel];
        minSamples = m_settings.channelMinSamples[channel];
        maxSamples = m_settings.channelMaxSamples[channel];
    }
    
    // Extract channel data for analysis
    QVector<float> channelData = extractChannelData(channel);
    if (channelData.isEmpty()) return false;
    
    // Calculate channel statistics
    std::sort(channelData.begin(), channelData.end());
    double median = channelData[channelData.size() / 2];
    double q25 = channelData[channelData.size() / 4];
    double q75 = channelData[3 * channelData.size() / 4];
    double iqr = q75 - q25;
    
    // Adaptive threshold based on channel characteristics
    double backgroundThreshold = median + deviation * iqr / 1.349; // Convert IQR to std estimate
    
    samples.clear();
    values.clear();
    
    if (m_settings.sampleGeneration == SampleGeneration::Grid) {
        // Grid sampling with channel-specific thresholds
        int stepX = m_imageData.width / (m_settings.gridColumns + 1);
        int stepY = m_imageData.height / (m_settings.gridRows + 1);
        
        for (int row = 1; row <= m_settings.gridRows; ++row) {
            for (int col = 1; col <= m_settings.gridColumns; ++col) {
                if (m_cancelled) return false;
                
                int x = col * stepX;
                int y = row * stepY;
                
                if (x < m_imageData.width && y < m_imageData.height) {
                    int pixelIndex = y * m_imageData.width + x;
                    int channelIndex = pixelIndex + channel * m_imageData.width * m_imageData.height;
                    
                    if (channelIndex < m_imageData.pixels.size()) {
                        float pixelValue = m_imageData.pixels[channelIndex];
                        
                        // Only include pixels that look like background for this channel
                        if (pixelValue <= backgroundThreshold) {
                            samples.append(QPoint(x, y));
                            values.append(pixelValue);
                        }
                    }
                }
            }
        }
    } else {
        // Automatic sampling with channel-specific analysis
        int blockSize = std::max(8, std::min(m_imageData.width, m_imageData.height) / 32);
        
        for (int y = blockSize; y < m_imageData.height - blockSize; y += blockSize/2) {
            for (int x = blockSize; x < m_imageData.width - blockSize; x += blockSize/2) {
                if (m_cancelled) return false;
                
                // Analyze local neighborhood for this channel
                double localSum = 0.0;
                double localSumSq = 0.0;
                int localCount = 0;
                
                for (int dy = -blockSize/2; dy <= blockSize/2; ++dy) {
                    for (int dx = -blockSize/2; dx <= blockSize/2; ++dx) {
                        int px = x + dx;
                        int py = y + dy;
                        
                        if (px >= 0 && px < m_imageData.width && py >= 0 && py < m_imageData.height) {
                            int pixelIndex = py * m_imageData.width + px;
                            int channelIndex = pixelIndex + channel * m_imageData.width * m_imageData.height;
                            
                            if (channelIndex < m_imageData.pixels.size()) {
                                double val = m_imageData.pixels[channelIndex];
                                localSum += val;
                                localSumSq += val * val;
                                localCount++;
                            }
                        }
                    }
                }
                
                if (localCount > 10) {
                    double localMean = localSum / localCount;
                    double localVariance = (localSumSq / localCount) - (localMean * localMean);
                    double localStdDev = std::sqrt(localVariance);
                    
                    // Check if this region is suitable as background
                    if (localStdDev < tolerance && localMean <= backgroundThreshold) {
                        int centerIndex = y * m_imageData.width + x;
                        int channelIndex = centerIndex + channel * m_imageData.width * m_imageData.height;
                        
                        if (channelIndex < m_imageData.pixels.size()) {
                            samples.append(QPoint(x, y));
                            values.append(m_imageData.pixels[channelIndex]);
                        }
                    }
                }
                
                if (samples.size() >= maxSamples) {
                    break;
                }
            }
            if (samples.size() >= maxSamples) {
                break;
            }
        }
    }
    
    // Check if we have enough samples
    if (samples.size() < minSamples) {
        qDebug() << "Channel" << channel << "insufficient samples:" << samples.size() << "minimum:" << minSamples;
        return false;
    }
    
    qDebug() << "Channel" << channel << "generated" << samples.size() << "samples";
    return true;
}

bool BackgroundExtractionWorker::fitChannelPolynomialModel(int channel, const QVector<QPoint>& samples, 
                                                          const QVector<float>& values, ChannelResult& result)
{
    if (samples.isEmpty() || samples.size() != values.size()) {
        return false;
    }
    
    // Perform outlier rejection if enabled
    QVector<QPoint> validSamples = samples;
    QVector<float> validValues = values;
    QVector<bool> validFlags(samples.size(), true);
    
    if (m_settings.useOutlierRejection) {
        rejectChannelOutliers(channel, validSamples, validValues);
        
        // Update valid flags
        validFlags.fill(false);
        for (int i = 0; i < validSamples.size(); ++i) {
            for (int j = 0; j < samples.size(); ++j) {
                if (validSamples[i] == samples[j] && std::abs(validValues[i] - values[j]) < 1e-6) {
                    validFlags[j] = true;
                    break;
                }
            }
        }
    }
    
    result.sampleRejected.resize(samples.size());
    for (int i = 0; i < validFlags.size(); ++i) {
        result.sampleRejected[i] = !validFlags[i];
    }
    
    // Determine model order
    BackgroundModel model = m_settings.model;
    int order = 2; // Default to polynomial2
    
    switch (model) {
    case BackgroundModel::Linear: order = 1; break;
    case BackgroundModel::Polynomial2: order = 2; break;
    case BackgroundModel::Polynomial3: order = 3; break;
    default: order = 2; break;
    }
    
    int numTerms = getNumTerms(order);
    
    if (validSamples.size() < numTerms) {
        result.errorMessage = QString("Too few samples (%1) for model order %2 (need %3)")
                              .arg(validSamples.size()).arg(order).arg(numTerms);
        return false;
    }
    
    // Build coefficient matrix and solve
    std::vector<std::vector<double>> A(validSamples.size(), std::vector<double>(numTerms));
    std::vector<double> b(validSamples.size());
    
    for (int i = 0; i < validSamples.size(); ++i) {
        double x = double(validSamples[i].x()) / m_imageData.width;   // Normalize to [0,1]
        double y = double(validSamples[i].y()) / m_imageData.height;  // Normalize to [0,1]
        
        int termIndex = 0;
        for (int py = 0; py <= order; ++py) {
            for (int px = 0; px <= order - py; ++px) {
                A[i][termIndex] = std::pow(x, px) * std::pow(y, py);
                termIndex++;
            }
        }
        
        b[i] = validValues[i];
    }
    
    // Solve linear system
    std::vector<double> coefficients;
    if (!solveLinearSystem(A, b, coefficients)) {
        result.errorMessage = "Failed to solve linear system";
        return false;
    }
    
    // Generate background surface
    result.backgroundData.resize(m_imageData.width * m_imageData.height);
    
    for (int y = 0; y < m_imageData.height; ++y) {
        for (int x = 0; x < m_imageData.width; ++x) {
            double nx = double(x) / m_imageData.width;
            double ny = double(y) / m_imageData.height;
            
            double value = evaluatePolynomial(coefficients, nx, ny, order);
            result.backgroundData[y * m_imageData.width + x] = float(value);
        }
    }
    
    // Calculate error metrics
    double errorSum = 0.0;
    double maxError = 0.0;
    int errorCount = 0;
    
    for (int i = 0; i < validSamples.size(); ++i) {
        const QPoint& point = validSamples[i];
        int index = point.y() * m_imageData.width + point.x();
        
        if (index < result.backgroundData.size()) {
            double error = std::abs(result.backgroundData[index] - validValues[i]);
            errorSum += error * error;
            maxError = std::max(maxError, error);
            errorCount++;
        }
    }
    
    if (errorCount > 0) {
        result.rmsError = std::sqrt(errorSum / errorCount);
        result.meanDeviation = std::sqrt(errorSum) / errorCount;
        result.maxDeviation = maxError;
    }
    
    return true;
}

void BackgroundExtractionWorker::rejectChannelOutliers(int channel, QVector<QPoint>& samples, QVector<float>& values)
{
    if (samples.size() != values.size() || samples.size() < 10) {
        return;
    }
    
    // Get channel-specific rejection parameters
    double rejectionLow = m_settings.rejectionLow;
    double rejectionHigh = m_settings.rejectionHigh;
    
    for (int iter = 0; iter < m_settings.rejectionIterations; ++iter) {
        if (samples.size() < 10) break;
        
        // Calculate robust statistics
        QVector<float> sortedValues = values;
        std::sort(sortedValues.begin(), sortedValues.end());
        
        float median = sortedValues[sortedValues.size() / 2];
        
        QVector<float> deviations;
        for (float val : sortedValues) {
            deviations.append(std::abs(val - median));
        }
        std::sort(deviations.begin(), deviations.end());
        float mad = deviations[deviations.size() / 2];
        
        // Reject outliers
        float lowerThreshold = median - rejectionLow * mad * 1.4826f;
        float upperThreshold = median + rejectionHigh * mad * 1.4826f;
        
        QVector<QPoint> newSamples;
        QVector<float> newValues;
        
        for (int i = 0; i < samples.size(); ++i) {
            if (values[i] >= lowerThreshold && values[i] <= upperThreshold) {
                newSamples.append(samples[i]);
                newValues.append(values[i]);
            }
        }
        
        if (newSamples.size() == samples.size()) {
            break; // No more outliers found
        }
        
        samples = newSamples;
        values = newValues;
    }
}

QVector<double> BackgroundExtractionWorker::calculateChannelCorrelations()
{
    QVector<double> correlations;
    
    if (m_imageData.channels < 2) {
        return correlations;
    }
    
    int pixelsPerChannel = m_imageData.width * m_imageData.height;
    
    // Calculate correlations between consecutive channels
    for (int ch = 0; ch < m_imageData.channels - 1; ++ch) {
        double sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0, sumY2 = 0;
        int count = 0;
        
        // Sample every 10th pixel for efficiency
        for (int i = 0; i < pixelsPerChannel; i += 10) {
            int idx1 = i + ch * pixelsPerChannel;
            int idx2 = i + (ch + 1) * pixelsPerChannel;
            
            if (idx1 < m_imageData.pixels.size() && idx2 < m_imageData.pixels.size()) {
                double x = m_imageData.pixels[idx1];
                double y = m_imageData.pixels[idx2];
                
                sumX += x;
                sumY += y;
                sumXY += x * y;
                sumX2 += x * x;
                sumY2 += y * y;
                count++;
            }
        }
        
        if (count > 10) {
            double meanX = sumX / count;
            double meanY = sumY / count;
            double covariance = (sumXY / count) - (meanX * meanY);
            double stdX = std::sqrt((sumX2 / count) - (meanX * meanX));
            double stdY = std::sqrt((sumY2 / count) - (meanY * meanY));
            
            if (stdX > 0 && stdY > 0) {
                double correlation = covariance / (stdX * stdY);
                correlations.append(correlation);
            } else {
                correlations.append(0.0);
            }
        } else {
            correlations.append(0.0);
        }
    }
    
    return correlations;
}

void BackgroundExtractionWorker::shareGoodSamplesBetweenChannels()
{
    // This is a simplified implementation
    // In practice, you'd want more sophisticated correlation analysis
    
    if (m_result.channelCorrelations.isEmpty()) {
        return;
    }
    
    // Find channels with high correlation
    for (int i = 0; i < m_result.channelCorrelations.size(); ++i) {
        if (std::abs(m_result.channelCorrelations[i]) > m_settings.correlationThreshold) {
            // Channels i and i+1 are highly correlated
            // Share samples with low error between them
            // Implementation would go here...
        }
    }
}

void BackgroundExtractionWorker::applyCrossChannelValidation()
{
    // Cross-validate results between channels
    // Flag suspicious results, apply smoothing constraints between channels
    // Implementation would go here...
}

void BackgroundExtractionWorker::combineChannelResults()
{
    // Combine per-channel results into legacy format for compatibility
    m_result.backgroundData.clear();
    m_result.correctedData.clear();
    
    int pixelsPerChannel = m_imageData.width * m_imageData.height;
    int totalPixels = pixelsPerChannel * m_imageData.channels;
    
    m_result.backgroundData.resize(totalPixels);
    m_result.correctedData.resize(totalPixels);
    
    // Combine channel data
    for (int ch = 0; ch < m_result.channelResults.size(); ++ch) {
        const ChannelResult& channelResult = m_result.channelResults[ch];
        
        if (channelResult.success) {
            // Copy background data
            for (int i = 0; i < pixelsPerChannel && i < channelResult.backgroundData.size(); ++i) {
                int combinedIndex = i + ch * pixelsPerChannel;
                m_result.backgroundData[combinedIndex] = channelResult.backgroundData[i];
            }
            
            // Copy corrected data
            for (int i = 0; i < pixelsPerChannel && i < channelResult.correctedData.size(); ++i) {
                int combinedIndex = i + ch * pixelsPerChannel;
                m_result.correctedData[combinedIndex] = channelResult.correctedData[i];
            }
        }
    }
    
    // Calculate combined statistics
    double totalRmsError = 0.0;
    double totalMeanDeviation = 0.0;
    double totalMaxDeviation = 0.0;
    int totalSamples = 0;
    int successfulChannels = 0;
    
    for (const ChannelResult& channelResult : m_result.channelResults) {
        if (channelResult.success) {
            totalRmsError += channelResult.rmsError * channelResult.rmsError;
            totalMeanDeviation += channelResult.meanDeviation;
            totalMaxDeviation = std::max(totalMaxDeviation, channelResult.maxDeviation);
            totalSamples += channelResult.samplesUsed;
            successfulChannels++;
        }
    }
    
    if (successfulChannels > 0) {
        m_result.overallRmsError = std::sqrt(totalRmsError / successfulChannels);
        m_result.overallMeanDeviation = totalMeanDeviation / successfulChannels;
        m_result.overallMaxDeviation = totalMaxDeviation;
        m_result.totalSamplesUsed = totalSamples;
        
        // Legacy compatibility
        m_result.rmsError = m_result.overallRmsError;
        m_result.meanDeviation = m_result.overallMeanDeviation;
        m_result.maxDeviation = m_result.overallMaxDeviation;
        m_result.samplesUsed = totalSamples / successfulChannels; // Average samples per channel
        
        // Use samples from first successful channel for legacy compatibility
        for (const ChannelResult& channelResult : m_result.channelResults) {
            if (channelResult.success) {
                m_result.samplePoints = channelResult.samplePoints;
                m_result.sampleValues = channelResult.sampleValues;
                m_result.sampleRejected = channelResult.sampleRejected;
                break;
            }
        }
    }
}

bool BackgroundExtractionWorker::performCombinedExtraction()
{
    emit progress(10, "Starting combined channel extraction");
    
    // This method processes all channels together (original behavior)
    // Use the existing implementation but ensure channel results are populated
    
    m_channelSamples.clear();
    m_channelSampleValues.clear();
    m_channelSampleValid.clear();
    
    // Generate samples using combined approach
    if (!generateSamples()) {
        m_result.success = false;
        m_result.errorMessage = "Failed to generate sufficient background samples";
        return false;
    }
    
    if (m_cancelled) return false;
    
    // Fit model using combined approach
    if (!fitModel()) {
        m_result.success = false;
        m_result.errorMessage = "Failed to fit background model";
        return false;
    }
    
    if (m_cancelled) return false;
    
    // Apply correction
    if (m_settings.replaceTarget) {
        applyCorrection();
    }
    
    // Create dummy channel results for compatibility
    m_result.channelResults.resize(m_imageData.channels);
    int pixelsPerChannel = m_imageData.width * m_imageData.height;
    
    for (int ch = 0; ch < m_imageData.channels; ++ch) {
        ChannelResult& channelResult = m_result.channelResults[ch];
        channelResult.channel = ch;
        channelResult.success = true;
        channelResult.samplesUsed = m_samples.size();
        channelResult.samplePoints = m_samples;
        channelResult.sampleValues = m_sampleValues;
        channelResult.sampleRejected = m_sampleValid;
        
        // Extract channel-specific data from combined result
        channelResult.backgroundData.resize(pixelsPerChannel);
        channelResult.correctedData.resize(pixelsPerChannel);
        
        for (int i = 0; i < pixelsPerChannel; ++i) {
            int combinedIndex = i + ch * pixelsPerChannel;
            if (combinedIndex < m_result.backgroundData.size()) {
                channelResult.backgroundData[i] = m_result.backgroundData[combinedIndex];
            }
            if (combinedIndex < m_result.correctedData.size()) {
                channelResult.correctedData[i] = m_result.correctedData[combinedIndex];
            }
        }
        
        channelResult.rmsError = m_result.rmsError;
        channelResult.meanDeviation = m_result.meanDeviation;
        channelResult.maxDeviation = m_result.maxDeviation;
    }
    
    return true;
}

bool BackgroundExtractionWorker::performLuminanceOnlyExtraction()
{
    emit progress(10, "Starting luminance-only extraction");
    
    if (m_imageData.channels < 3) {
        // Not enough channels for RGB, fall back to combined processing
        return performCombinedExtraction();
    }
    
    // Calculate luminance from RGB
    QVector<float> luminanceData;
    calculateLuminance(luminanceData);
    
    // Create temporary single-channel image data
    ImageData luminanceImage;
    luminanceImage.width = m_imageData.width;
    luminanceImage.height = m_imageData.height;
    luminanceImage.channels = 1;
    luminanceImage.pixels = luminanceData;
    
    // Store original image data
    ImageData originalData = m_imageData;
    m_imageData = luminanceImage;
    
    // Process luminance channel
    m_channelSamples.clear();
    m_channelSampleValues.clear();
    m_channelSampleValid.clear();
    
    if (!generateSamples()) {
        m_result.success = false;
        m_result.errorMessage = "Failed to generate sufficient samples from luminance";
        m_imageData = originalData;
        return false;
    }
    
    if (m_cancelled) {
        m_imageData = originalData;
        return false;
    }
    
    if (!fitModel()) {
        m_result.success = false;
        m_result.errorMessage = "Failed to fit luminance background model";
        m_imageData = originalData;
        return false;
    }
    
    // Restore original image data
    m_imageData = originalData;
    
    // Apply luminance background correction to all channels
    m_result.channelResults.resize(m_imageData.channels);
    int pixelsPerChannel = m_imageData.width * m_imageData.height;
    
    // The luminance background is stored in m_result.backgroundData (single channel)
    // Apply this correction to all RGB channels
    for (int ch = 0; ch < m_imageData.channels; ++ch) {
        ChannelResult& channelResult = m_result.channelResults[ch];
        channelResult.channel = ch;
        channelResult.success = true;
        channelResult.samplesUsed = m_samples.size();
        channelResult.samplePoints = m_samples;
        channelResult.sampleValues = m_sampleValues;
        channelResult.sampleRejected = m_sampleValid;
        
        // Copy luminance background to each channel
        channelResult.backgroundData = m_result.backgroundData;
        
        // Apply correction to this channel
        channelResult.correctedData.resize(pixelsPerChannel);
        for (int i = 0; i < pixelsPerChannel; ++i) {
            int channelIndex = i + ch * pixelsPerChannel;
            if (channelIndex < m_imageData.pixels.size() && i < channelResult.backgroundData.size()) {
                channelResult.correctedData[i] = m_imageData.pixels[channelIndex] - channelResult.backgroundData[i];
            }
        }
        
        channelResult.rmsError = m_result.rmsError;
        channelResult.meanDeviation = m_result.meanDeviation;
        channelResult.maxDeviation = m_result.maxDeviation;
    }
    
    // Update combined results
    combineChannelResults();
    
    return true;
}

void BackgroundExtractionWorker::calculateLuminance(QVector<float>& luminance) const
{
    int pixelsPerChannel = m_imageData.width * m_imageData.height;
    luminance.resize(pixelsPerChannel);
    
    // Use channel weights if specified, otherwise use standard RGB weights
    QVector<double> weights = m_settings.channelWeights;
    if (weights.size() < 3) {
        weights = {0.299, 0.587, 0.114}; // Standard RGB to luminance conversion
    }
    
    for (int i = 0; i < pixelsPerChannel; ++i) {
        double lum = 0.0;
        
        for (int ch = 0; ch < std::min(m_imageData.channels, 3); ++ch) {
            int channelIndex = i + ch * pixelsPerChannel;
            if (channelIndex < m_imageData.pixels.size() && ch < weights.size()) {
                lum += m_imageData.pixels[channelIndex] * weights[ch];
            }
        }
        
        luminance[i] = static_cast<float>(lum);
    }
}

// Existing methods from original implementation
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
    
    // For multi-channel, use first channel or luminance
    QVector<float> analysisData;
    if (m_imageData.channels > 1 && m_settings.channelMode != ChannelMode::Combined) {
        // Use luminance for analysis
        calculateLuminance(analysisData);
    } else {
        // Use first channel
        int pixelsPerChannel = width * height;
        for (int i = 0; i < pixelsPerChannel; ++i) {
            analysisData.append(pixels[i]);
        }
    }
    
    // Analyze local variance to find uniform (background) regions
    QVector<float> localVariances;
    QVector<QPoint> candidatePoints;
    QVector<float> candidateValues;
    
    int blockSize = std::max(8, std::min(width, height) / 32);
    
    emit progress(30, "Computing local variance statistics...");
    
    for (int y = blockSize; y < height - blockSize; y += blockSize/2) {
        for (int x = blockSize; x < width - blockSize; x += blockSize/2) {
            
            if (m_cancelled) return false;
            
            // Calculate local statistics in this block using analysis data
            QVector<float> blockValues;
            float sum = 0.0f;
            
            for (int dy = -blockSize/2; dy <= blockSize/2; ++dy) {
                for (int dx = -blockSize/2; dx <= blockSize/2; ++dx) {
                    int px = x + dx;
                    int py = y + dy;
                    if (px >= 0 && px < width && py >= 0 && py < height) {
                        int index = py * width + px;
                        if (index < analysisData.size()) {
                            float val = analysisData[index];
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
    int minDistanceSquared = (blockSize * blockSize) / 4;
    
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
            
            // Get the actual pixel value from the original data (not analysis data)
            int pixelIndex = newPoint.y() * width + newPoint.x();
            if (pixelIndex < m_imageData.pixels.size()) {
                m_sampleValues.append(pixels[pixelIndex]);
            } else {
                m_sampleValues.append(candidateValues[idx]);
            }
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

bool BackgroundExtractionWorker::fitPolynomialModel()
{
    if (m_samples.isEmpty()) {
        return false;
    }
    
    int order = getModelOrder();
    int numTerms = getNumTerms(order);
    
    // Count valid samples
    int validCount = 0;
    for (bool valid : m_sampleValid) {
        if (valid) validCount++;
    }
    
    if (validCount < numTerms) {
        return false;
    }
    
    // Build coefficient matrix A and result vector b (only using valid samples)
    std::vector<std::vector<double>> A(validCount, std::vector<double>(numTerms));
    std::vector<double> b(validCount);
    
    int validIndex = 0;
    for (int i = 0; i < m_samples.size(); ++i) {
        if (!m_sampleValid[i]) continue;
        
        double x = double(m_samples[i].x()) / m_imageData.width;
        double y = double(m_samples[i].y()) / m_imageData.height;
        
        int termIndex = 0;
        for (int py = 0; py <= order; ++py) {
            for (int px = 0; px <= order - py; ++px) {
                A[validIndex][termIndex] = std::pow(x, px) * std::pow(y, py);
                termIndex++;
            }
        }
        
        b[validIndex] = m_sampleValues[i];
        validIndex++;
    }
    
    // Solve linear system
    std::vector<double> coefficients;
    if (!solveLinearSystem(A, b, coefficients)) {
        return false;
    }
    
    // Generate background surface for all channels
    int pixelsPerChannel = m_imageData.width * m_imageData.height;
    m_result.backgroundData.resize(pixelsPerChannel * m_imageData.channels);
    
    for (int ch = 0; ch < m_imageData.channels; ++ch) {
        for (int y = 0; y < m_imageData.height; ++y) {
            for (int x = 0; x < m_imageData.width; ++x) {
                double nx = double(x) / m_imageData.width;
                double ny = double(y) / m_imageData.height;
                
                double value = evaluatePolynomial(coefficients, nx, ny, order);
                int index = (y * m_imageData.width + x) + ch * pixelsPerChannel;
                m_result.backgroundData[index] = float(value);
            }
        }
    }
    
    calculateErrorMetrics();
    return true;
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
    
    int pixelsPerChannel = m_imageData.width * m_imageData.height;
    
    for (int i = 0; i < m_samples.size(); ++i) {
        if (m_sampleValid[i]) {
            const QPoint& point = m_samples[i];
            int x = point.x();
            int y = point.y();
            
            if (x >= 0 && x < m_imageData.width && y >= 0 && y < m_imageData.height) {
                int pixelIndex = y * m_imageData.width + x;
                
                // Use first channel for error calculation (for combined processing)
                if (pixelIndex < pixelsPerChannel && pixelIndex < m_result.backgroundData.size()) {
                    double originalValue = m_imageData.pixels[pixelIndex];
                    double backgroundValue = m_result.backgroundData[pixelIndex];
                    double error = std::abs(backgroundValue - originalValue);
                    
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
    int pixelsPerChannel = m_imageData.width * m_imageData.height;
    int totalPixels = pixelsPerChannel * m_imageData.channels;
    
    if (m_result.backgroundData.size() != totalPixels) {
        qDebug() << "Background data size mismatch:" << m_result.backgroundData.size() << "vs" << totalPixels;
        return;
    }
    
    m_result.correctedData.resize(totalPixels);
    
    for (int i = 0; i < totalPixels; ++i) {
        m_result.correctedData[i] = m_imageData.pixels[i] - m_result.backgroundData[i];
    }
}

// Utility methods
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

bool BackgroundExtractionWorker::solveLinearSystem(const std::vector<std::vector<double>>& A, 
                                                 const std::vector<double>& b, 
                                                 std::vector<double>& x)
{
    int m = A.size(), n = A[0].size();
    Eigen::MatrixXd M(m, n);
    Eigen::VectorXd B(m);

    for (int i = 0; i < m; ++i) {
        B(i) = b[i];
        for (int j = 0; j < n; ++j)
            M(i, j) = A[i][j];
    }

    Eigen::VectorXd sol = M.colPivHouseholderQr().solve(B);
    x.resize(n);
    for (int i = 0; i < n; ++i)
        x[i] = sol(i);
    
    return true;
}
// BackgroundExtractor.cpp - Add these missing method implementations at the end of the file

// Add these methods to the BackgroundExtractionWorker class implementation:

bool BackgroundExtractionWorker::performExtraction()
{
    QElapsedTimer timer;
    timer.start();
    
    try {
        emit progress(0, "Starting background extraction...");
        
        if (m_cancelled) return false;
        
        // Initialize result structure
        m_result.channelResults.resize(m_imageData.channels);
        m_result.usedChannelMode = m_settings.channelMode;
        
        bool success = false;
        
        switch (m_settings.channelMode) {
        case ChannelMode::PerChannel:
            success = performPerChannelExtraction();
            break;
        case ChannelMode::Combined:
            success = performCombinedExtraction();
            break;
        case ChannelMode::LuminanceOnly:
            success = performLuminanceOnlyExtraction();
            break;
        default:
            success = performPerChannelExtraction();
            break;
        }
        
        if (m_cancelled) return false;
        
        if (success) {
            combineChannelResults();
            m_result.success = true;
            m_result.processingTimeSeconds = timer.elapsed() / 1000.0;
            emit progress(100, "Background extraction completed");
        } else {
            m_result.success = false;
            m_result.errorMessage = "Background extraction failed";
        }
        
        return success;
        
    } catch (const std::exception& e) {
        m_result.success = false;
        m_result.errorMessage = QString("Exception during background extraction: %1").arg(e.what());
        return false;
    }
}
