#ifndef HARDENED_BACKGROUND_EXTRACTOR_H
#define HARDENED_BACKGROUND_EXTRACTOR_H

#include "BackgroundExtractor.h"

// Include the actual PCL gradient domain classes
#include "GradientsBase.h"
#include "GradientsHdrCompression.h"
#include "GradientsMergeMosaic.h"

// Additional PCL includes for advanced background extraction
#include <pcl/MultiscaleMedianTransform.h>
#include <pcl/MorphologicalTransformation.h>
#include <pcl/LinearFit.h>

class HardenedBackgroundExtractor : public BackgroundExtractor
{
    Q_OBJECT

public:
    explicit HardenedBackgroundExtractor(QObject* parent = nullptr);
    ~HardenedBackgroundExtractor();

    // Enhanced background models using PCL gradient domain infrastructure
    enum class AdvancedModel {
        // Basic models (from base class)
        Linear = 0,
        Polynomial2 = 1,
        Polynomial3 = 2,
        RBF = 3,
        
        // Advanced gradient domain models
        GradientDomain = 10,           // Full gradient domain solution using GradientsBase
        HDRCompression = 11,           // HDR compression for background extraction
        MultiscaleMedian = 12,         // Multiscale median transform approach
        MorphologicalMask = 13,        // Morphological operations for background detection
        HybridGradient = 14            // Combination of gradient methods
    };

    // Enhanced sample generation using gradient domain analysis
    enum class AdvancedSampling {
        // Basic methods (from base class)
        Automatic = 0,
        Manual = 1,
        Grid = 2,
        
        // Advanced gradient domain methods
        GradientAnalysis = 10,         // Use gradient magnitude for sample selection
        LowGradientRegions = 11,       // Focus on low-gradient (background) regions
        StructureAvoidance = 12,       // Avoid high-structure areas using gradients
        AdaptiveThreshold = 13,        // Adaptive thresholding based on gradient statistics
        MultiscaleAnalysis = 14        // Use multiscale decomposition for sampling
    };

    // Advanced settings structure
    struct AdvancedBackgroundSettings : public BackgroundExtractionSettings {
        AdvancedModel advancedModel = AdvancedModel::GradientDomain;
        AdvancedSampling advancedSampling = AdvancedSampling::GradientAnalysis;
        
        // Gradient domain parameters
        double gradientThreshold = 0.1;           // Threshold for low-gradient regions
        double gradientPercentile = 0.2;          // Percentile for gradient-based thresholding
        bool useGradientMagnitude = true;         // Use gradient magnitude for analysis
        
        // HDR compression parameters (for GradientsHdrCompression)
        double logMaxGradient = 1.0;              // Log10 of maximum gradient for compression
        double logMinGradient = -2.0;             // Log10 of minimum gradient
        double expGradient = 1.0;                 // Exponent for gradient transformation
        bool rescale01 = true;                    // Rescale to [0,1] range
        bool preserveColor = true;                // Preserve color ratios
        
        // Multiscale parameters
        int multiscaleLayers = 6;                 // Number of scales for analysis
        double multiscaleThreshold = 0.01;       // Threshold for multiscale detection
        bool useLinearMask = true;                // Use linear mask in multiscale
        
        // Morphological parameters
        int structuringElementSize = 5;           // Size of morphological structuring element
        int morphologicalIterations = 3;         // Number of morphological iterations
        
        // Advanced quality control
        double convergenceThreshold = 0.001;     // Convergence criterion for iterative methods
        int maxIterations = 50;                  // Maximum iterations for solving
        bool enableStructureProtection = true;   // Protect astronomical structures
        double structureProtectionThreshold = 0.1; // Threshold for structure protection
        
        // Outlier rejection (enhanced)
        bool useRobustEstimation = true;          // Use robust statistical methods
        double robustThreshold = 2.5;            // Threshold for robust outlier detection
        int robustIterations = 5;                // Iterations for robust estimation
    };

    // Set advanced settings
    void setAdvancedSettings(const AdvancedBackgroundSettings& settings);
    AdvancedBackgroundSettings advancedSettings() const;

    // Advanced extraction methods using gradient domain
    bool extractBackgroundWithGradients(const ImageData& imageData);
    bool extractBackgroundWithHDRCompression(const ImageData& imageData);
    bool extractBackgroundWithMultiscale(const ImageData& imageData);
    
