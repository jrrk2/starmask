// BackgroundExtractor.h - Enhanced for per-channel background extraction
#ifndef BACKGROUND_EXTRACTOR_H
#define BACKGROUND_EXTRACTOR_H

#include <QObject>
#include <QThread>
#include <QVector>
#include <QPoint>
#include <QString>
#include <memory>
#include <vector>

#include "ImageReader.h"

// Enums for background extraction settings
enum class BackgroundModel {
    Linear = 0,
    Polynomial2 = 1,
    Polynomial3 = 2,
    RBF = 3
};

enum class SampleGeneration {
    Automatic = 0,
    Manual = 1,
    Grid = 2
};

enum class ChannelMode {
    Combined = 0,      // Process all channels together (original behavior)
    PerChannel = 1,    // Process each channel separately
    LuminanceOnly = 2  // Process luminance only, apply to all channels
};

// Per-channel background extraction settings
struct BackgroundExtractionSettings {
    // Model settings
    BackgroundModel model = BackgroundModel::Polynomial2;
    double tolerance = 1.0;
    double deviation = 3.0;
    
    // Sample generation
    SampleGeneration sampleGeneration = SampleGeneration::Automatic;
    int minSamples = 50;
    int maxSamples = 500;
    
    // Grid sampling settings
    int gridRows = 8;
    int gridColumns = 8;
    
    // Outlier rejection
    bool useOutlierRejection = true;
    double rejectionLow = 2.5;
    double rejectionHigh = 2.5;
    int rejectionIterations = 3;
    
    // Processing options
    bool discardModel = false;
    bool replaceTarget = true;
    bool normalizeOutput = false;
    double maxError = 0.1;
    
    // NEW: Channel processing mode
    ChannelMode channelMode = ChannelMode::PerChannel;
    
    // NEW: Per-channel settings override
    bool usePerChannelSettings = false;
    QVector<double> channelTolerances;     // One per channel
    QVector<double> channelDeviations;     // One per channel
    QVector<int> channelMinSamples;        // One per channel
    QVector<int> channelMaxSamples;        // One per channel
    
    // NEW: Channel weighting for combined processing
    QVector<double> channelWeights;        // RGB weights for luminance calculation
    
    // NEW: Cross-channel correlation options
    bool useChannelCorrelation = true;     // Use inter-channel information
    double correlationThreshold = 0.8;     // Threshold for considering channels correlated
    bool shareGoodSamples = true;          // Share sample points between channels
};

// Per-channel result structure
struct ChannelResult {
    int channel = 0;
    bool success = false;
    QString errorMessage;
    
    // Sample information
    int samplesUsed = 0;
    QVector<QPoint> samplePoints;
    QVector<float> sampleValues;
    QVector<bool> sampleRejected;
    
    // Model results
    QVector<float> backgroundData;
    QVector<float> correctedData;
    
    // Error metrics
    double rmsError = 0.0;
    double meanDeviation = 0.0;
    double maxDeviation = 0.0;
    double processingTimeSeconds = 0.0;
    
    // NEW: Channel-specific statistics
    double channelMean = 0.0;
    double channelStdDev = 0.0;
    double channelMin = 0.0;
    double channelMax = 0.0;
    double backgroundLevel = 0.0;
    double gradientStrength = 0.0;
};

struct BackgroundExtractionResult {
    bool success = false;
    QString errorMessage;
    double processingTimeSeconds = 0.0;
    
    // Overall statistics
    int totalSamplesUsed = 0;
    double overallRmsError = 0.0;
    double overallMeanDeviation = 0.0;
    double overallMaxDeviation = 0.0;
    
    // NEW: Per-channel results
    QVector<ChannelResult> channelResults;
    
    // Combined results (for compatibility and display)
    QVector<QPoint> samplePoints;
    QVector<float> sampleValues;
    QVector<bool> sampleRejected;
    QVector<float> backgroundData;    // All channels combined
    QVector<float> correctedData;     // All channels combined
    
    // Legacy compatibility properties
    int samplesUsed = 0;
    double rmsError = 0.0;
    double meanDeviation = 0.0;
    double maxDeviation = 0.0;
    
    // NEW: Channel analysis results
    QVector<double> channelCorrelations;   // Correlation between channels
    QVector<QString> channelNotes;         // Per-channel processing notes
    ChannelMode usedChannelMode = ChannelMode::Combined;
    
    QString getStatsSummary() const {
        if (usedChannelMode == ChannelMode::PerChannel && !channelResults.isEmpty()) {
            QString summary = QString("Per-Channel Results:\n");
            for (int i = 0; i < channelResults.size(); ++i) {
                const auto& ch = channelResults[i];
                summary += QString("  Ch%1: %2 samples, RMS: %3, Background: %4\n")
                    .arg(i)
                    .arg(ch.samplesUsed)
                    .arg(ch.rmsError, 0, 'f', 6)
                    .arg(ch.backgroundLevel, 0, 'f', 6);
            }
            summary += QString("Total Processing Time: %1s")
                .arg(processingTimeSeconds, 0, 'f', 2);
            return summary;
        } else {
            return QString("Samples: %1, RMS Error: %2, Max Error: %3, Processing Time: %4s")
                    .arg(samplesUsed)
                    .arg(rmsError, 0, 'f', 6)
                    .arg(maxDeviation, 0, 'f', 6)
                    .arg(processingTimeSeconds, 0, 'f', 2);
        }
    }
    
