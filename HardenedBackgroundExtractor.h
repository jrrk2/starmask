#ifndef HARDENED_BACKGROUND_EXTRACTOR_H
#define HARDENED_BACKGROUND_EXTRACTOR_H

#include "BackgroundExtractor.h"

// Include PCL gradient domain classes
#include "GradientsBase.h"
#include "GradientsHdrCompression.h"
#include "GradientsMergeMosaic.h"

// Additional PCL includes for advanced background extraction
#include <pcl/MultiscaleMedianTransform.h>
#include <pcl/MorphologicalTransformation.h>
#include <pcl/ATrousWaveletTransform.h>
#include <pcl/LinearFit.h>
#include <pcl/SurfaceSpline.h>

class HardenedBackgroundExtractor : public BackgroundExtractor
{
    Q_OBJECT

public:
    explicit HardenedBackgroundExtractor(QObject* parent = nullptr);
    ~HardenedBackgroundExtractor();

    // Enhanced background models using PCL infrastructure
    enum class AdvancedModel {
        // Basic models (from base class)
        Linear = 0,
        Polynomial2 = 1,
        Polynomial3 = 2,
        RBF = 3,
        
        // Advanced PCL-based models
        SurfaceSpline = 10,        // PCL SurfaceSpline interpolation
        MultiscaleMedian = 11,     // Multiscale median transform approach
        WaveletTransform = 12,     // À-trous wavelet decomposition
        GradientDomain = 13,       // Full gradient domain solution
        HybridApproach = 14        // Combination of multiple methods
    };

    // Enhanced sample generation using PCL algorithms
    enum class AdvancedSampling {
        // Basic methods (from base class)
        Automatic = 0,
        Manual = 1,
        Grid = 2,
        
        // Advanced PCL-based methods
        MultiscaleDetection = 10,   // Use multiscale analysis for detection
        MorphologicalMask = 11,     // Morphological operations for masking
        IterativeRefinement = 12,   // Iterative sample refinement
        AdaptiveGrid = 13,          // Adaptive grid based on image content
        ClusterAnalysis = 14        // Cluster-based sample selection
    };

    // Advanced settings structure
    struct AdvancedBackgroundSettings : public BackgroundExtractionSettings {
        AdvancedModel advancedModel = AdvancedModel::SurfaceSpline;
        AdvancedSampling advancedSampling = AdvancedSampling::MultiscaleDetection;
        
        // Multiscale parameters
        int multiscaleLayers = 6;
        double multiscaleThreshold = 0.01;
        bool useLinearMask = true;
        
        // Surface spline parameters
        double smoothingFactor = 0.1;
        int splineOrder = 2;
        double splineThreshold = 0.5;
        
        // Gradient domain parameters
        bool useGradientDomain = false;
        double maxGradient = 1.0;
        double minGradient = 0.0;
        double expGradient = 1.0;
        
        // Morphological parameters
        int structuringElementSize = 5;
        int morphologicalIterations = 3;
        
        // Advanced outlier rejection
        bool useRobustEstimation = true;
        double robustThreshold = 2.5;
        int robustIterations = 5;
        
        // Quality control
        double convergenceThreshold = 0.001;
        int maxIterations = 50;
        bool enableStructureProtection = true;
        double structureProtectionThreshold = 0.1;
    };

    // Set advanced settings
    void setAdvancedSettings(const AdvancedBackgroundSettings& settings);
    AdvancedBackgroundSettings advancedSettings() const;

    // Advanced extraction methods
    bool extractBackgroundWithPCL(const ImageData& imageData);
    bool extractBackgroundMultiscale(const ImageData& imageData);
    bool extractBackgroundSurfaceSpline(const ImageData& imageData);
    bool extractBackgroundGradientDomain(const ImageData& imageData);

    // Quality assessment
    double assessBackgroundQuality(const QVector<float>& backgroundData, 
                                  const QVector<float>& originalData) const;
    
    // Advanced sample generation
    QVector<QPoint> generateMultiscaleSamples(const ImageData& imageData);
    QVector<QPoint> generateMorphologicalSamples(const ImageData& imageData);
    QVector<QPoint> generateAdaptiveGridSamples(const ImageData& imageData);

    // Utility methods for PCL integration
    static pcl::Image convertTopcl::Image(const ImageData& imageData);
    static ImageData convertFromPCLImage(const pcl::Image& pclImage);
    static QVector<float> convertToQVector(const pcl::DImage& pclImage);

    // Preset configurations for different image types
    static AdvancedBackgroundSettings getDeepSkySettings();
    static AdvancedBackgroundSettings getWidefieldSettings();
    static AdvancedBackgroundSettings getPlanetarySettings();
    static AdvancedBackgroundSettings getNoiseReductionSettings();

signals:
    void advancedProgressUpdate(const QString& stage, int percentage, const QString& details);

private:
    class AdvancedBackgroundExtractorPrivate;
    std::unique_ptr<AdvancedBackgroundExtractorPrivate> d_advanced;

    // Advanced processing methods
    bool processMultiscaleExtraction(const pcl::Image& image, pcl::DImage& backgroundModel);
    bool processSurfaceSplineExtraction(const pcl::Image& image, pcl::DImage& backgroundModel);
    bool processGradientDomainExtraction(const pcl::Image& image, pcl::DImage& backgroundModel);
    
    // Quality control and validation
    bool validateExtraction(const pcl::DImage& backgroundModel, const pcl::Image& originalImage);
    double calculateStructureMetric(const pcl::Image& image, const QVector<QPoint>& samples);
    
    // Advanced sample filtering
    QVector<QPoint> filterSamplesWithStructureProtection(const QVector<QPoint>& samples, 
                                                          const pcl::Image& image);
    QVector<QPoint> refineSamplesIteratively(const QVector<QPoint>& initialSamples, 
                                            const pcl::Image& image);
};

// Advanced worker thread for complex processing
class AdvancedBackgroundWorker : public BackgroundExtractionWorker
{
    Q_OBJECT

public:
    AdvancedBackgroundWorker(const ImageData& imageData,
                            const HardenedBackgroundExtractor::AdvancedBackgroundSettings& settings,
                            QObject* parent = nullptr);

signals:
    void advancedProgress(const QString& stage, int percentage, const QString& details);

protected:
    void run() override;

private:
    bool performAdvancedExtraction();
    bool executeMultiscaleApproach();
    bool executeSurfaceSplineApproach();
    bool executeGradientDomainApproach();
    bool executeHybridApproach();
    
    HardenedBackgroundExtractor::AdvancedBackgroundSettings m_advancedSettings;
    
    // PCL objects for advanced processing
    std::unique_ptr<pcl::MultiscaleMedianTransform> m_multiscaleTransform;
    std::unique_ptr<pcl::SurfaceSpline<pcl::RBFType::ThinPlateSpline>> m_surfaceSpline;
    std::unique_ptr<GradientsHdrCompression> m_gradientProcessor;
};

#endif // HARDENED_BACKGROUND_EXTRACTOR_H