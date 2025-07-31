#ifndef BACKGROUND_EXTRACTOR_H
#define BACKGROUND_EXTRACTOR_H

#include <QObject>
#include <QThread>
#include <QString>
#include <QVector>
#include <QPoint>
#include <memory>

// Include ImageReader.h to get complete ImageData definition
#include "ImageReader.h"

// Forward declarations
class BackgroundExtractorPrivate;

enum class BackgroundModel {
    Linear,        // Linear background model
    Polynomial2,   // 2nd order polynomial
    Polynomial3,   // 3rd order polynomial
    RBF           // Radial Basis Function
};

enum class SampleGeneration {
    Automatic,     // Automatic sample generation
    Manual,        // User-defined samples
    Grid          // Regular grid sampling
};

struct BackgroundExtractionSettings {
    // Core parameters
    BackgroundModel model = BackgroundModel::Polynomial2;
    SampleGeneration sampleGeneration = SampleGeneration::Automatic;
    
    // Sample generation parameters
    double tolerance = 1.0;          // Sample tolerance (0.1 - 10.0)
    double deviation = 0.8;          // Maximum deviation (0.1 - 2.0)
    int minSamples = 50;             // Minimum number of samples
    int maxSamples = 5000;           // Maximum number of samples
    
    // Outlier rejection
    bool useOutlierRejection = true;
    double rejectionLow = 2.0;       // Low rejection threshold (sigma)
    double rejectionHigh = 2.5;      // High rejection threshold (sigma)
    int rejectionIterations = 3;     // Maximum rejection iterations
    
    // Grid sampling (when sampleGeneration == Grid)
    int gridRows = 16;               // Number of grid rows
    int gridColumns = 16;            // Number of grid columns
    
    // Processing options
    bool discardModel = true;        // Discard model after extraction
    bool replaceTarget = false;      // Replace original image with corrected
    bool normalizeOutput = true;     // Normalize the output background
    
    // Quality control
    double maxError = 0.1;           // Maximum fitting error threshold
    
    QString toString() const {
        return QString("Model: %1, Samples: %2-%3, Tolerance: %4, Deviation: %5")
               .arg(static_cast<int>(model))
               .arg(minSamples).arg(maxSamples)
               .arg(tolerance).arg(deviation);
    }
};

struct BackgroundExtractionResult {
    bool success = false;
    QString errorMessage;
    
    // Results
    QVector<float> backgroundData;    // Extracted background model
    QVector<float> correctedData;     // Background-corrected image
    
    // Statistics
    int samplesUsed = 0;
    int samplesRejected = 0;
    double rmsError = 0.0;
    double meanDeviation = 0.0;
    double maxDeviation = 0.0;
    
    // Sample locations (for visualization)
    QVector<QPoint> samplePoints;
    QVector<float> sampleValues;
    QVector<bool> sampleRejected;
    
    // Timing
    double processingTimeSeconds = 0.0;
    
    bool isValid() const {
        return success && !backgroundData.isEmpty();
    }
    
    void clear() {
        success = false;
        errorMessage.clear();
        backgroundData.clear();
        correctedData.clear();
        samplesUsed = samplesRejected = 0;
        rmsError = meanDeviation = maxDeviation = 0.0;
        samplePoints.clear();
        sampleValues.clear();
        sampleRejected.clear();
        processingTimeSeconds = 0.0;
    }
    
    QString getStatsSummary() const {
        if (!success) return "Extraction failed";
        
        return QString("Samples: %1 (rejected: %2)\n"
                      "RMS Error: %3\n"
                      "Mean Dev: %4\n"
                      "Max Dev: %5\n"
                      "Time: %6s")
               .arg(samplesUsed).arg(samplesRejected)
               .arg(rmsError, 0, 'f', 6)
               .arg(meanDeviation, 0, 'f', 6)
               .arg(maxDeviation, 0, 'f', 6)
               .arg(processingTimeSeconds, 0, 'f', 2);
    }
};

class BackgroundExtractor : public QObject
{
    Q_OBJECT

public:
    explicit BackgroundExtractor(QObject* parent = nullptr);
    ~BackgroundExtractor();
    
    // Settings management
    void setSettings(const BackgroundExtractionSettings& settings);
    BackgroundExtractionSettings settings() const;
    
    // Main extraction function
    bool extractBackground(const ImageData& imageData);
    
    // Async extraction
    void extractBackgroundAsync(const ImageData& imageData);
    void cancelExtraction();
    bool isExtracting() const;
    
    // Results
    const BackgroundExtractionResult& result() const;
    bool hasResult() const;
    void clearResult();
    
    // Preview functionality
    bool generatePreview(const ImageData& imageData, int previewSize = 256);
    const BackgroundExtractionResult& previewResult() const;
    
    // Manual sample management
    void addManualSample(const QPoint& point, float value);
    void removeManualSample(const QPoint& point);
    void clearManualSamples();
    QVector<QPoint> getManualSamples() const;
    
    // Utility functions
    static QString getModelName(BackgroundModel model);
    static QString getSampleGenerationName(SampleGeneration generation);
    static BackgroundExtractionSettings getDefaultSettings();
    static BackgroundExtractionSettings getConservativeSettings();
    static BackgroundExtractionSettings getAggressiveSettings();

signals:
    void extractionStarted();
    void extractionProgress(int percentage, const QString& stage);
    void extractionFinished(bool success);
    void extractionCancelled();
    void previewReady();

private slots:
    void onWorkerFinished();
    void onWorkerProgress(int percentage, const QString& stage);

private:
    std::unique_ptr<BackgroundExtractorPrivate> d;
    
    // Non-copyable
    BackgroundExtractor(const BackgroundExtractor&) = delete;
    BackgroundExtractor& operator=(const BackgroundExtractor&) = delete;
};

// Worker thread for background extraction
class BackgroundExtractionWorker : public QThread
{
    Q_OBJECT

public:
    BackgroundExtractionWorker(const ImageData& imageData, 
                              const BackgroundExtractionSettings& settings,
                              QObject* parent = nullptr);
    
    const BackgroundExtractionResult& result() const { return m_result; }
    void requestCancel() { m_cancelled = true; }
    
    // Public method for synchronous processing
    bool processExtraction();

signals:
    void progress(int percentage, const QString& stage);

protected:
    void run() override;

private:
    bool performExtraction();
    bool generateSamples();
    bool fitModel();
    bool evaluateModel();
    void rejectOutliers();
    
    // Use ImageData directly instead of forward declaration
    ImageData m_imageData;
    BackgroundExtractionSettings m_settings;
    BackgroundExtractionResult m_result;
    bool m_cancelled = false;
    
    // PCL-specific data
    QVector<QPoint> m_samples;
    QVector<float> m_sampleValues;
    QVector<bool> m_sampleValid;
    
    // Make class non-copyable to avoid Qt MOC issues
    Q_DISABLE_COPY(BackgroundExtractionWorker)
};

#endif // BACKGROUND_EXTRACTOR_H