    // NEW: Get results for specific channel
    ChannelResult getChannelResult(int channel) const {
        if (channel >= 0 && channel < channelResults.size()) {
            return channelResults[channel];
        }
        return ChannelResult();
    }
    
    // NEW: Get background data for specific channel
    QVector<float> getChannelBackgroundData(int channel, int width, int height) const {
        if (channel >= 0 && channel < channelResults.size()) {
            return channelResults[channel].backgroundData;
        }
        
        // Fallback: extract from combined data
        QVector<float> channelData;
        if (!backgroundData.isEmpty() && channelResults.size() > 0) {
            int pixelsPerChannel = width * height;
            int startIndex = channel * pixelsPerChannel;
            if (startIndex + pixelsPerChannel <= backgroundData.size()) {
                channelData.reserve(pixelsPerChannel);
                for (int i = startIndex; i < startIndex + pixelsPerChannel; ++i) {
                    channelData.append(backgroundData[i]);
                }
            }
        }
        return channelData;
    }
};

class BackgroundExtractorPrivate;

// Enhanced background extraction class with per-channel support
class BackgroundExtractor : public QObject
{
    Q_OBJECT

public:
    explicit BackgroundExtractor(QObject* parent = nullptr);
    ~BackgroundExtractor();

    // Settings
    void setSettings(const BackgroundExtractionSettings& settings);
    BackgroundExtractionSettings settings() const;
    static BackgroundExtractionSettings getDefaultSettings();
    static BackgroundExtractionSettings getConservativeSettings();
    static BackgroundExtractionSettings getAggressiveSettings();
    
    // NEW: Per-channel preset settings
    static BackgroundExtractionSettings getPerChannelSettings();
    static BackgroundExtractionSettings getAstronomyRGBSettings();  // Optimized for RGB astrophotography
    static BackgroundExtractionSettings getLuminanceOnlySettings(); // Process luminance, apply to all

    // Main extraction methods
    bool extractBackground(const ImageData& imageData);
    bool extractBackgroundAsync(const ImageData& imageData);
    
    // Preview and utility methods
    bool generatePreview(const ImageData& imageData, int maxSize = 256);
    void cancelExtraction();
    bool isExtracting() const;

    // Results
    BackgroundExtractionResult result() const;
    bool hasResult() const;
    void clearResult();
    
    // NEW: Channel-specific result access
    ChannelResult getChannelResult(int channel) const;
    QVector<float> getChannelBackgroundData(int channel, int width, int height) const;
    QVector<float> getChannelCorrectedData(int channel, int width, int height) const;

    // Manual sample management
    void addManualSample(const QPoint& point, float value);
    void clearManualSamples();
    QVector<QPoint> manualSamples() const;
    QVector<QPoint> getManualSamples() const;
    
    // NEW: Per-channel manual samples
    void addManualSampleForChannel(int channel, const QPoint& point, float value);
    void clearManualSamplesForChannel(int channel);
    QVector<QPoint> getManualSamplesForChannel(int channel) const;
    
    // NEW: Channel analysis utilities
    QVector<double> analyzeChannelCorrelations(const ImageData& imageData) const;
    QVector<double> estimateChannelBackgroundLevels(const ImageData& imageData) const;
    QString getChannelAnalysisReport(const ImageData& imageData) const;

signals:
    void progress(int percentage, const QString& message);
    void extractionCompleted(const BackgroundExtractionResult& result);
    
    // Additional signals for UI updates
    void extractionStarted();
    void extractionProgress(int percentage, const QString& message);
    void extractionFinished(bool success, const QString& message);
    void previewReady(const BackgroundExtractionResult& result);
    
    // NEW: Per-channel progress signals
    void channelProgress(int channel, int percentage, const QString& message);
    void channelCompleted(int channel, const ChannelResult& result);

private slots:
    void onWorkerFinished(const BackgroundExtractionResult& result);

private:
    std::unique_ptr<BackgroundExtractorPrivate> d;
// BackgroundExtractor.h - Add these missing member variable declarations
// Add these to the private section of the BackgroundExtractionWorker class:

private:
    // Main processing methods
    bool performExtraction();
    bool performPerChannelExtraction();
    bool performCombinedExtraction();
    bool performLuminanceOnlyExtraction();
    
    // Per-channel processing
    bool extractChannelBackground(int channel, ChannelResult& result);
    bool generateChannelSamples(int channel, QVector<QPoint>& samples, QVector<float>& values);
    bool fitChannelModel(int channel, const QVector<QPoint>& samples, 
                        const QVector<float>& values, ChannelResult& result);
    
