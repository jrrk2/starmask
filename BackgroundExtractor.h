// BackgroundExtractor.h - Complete fixed header
#ifndef BACKGROUND_EXTRACTOR_H
#define BACKGROUND_EXTRACTOR_H

#include <QObject>
#include <QThread>
#include <QVector>
#include <QPoint>
#include <QString>
#include <memory>
#include <vector>

// Use the existing ImageData from ImageReader.h
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

// Structures for background extraction
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
    int rejectionIterations = 3;  // Added missing property
    
    // Processing options
    bool discardModel = false;
    bool replaceTarget = true;
    bool normalizeOutput = false;
    double maxError = 0.1;  // Added missing property
};

struct BackgroundExtractionResult {
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
    
    // Additional properties expected by widget
    double processingTimeSeconds = 0.0;
    
    // Stats summary method
    QString getStatsSummary() const {
        return QString("Samples: %1, RMS Error: %2, Max Error: %3, Processing Time: %4s")
                .arg(samplesUsed)
                .arg(rmsError, 0, 'f', 6)
                .arg(maxDeviation, 0, 'f', 6)
                .arg(processingTimeSeconds, 0, 'f', 2);
    }
};

class BackgroundExtractorPrivate;

// Main background extraction class
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

    // Manual sample management
    void addManualSample(const QPoint& point, float value);
    void clearManualSamples();
    QVector<QPoint> manualSamples() const;
    QVector<QPoint> getManualSamples() const; // Alternative name for compatibility

signals:
    void progress(int percentage, const QString& message);
    void extractionCompleted(const BackgroundExtractionResult& result);
    
    // Additional signals expected by the widget
    void extractionStarted();
    void extractionProgress(int percentage, const QString& message);
    void extractionFinished(bool success, const QString& message);
    void previewReady(const BackgroundExtractionResult& result);

private slots:
    void onWorkerFinished(const BackgroundExtractionResult& result);

private:
    std::unique_ptr<BackgroundExtractorPrivate> d;
};

// Worker thread for background extraction
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

protected:
    void run() override;

private:
    // Main processing steps
    bool generateSamples();
    bool fitModel();
    void applyCorrection();

    // Sample generation methods
    bool generateGridSamples();
    bool generateImprovedAutomaticSamples();

    // Model fitting methods
    bool fitPolynomialModel();
    void rejectOutliers();
    void calculateErrorMetrics();

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

    // Sample data
    QVector<QPoint> m_samples;
    QVector<float> m_sampleValues;
    QVector<bool> m_sampleValid;
};

#endif // BACKGROUND_EXTRACTOR_H