    // Quality assessment using gradient domain metrics
    double assessGradientBackgroundQuality(const QVector<float>& backgroundData, 
                                          const QVector<float>& originalData) const;
    
    // Advanced sample generation using gradient analysis
    QVector<QPoint> generateGradientBasedSamples(const ImageData& imageData);
    QVector<QPoint> generateLowGradientSamples(const ImageData& imageData);
    QVector<QPoint> generateStructureAvoidingSamples(const ImageData& imageData);

    // Utility methods for PCL gradient domain integration
    static GradientsBase::imageType_t convertToGradientImage(const ImageData& imageData);
    static ImageData convertFromGradientImage(const GradientsBase::imageType_t& gradientImage);
    static QVector<float> convertGradientToQVector(const GradientsBase::imageType_t& gradientImage);

    // Preset configurations for different astronomical image types
    static AdvancedBackgroundSettings getDeepSkyGradientSettings();
    static AdvancedBackgroundSettings getWidefieldGradientSettings();
    static AdvancedBackgroundSettings getPlanetaryGradientSettings();
    static AdvancedBackgroundSettings getHDRProcessingSettings();

signals:
    void gradientProgressUpdate(const QString& stage, int percentage, const QString& details);

private:
    class AdvancedBackgroundExtractorPrivate;
    std::unique_ptr<AdvancedBackgroundExtractorPrivate> d_advanced;

    // Gradient domain processing methods
    bool processGradientDomainExtraction(const GradientsBase::imageType_t& image, 
                                       GradientsBase::imageType_t& backgroundModel);
    bool processHDRCompressionExtraction(const GradientsBase::imageType_t& image,
                                       GradientsBase::imageType_t& backgroundModel);
    bool processMultiscaleExtraction(const GradientsBase::imageType_t& image,
                                   GradientsBase::imageType_t& backgroundModel);
    
    // Gradient-based sample analysis
    QVector<QPoint> analyzeGradientField(const GradientsBase::imageType_t& dxImage,
                                        const GradientsBase::imageType_t& dyImage);
    double calculateGradientStatistics(const GradientsBase::imageType_t& dxImage,
                                      const GradientsBase::imageType_t& dyImage,
                                      double& mean, double& stdDev) const;
    
    // Quality control using gradient domain metrics
    bool validateGradientExtraction(const GradientsBase::imageType_t& backgroundModel,
                                  const GradientsBase::imageType_t& originalImage);
    double calculateGradientCoherence(const GradientsBase::imageType_t& image) const;
    
    // Advanced filtering and processing
    QVector<QPoint> filterGradientSamples(const QVector<QPoint>& samples,
                                         const GradientsBase::imageType_t& image);
    bool applyGradientBasedSmoothing(GradientsBase::imageType_t& backgroundModel);
};

// Advanced worker thread for gradient domain processing
class AdvancedGradientWorker : public BackgroundExtractionWorker
{
    Q_OBJECT

public:
    AdvancedGradientWorker(const ImageData& imageData,
                          const HardenedBackgroundExtractor::AdvancedBackgroundSettings& settings,
                          QObject* parent = nullptr);

signals:
    void gradientProgress(const QString& stage, int percentage, const QString& details);

protected:
    void run() override;

private:
    bool performGradientDomainExtraction();
    bool executeGradientAnalysis();
    bool executeHDRCompressionMethod();
    bool executeMultiscaleMethod();
    bool executeHybridGradientMethod();
    
    // Gradient domain utilities
    bool computeGradientFields();
    bool analyzeGradientStatistics();
    bool selectGradientBasedSamples();
    bool fitGradientDomainModel();
    
    HardenedBackgroundExtractor::AdvancedBackgroundSettings m_advancedSettings;
    
    // PCL gradient domain objects
    std::unique_ptr<GradientsBase> m_gradientBase;
    std::unique_ptr<GradientsHdrCompression> m_hdrCompressor;
    std::unique_ptr<pcl::MultiscaleMedianTransform> m_multiscaleTransform;
    
    // Gradient field data
    GradientsBase::imageType_t m_dxImage;
    GradientsBase::imageType_t m_dyImage;
    GradientsBase::imageType_t m_workingImage;
    
    // Statistics
    double m_gradientMean = 0.0;
    double m_gradientStdDev = 0.0;
    double m_gradientThreshold = 0.0;
};

#endif // HARDENED_BACKGROUND_EXTRACTOR_H