    // Channel analysis
    QVector<double> calculateChannelCorrelations();
    void shareGoodSamplesBetweenChannels();
    void applyCrossChannelValidation();
    
    // Utility methods
    QVector<float> extractChannelData(int channel) const;
    void combineChannelResults();
    void calculateLuminance(QVector<float>& luminance) const;
    
    bool generateImprovedAutomaticSamples();
    bool generateChannelSpecificSamples(int channel, QVector<QPoint>& samples, QVector<float>& values);
    
    // Model fitting methods
    bool fitPolynomialModel();
    void rejectOutliers();
    void calculateErrorMetrics();
    
    // Channel-specific fitting
    bool fitChannelPolynomialModel(int channel, const QVector<QPoint>& samples, 
                                  const QVector<float>& values, ChannelResult& result);
    void rejectChannelOutliers(int channel, QVector<QPoint>& samples, QVector<float>& values);
    
    // Utility methods
    int getModelOrder() const;
    int getNumTerms(int order) const;
    double evaluatePolynomial(const std::vector<double>& coeffs, double x, double y, int order) const;
    bool solveLinearSystem(const std::vector<std::vector<double>>& A, 
                          const std::vector<double>& b, 
                          std::vector<double>& x);

    // Data members
    ImageData m_imageData;
    BackgroundExtractionSettings m_settings;
    BackgroundExtractionResult m_result;
    bool m_cancelled;
};

// Enhanced worker thread for per-channel background extraction
class BackgroundExtractionWorker : public QThread
{
    Q_OBJECT

public:
    explicit BackgroundExtractionWorker(const ImageData& imageData,
                                      const BackgroundExtractionSettings& settings,
                                      QObject* parent = nullptr);

    void cancel();

signals:
    void progress(int percentage, const QString& message);
    void finished(const BackgroundExtractionResult& result);
    void channelProgress(int channel, int percentage, const QString& message);
    void channelCompleted(int channel, const ChannelResult& result);

protected:
    void run() override;

private:
    // Main processing methods
    bool performExtraction();
    bool performPerChannelExtraction();
    bool performCombinedExtraction();
    bool performLuminanceOnlyExtraction();
    
    // Per-channel processing
    bool extractChannelBackground(int channel, ChannelResult& result);
    bool generateChannelSamples(int channel, QVector<QPoint>& samples, QVector<float>& values);
    bool fitChannelModel(int channel, const QVector<QPoint>& samples, 
                        const QVector<float>& values, ChannelResult& result);
    
    // Channel analysis
    QVector<double> calculateChannelCorrelations();
    void shareGoodSamplesBetweenChannels();
    void applyCrossChannelValidation();
    
    // Utility methods
    QVector<float> extractChannelData(int channel) const;
    void combineChannelResults();
    void calculateLuminance(QVector<float>& luminance) const;
    
    // Sample generation methods (channel-aware)
    bool generateGridSamples();
    bool generateImprovedAutomaticSamples();
    bool generateChannelSpecificSamples(int channel, QVector<QPoint>& samples, QVector<float>& values);
    
    // Model fitting methods
    bool fitPolynomialModel();
    void rejectOutliers();
    void calculateErrorMetrics();
    
    // Channel-specific fitting
    bool fitChannelPolynomialModel(int channel, const QVector<QPoint>& samples, 
                                  const QVector<float>& values, ChannelResult& result);
    void rejectChannelOutliers(int channel, QVector<QPoint>& samples, QVector<float>& values);
    
    // Utility methods
    int getModelOrder() const;
    int getNumTerms(int order) const;
    double evaluatePolynomial(const std::vector<double>& coeffs, double x, double y, int order) const;
    bool solveLinearSystem(const std::vector<std::vector<double>>& A, 
                          const std::vector<double>& b, 
                          std::vector<double>& x);
    // Sample generation methods (channel-aware)
    bool generateSamples();  // Add this declaration
    void applyCorrection();  // Add this declaration
    bool fitModel();  // Add this declaration

    // Data members
    ImageData m_imageData;
    BackgroundExtractionSettings m_settings;
    BackgroundExtractionResult m_result;
    bool m_cancelled;

    // Per-channel data
    QVector<QVector<QPoint>> m_channelSamples;      // Samples for each channel
    QVector<QVector<float>> m_channelSampleValues;  // Sample values for each channel
    QVector<QVector<bool>> m_channelSampleValid;    // Valid flags for each channel
    
    // Shared samples (when shareGoodSamples is enabled)
    QVector<QPoint> m_sharedSamples;
    QVector<bool> m_sharedSampleValid;

    // ADD THESE MISSING MEMBER VARIABLES:
    // Sample data for combined processing
    QVector<QPoint> m_samples;      // Main samples list
    QVector<float> m_sampleValues;  // Sample values
    QVector<bool> m_sampleValid;    // Valid flags for samples

};

#endif // BACKGROUND_EXTRACTOR_H
