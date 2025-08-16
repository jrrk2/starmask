#include "HardenedBackgroundExtractor.h"
#include "ImageReader.h"

// PCL includes from the gradient module
#include "GradientsBase.h"
#include "GradientsHdrCompression.h"
#include "GradientsMergeMosaic.h"

// Additional PCL includes for advanced algorithms
#include <pcl/MultiscaleMedianTransform.h>
#include <pcl/MorphologicalTransformation.h>
#include <pcl/ATrousWaveletTransform.h>
#include <pcl/LinearFit.h>
#include <pcl/SurfaceSpline.h>
#include <pcl/RobustChauvenetRejection.h>
#include <pcl/MedianFilter.h>
#include <pcl/GaussianFilter.h>

// Qt includes
#include <QDebug>
#include <QTime>
#include <QRandomGenerator>
#include <QtMath>

class HardenedBackgroundExtractor::AdvancedBackgroundExtractorPrivate
{
public:
    AdvancedBackgroundSettings advancedSettings;
    
    // PCL-based processors 
    std::unique_ptr<GradientsHdrCompression> gradientCompressor;
    std::unique_ptr<GradientsMergeMosaic> mosaicProcessor;
    
    // Quality metrics
    double lastQualityScore = 0.0;
    QString lastQualityReport;
    
    AdvancedBackgroundExtractorPrivate() {
        gradientCompressor = std::make_unique<GradientsHdrCompression>();
        mosaicProcessor = std::make_unique<GradientsMergeMosaic>();
    }
};

HardenedBackgroundExtractor::HardenedBackgroundExtractor(QObject* parent)
    : BackgroundExtractor(parent)
    , d_advanced(std::make_unique<AdvancedBackgroundExtractorPrivate>())
{
    d_advanced->advancedSettings = getDeepSkyGradientSettings();
}

HardenedBackgroundExtractor::~HardenedBackgroundExtractor() = default;

void HardenedBackgroundExtractor::setAdvancedSettings(const AdvancedBackgroundSettings& settings)
{
    d_advanced->advancedSettings = settings;
    
    // Also update base class settings
    BackgroundExtractionSettings baseSettings;
    baseSettings.model = static_cast<BackgroundModel>(static_cast<int>(settings.advancedModel));
    baseSettings.sampleGeneration = static_cast<SampleGeneration>(static_cast<int>(settings.advancedSampling));
    baseSettings.tolerance = settings.tolerance;
    baseSettings.deviation = settings.deviation;
    baseSettings.minSamples = settings.minSamples;
    baseSettings.maxSamples = settings.maxSamples;
    baseSettings.useOutlierRejection = settings.useOutlierRejection;
    baseSettings.rejectionLow = settings.rejectionLow;
    baseSettings.rejectionHigh = settings.rejectionHigh;
    baseSettings.rejectionIterations = settings.rejectionIterations;
    
    setSettings(baseSettings);
}

HardenedBackgroundExtractor::AdvancedBackgroundSettings HardenedBackgroundExtractor::advancedSettings() const
{
    return d_advanced->advancedSettings;
}

bool HardenedBackgroundExtractor::extractBackgroundWithGradients(const ImageData& imageData)
{
    if (!imageData.isValid()) {
        return false;
    }
    
    emit extractionStarted();
    emit gradientProgressUpdate("Initializing gradient-based extraction", 5, "Converting image data");
    
    try {
        // Convert to PCL image format - FIXED METHOD NAME
        pcl::Image pclImage = convertToPCLImage(imageData);
        
        // Choose extraction method based on advanced model
        pcl::DImage backgroundModel;
        bool success = false;
        
        switch (d_advanced->advancedSettings.advancedModel) {
        case AdvancedModel::MultiscaleMedian:
            success = processMultiscaleExtraction(pclImage, backgroundModel);
            break;
        case AdvancedModel::GradientDomain:
            success = processGradientDomainExtraction(pclImage, backgroundModel);
            break;
        case AdvancedModel::HDRCompression:
            success = processHDRCompressionExtraction(pclImage, backgroundModel);
            break;
        case AdvancedModel::HybridGradient:
            success = processHybridExtraction(pclImage, backgroundModel);
            break;
        default:
            // Fall back to base class implementation
            return extractBackgroundAsync(imageData);
        }
        
        if (success) {
            // Convert back to our format and store results
            QVector<float> backgroundData = convertToQVector(backgroundModel);
            
            // Create corrected image
            QVector<float> correctedData = imageData.pixels;
            for (int i = 0; i < correctedData.size() && i < backgroundData.size(); ++i) {
                correctedData[i] -= backgroundData[i];
            }
            
            // Update result structure
            BackgroundExtractionResult newResult;
            newResult.success = true;
            newResult.backgroundData = backgroundData;
            newResult.correctedData = correctedData;
            newResult.rmsError = calculateRMSError(backgroundData, imageData.pixels);
            newResult.processingTimeSeconds = 0.0; // Would be set by timer
            
            // Quality assessment
            d_advanced->lastQualityScore = assessGradientBackgroundQuality(backgroundData, imageData.pixels);
            
            emit extractionFinished(true, "Success");
            return true;
        }
        
    } catch (const pcl::Error& e) {
        qDebug() << "PCL Error in gradient extraction:" << e.Message().c_str();
        emit extractionFinished(false, QString("PCL Error: %1").arg(e.Message().c_str()));
        return false;
    } catch (const std::exception& e) {
        qDebug() << "Standard error in gradient extraction:" << e.what();
        emit extractionFinished(false, QString("Error: %1").arg(e.what()));
        return false;
    }
    
    emit extractionFinished(false, "Unknown error");
    return false;
}

bool HardenedBackgroundExtractor::extractBackgroundWithHDRCompression(const ImageData& imageData)
{
    return extractBackgroundWithGradients(imageData); // Delegate to main method
}

bool HardenedBackgroundExtractor::extractBackgroundWithMultiscale(const ImageData& imageData)
{
    return extractBackgroundWithGradients(imageData); // Delegate to main method
}

bool HardenedBackgroundExtractor::processMultiscaleExtraction(const pcl::Image& image, pcl::DImage& backgroundModel)
{
    emit gradientProgressUpdate("Multiscale extraction", 20, "Performing multiscale median transform");
    
    try {
        // Create MultiscaleMedianTransform instance
        pcl::MultiscaleMedianTransform mmt;
        
        // Configure the transform
        mmt.SetNumberOfLayers(d_advanced->advancedSettings.multiscaleLayers);
        mmt.EnableLinearMask(d_advanced->advancedSettings.useLinearMask);
        
        // Apply multiscale decomposition
        pcl::ImageVariant transformedImage;
        transformedImage = pcl::ImageVariant(&image);
        
        emit gradientProgressUpdate("Multiscale extraction", 40, "Analyzing scale layers");
        
        // Execute the transform
        bool success = mmt.ExecuteOn(transformedImage);
        
        if (success) {
            // Extract the large-scale structures (background)
            // The last layers contain the background information
            backgroundModel.Assign(image);
            
            // Apply smoothing to create background model
            pcl::GaussianFilter gaussianFilter;
            gaussianFilter.SetSigma(2.0);  // Adjust based on image characteristics
            
            pcl::ImageVariant backgroundVariant(&backgroundModel);
            gaussianFilter.ExecuteOn(backgroundVariant);
            
            emit gradientProgressUpdate("Multiscale extraction", 80, "Refining background model");
            
            return true;
        }
        
    } catch (const pcl::Error& e) {
        qDebug() << "PCL Error in multiscale extraction:" << e.Message().c_str();
        return false;
    }
    
    return false;
}

bool HardenedBackgroundExtractor::processGradientDomainExtraction(const pcl::Image& image, pcl::DImage& backgroundModel)
{
    emit gradientProgressUpdate("Gradient domain extraction", 20, "Initializing gradient domain processing");
    
    try {
        // Use the existing GradientsHdrCompression from the module
        GradientsBase::imageType_t gradientImage;
        gradientImage = reinterpret_cast<const GradientsBase::imageType_t&>(image);
        
        d_advanced->gradientCompressor->hdrCompressionSetImage(gradientImage);
        
        emit gradientProgressUpdate("Gradient domain extraction", 40, "Analyzing gradient field");
        
        // Configure gradient compression parameters
        double maxGradient = d_advanced->advancedSettings.logMaxGradient;
        double minGradient = d_advanced->advancedSettings.logMinGradient;
        double expGradient = d_advanced->advancedSettings.expGradient;
        
        // Apply gradient domain processing
        GradientsBase::imageType_t resultImage;
        d_advanced->gradientCompressor->hdrCompression(
            maxGradient, minGradient, expGradient, false, resultImage);
        
        emit gradientProgressUpdate("Gradient domain extraction", 60, "Extracting background component");
        
        // The result contains the compressed dynamic range
        // We need to extract the background component from this
        backgroundModel.Assign(reinterpret_cast<const pcl::DImage&>(resultImage));
        
        // Apply additional smoothing to isolate background
        pcl::MedianFilter medianFilter;
        medianFilter.SetSize(5);  // 5x5 median filter
        
        pcl::ImageVariant backgroundVariant(&backgroundModel);
        medianFilter.ExecuteOn(backgroundVariant);
        
        emit gradientProgressUpdate("Gradient domain extraction", 80, "Refining gradient-based model");
        
        return true;
        
    } catch (const pcl::Error& e) {
        qDebug() << "PCL Error in gradient domain extraction:" << e.Message().c_str();
        return false;
    }
    
    return false;
}

bool HardenedBackgroundExtractor::processHDRCompressionExtraction(const pcl::Image& image, pcl::DImage& backgroundModel)
{
    emit gradientProgressUpdate("HDR compression extraction", 20, "Setting up HDR compression");
    
    try {
        // Convert input image to gradient domain format
        GradientsBase::imageType_t gradientImage;
        gradientImage = reinterpret_cast<const GradientsBase::imageType_t&>(image);
        
        // Configure HDR compression parameters
        double logMaxGradient = d_advanced->advancedSettings.logMaxGradient;
        double logMinGradient = d_advanced->advancedSettings.logMinGradient;
        double expGradient = d_advanced->advancedSettings.expGradient;
        bool rescale01 = d_advanced->advancedSettings.rescale01;
        
        emit gradientProgressUpdate("HDR compression extraction", 50, "Applying HDR compression");
        
        // Apply HDR compression
        GradientsBase::imageType_t compressedImage;
        d_advanced->gradientCompressor->hdrCompressionSetImage(gradientImage);
        d_advanced->gradientCompressor->hdrCompression(
            logMaxGradient, logMinGradient, expGradient, rescale01, compressedImage);
        
        // Convert result back to PCL DImage
        backgroundModel.Assign(reinterpret_cast<const pcl::DImage&>(compressedImage));
        
        emit gradientProgressUpdate("HDR compression extraction", 80, "Finalizing HDR background model");
        
        return true;
        
    } catch (const pcl::Error& e) {
        qDebug() << "PCL Error in HDR compression extraction:" << e.Message().c_str();
        return false;
    }
    
    return false;
}

bool HardenedBackgroundExtractor::processHybridExtraction(const pcl::Image& image, pcl::DImage& backgroundModel)
{
    emit gradientProgressUpdate("Hybrid extraction", 10, "Starting hybrid approach");
    
    // First try multiscale approach
    bool multiscaleSuccess = processMultiscaleExtraction(image, backgroundModel);
    
    if (multiscaleSuccess) {
        emit gradientProgressUpdate("Hybrid extraction", 60, "Refining with gradient domain");
        
        // Refine with gradient domain processing
        pcl::DImage refinedModel;
        bool gradientSuccess = processGradientDomainExtraction(image, refinedModel);
        
        if (gradientSuccess) {
            // Combine both results (simple average for now)
            for (int y = 0; y < backgroundModel.Height(); ++y) {
                for (int x = 0; x < backgroundModel.Width(); ++x) {
                    for (int c = 0; c < backgroundModel.NumberOfChannels(); ++c) {
                        double multiscaleValue = backgroundModel.Pixel(x, y, c);
                        double gradientValue = refinedModel.Pixel(x, y, c);
                        backgroundModel.Pixel(x, y, c) = (multiscaleValue + gradientValue) * 0.5;
                    }
                }
            }
        }
        
        emit gradientProgressUpdate("Hybrid extraction", 100, "Hybrid extraction complete");
        return true;
    }
    
    return false;
}

QVector<QPoint> HardenedBackgroundExtractor::generateGradientBasedSamples(const ImageData& imageData)
{
    return generateLowGradientSamples(imageData);
}

QVector<QPoint> HardenedBackgroundExtractor::generateLowGradientSamples(const ImageData& imageData)
{
    QVector<QPoint> samples;
    
    try {
        // Convert to PCL format for processing - FIXED METHOD CALL
        pcl::Image pclImage = convertToPCLImage(imageData);
        
        // Create gradient analysis to identify low-gradient regions
        pcl::DImage dxImage, dyImage;
        
        // Simple gradient calculation (Sobel-like)
        dxImage.AllocateData(pclImage.Width(), pclImage.Height(), pclImage.NumberOfChannels());
        dyImage.AllocateData(pclImage.Width(), pclImage.Height(), pclImage.NumberOfChannels());
        
        // Calculate gradients
        for (int c = 0; c < pclImage.NumberOfChannels(); ++c) {
            for (int y = 1; y < pclImage.Height() - 1; ++y) {
                for (int x = 1; x < pclImage.Width() - 1; ++x) {
                    // X gradient (Sobel)
                    double gx = pclImage.Pixel(x+1, y-1, c) + 2*pclImage.Pixel(x+1, y, c) + pclImage.Pixel(x+1, y+1, c) -
                               pclImage.Pixel(x-1, y-1, c) - 2*pclImage.Pixel(x-1, y, c) - pclImage.Pixel(x-1, y+1, c);
                    
                    // Y gradient (Sobel)
                    double gy = pclImage.Pixel(x-1, y+1, c) + 2*pclImage.Pixel(x, y+1, c) + pclImage.Pixel(x+1, y+1, c) -
                               pclImage.Pixel(x-1, y-1, c) - 2*pclImage.Pixel(x, y-1, c) - pclImage.Pixel(x+1, y-1, c);
                    
                    dxImage.Pixel(x, y, c) = gx / 8.0;
                    dyImage.Pixel(x, y, c) = gy / 8.0;
                }
            }
        }
        
        // Find low-gradient regions
        double gradientThreshold = d_advanced->advancedSettings.gradientThreshold;
        int sampleStep = std::max(8, std::min(imageData.width, imageData.height) / 32);
        
        for (int y = sampleStep; y < imageData.height - sampleStep; y += sampleStep) {
            for (int x = sampleStep; x < imageData.width - sampleStep; x += sampleStep) {
                
                // Calculate gradient magnitude at this point
                double gradientMagnitude = 0.0;
                for (int c = 0; c < pclImage.NumberOfChannels(); ++c) {
                    double gx = dxImage.Pixel(x, y, c);
                    double gy = dyImage.Pixel(x, y, c);
                    gradientMagnitude += std::sqrt(gx*gx + gy*gy);
                }
                gradientMagnitude /= pclImage.NumberOfChannels();
                
                if (gradientMagnitude < gradientThreshold) {
                    samples.append(QPoint(x, y));
                }
                
                if (samples.size() >= d_advanced->advancedSettings.maxSamples) {
                    break;
                }
            }
            if (samples.size() >= d_advanced->advancedSettings.maxSamples) {
                break;
            }
        }
        
    } catch (const pcl::Error& e) {
        qDebug() << "PCL Error in gradient sample generation:" << e.Message().c_str();
        // Fall back to adaptive grid sampling
        return generateAdaptiveGridSamples(imageData);
    }
    
    return samples;
}

QVector<QPoint> HardenedBackgroundExtractor::generateStructureAvoidingSamples(const ImageData& imageData)
{
    return generateLowGradientSamples(imageData); // Use gradient-based approach
}

QVector<QPoint> HardenedBackgroundExtractor::generateAdaptiveGridSamples(const ImageData& imageData)
{
    QVector<QPoint> samples;
    
    // Calculate image statistics to determine grid density
    double sum = 0.0;
    double sumSq = 0.0;
    int count = 0;
    
    // Sample every 10th pixel for statistics
    for (int y = 0; y < imageData.height; y += 10) {
        for (int x = 0; x < imageData.width; x += 10) {
            int index = y * imageData.width + x;
            if (index < imageData.pixels.size()) {
                double value = imageData.pixels[index];
                sum += value;
                sumSq += value * value;
                count++;
            }
        }
    }
    
    double mean = sum / count;
    double variance = (sumSq / count) - (mean * mean);
    double stdDev = std::sqrt(variance);
    
    // Adapt grid density based on image characteristics
    int baseGridSize = 16;
    if (variance > 0.01) {
        baseGridSize = 12;  // Denser grid for more complex images
    } else if (variance < 0.001) {
        baseGridSize = 24;  // Coarser grid for smooth images
    }
    
    int stepX = imageData.width / baseGridSize;
    int stepY = imageData.height / baseGridSize;
    
    stepX = std::max(4, stepX);
    stepY = std::max(4, stepY);
    
    // Generate grid samples with local adaptation
    for (int row = 1; row < baseGridSize - 1; ++row) {
        for (int col = 1; col < baseGridSize - 1; ++col) {
            int x = col * stepX;
            int y = row * stepY;
            
            if (x < imageData.width && y < imageData.height) {
                int index = y * imageData.width + x;
                if (index < imageData.pixels.size()) {
                    double pixelValue = imageData.pixels[index];
                    
                    // Only include samples that look like background
                    if (pixelValue < mean + 2.0 * stdDev) {
                        samples.append(QPoint(x, y));
                    }
                }
            }
        }
    }
    
    return samples;
}

double HardenedBackgroundExtractor::assessGradientBackgroundQuality(const QVector<float>& backgroundData, 
                                                                    const QVector<float>& originalData) const
{
    if (backgroundData.size() != originalData.size() || backgroundData.isEmpty()) {
        return 0.0;
    }
    
    // Calculate multiple quality metrics
    double mse = 0.0;
    double backgroundMean = 0.0;
    double originalMean = 0.0;
    
    // Basic statistics
    for (int i = 0; i < backgroundData.size(); ++i) {
        backgroundMean += backgroundData[i];
        originalMean += originalData[i];
    }
    backgroundMean /= backgroundData.size();
    originalMean /= originalData.size();
    
    // Mean squared error and smoothness metrics
    double smoothnessMetric = 0.0;
    int smoothnessCount = 0;
    
    for (int i = 0; i < backgroundData.size(); ++i) {
        double diff = backgroundData[i] - (originalData[i] * 0.1); // Expected background level
        mse += diff * diff;
        
        // Check local smoothness (assuming row-major layout)
        if (i > 0 && (i % 100) != 0) { // Avoid edge cases
            double smoothnessDiff = std::abs(backgroundData[i] - backgroundData[i-1]);
            smoothnessMetric += smoothnessDiff;
            smoothnessCount++;
        }
    }
    
    mse /= backgroundData.size();
    if (smoothnessCount > 0) {
        smoothnessMetric /= smoothnessCount;
    }
    
    // Combine metrics into quality score (0-1, higher is better)
    double rmse = std::sqrt(mse);
    double qualityScore = 1.0 / (1.0 + rmse + smoothnessMetric * 10.0);
    
    return std::max(0.0, std::min(1.0, qualityScore));
}

// Utility conversion methods - FIXED METHOD NAMES
pcl::Image HardenedBackgroundExtractor::convertToPCLImage(const ImageData& imageData)
{
    pcl::Image pclImage(imageData.width, imageData.height, imageData.channels);
    
    const float* src = imageData.pixels.constData();
    
    for (int c = 0; c < imageData.channels; ++c) {
        for (int y = 0; y < imageData.height; ++y) {
            for (int x = 0; x < imageData.width; ++x) {
                int srcIndex = (y * imageData.width + x) + c * imageData.width * imageData.height;
                pclImage.Pixel(x, y, c) = src[srcIndex];
            }
        }
    }
    
    return pclImage;
}

ImageData HardenedBackgroundExtractor::convertFromPCLImage(const pcl::Image& pclImage)
{
    ImageData imageData;
    imageData.width = pclImage.Width();
    imageData.height = pclImage.Height();
    imageData.channels = pclImage.NumberOfChannels();
    
    int totalPixels = imageData.width * imageData.height * imageData.channels;
    imageData.pixels.resize(totalPixels);
    
    float* dst = imageData.pixels.data();
    
    for (int c = 0; c < imageData.channels; ++c) {
        for (int y = 0; y < imageData.height; ++y) {
            for (int x = 0; x < imageData.width; ++x) {
                int dstIndex = (y * imageData.width + x) + c * imageData.width * imageData.height;
                dst[dstIndex] = static_cast<float>(pclImage.Pixel(x, y, c));
            }
        }
    }
    
    imageData.format = "PCL Converted";
    imageData.colorSpace = (imageData.channels == 1) ? "Grayscale" : "RGB";
    
    return imageData;
}

QVector<float> HardenedBackgroundExtractor::convertToQVector(const pcl::DImage& pclImage)
{
    int totalPixels = pclImage.Width() * pclImage.Height() * pclImage.NumberOfChannels();
    QVector<float> result(totalPixels);
    
    float* dst = result.data();
    
    for (int c = 0; c < pclImage.NumberOfChannels(); ++c) {
        for (int y = 0; y < pclImage.Height(); ++y) {
            for (int x = 0; x < pclImage.Width(); ++x) {
                int dstIndex = (y * pclImage.Width() + x) + c * pclImage.Width() * pclImage.Height();
                dst[dstIndex] = static_cast<float>(pclImage.Pixel(x, y, c));
            }
        }
    }
    
    return result;
}

// Preset configurations
HardenedBackgroundExtractor::AdvancedBackgroundSettings HardenedBackgroundExtractor::getDeepSkyGradientSettings()
{
    AdvancedBackgroundSettings settings;
    settings.advancedModel = AdvancedModel::GradientDomain;
    settings.advancedSampling = AdvancedSampling::LowGradientRegions;
    settings.tolerance = 1.0;
    settings.deviation = 0.8;
    settings.minSamples = 100;
    settings.maxSamples = 3000;
    settings.useOutlierRejection = true;
    settings.rejectionLow = 2.0;
    settings.rejectionHigh = 2.5;
    settings.rejectionIterations = 3;
    
    // Gradient domain parameters
    settings.gradientThreshold = 0.05;
    settings.gradientPercentile = 0.2;
    settings.useGradientMagnitude = true;
    
    // HDR compression parameters
    settings.logMaxGradient = 1.0;
    settings.logMinGradient = -2.0;
    settings.expGradient = 1.0;
    settings.rescale01 = true;
    settings.preserveColor = true;
    
    // Multiscale parameters
    settings.multiscaleLayers = 6;
    settings.multiscaleThreshold = 0.01;
    settings.useLinearMask = false;  // Better for nebulosity
    
    // Quality control
    settings.convergenceThreshold = 0.001;
    settings.maxIterations = 50;
    settings.enableStructureProtection = true;
    settings.structureProtectionThreshold = 0.05;
    settings.useRobustEstimation = true;
    settings.robustThreshold = 2.0;
    
    return settings;
}

HardenedBackgroundExtractor::AdvancedBackgroundSettings HardenedBackgroundExtractor::getWidefieldGradientSettings()
{
    AdvancedBackgroundSettings settings;
    settings.advancedModel = AdvancedModel::MultiscaleMedian;
    settings.advancedSampling = AdvancedSampling::AdaptiveThreshold;
    settings.tolerance = 1.5;
    settings.deviation = 1.0;
    settings.minSamples = 200;
    settings.maxSamples = 5000;
    settings.useOutlierRejection = true;
    settings.rejectionLow = 1.5;
    settings.rejectionHigh = 2.0;
    settings.rejectionIterations = 5;
    
    // HDR-specific parameters
    settings.logMaxGradient = 2.0;
    settings.logMinGradient = -3.0;
    settings.expGradient = 1.2;
    settings.rescale01 = true;
    settings.preserveColor = false;  // Allow color changes for HDR
    
    // Advanced parameters for HDR processing
    settings.multiscaleLayers = 5;
    settings.multiscaleThreshold = 0.02;
    settings.useLinearMask = true;
    settings.gradientThreshold = 0.08;
    settings.useRobustEstimation = true;
    settings.robustThreshold = 1.5;  // More aggressive for HDR
    settings.robustIterations = 7;
    settings.enableStructureProtection = true;
    settings.structureProtectionThreshold = 0.08;
    
    return settings;
}

// Helper methods
double HardenedBackgroundExtractor::calculateRMSError(const QVector<float>& backgroundData, 
                                                      const QVector<float>& originalData) const
{
    if (backgroundData.size() != originalData.size() || backgroundData.isEmpty()) {
        return 1.0;
    }
    
    double sumSquaredError = 0.0;
    for (int i = 0; i < backgroundData.size(); ++i) {
        double error = backgroundData[i] - (originalData[i] * 0.1); // Expected background level
        sumSquaredError += error * error;
    }
    
    return std::sqrt(sumSquaredError / backgroundData.size());
}

// Advanced worker thread implementation
AdvancedGradientWorker::AdvancedGradientWorker(const ImageData& imageData,
                                               const HardenedBackgroundExtractor::AdvancedBackgroundSettings& settings,
                                               QObject* parent)
    : BackgroundExtractionWorker(imageData, static_cast<BackgroundExtractionSettings>(settings), parent)
    , m_advancedSettings(settings)
{
    // Initialize PCL objects
    m_gradientBase = std::make_unique<GradientsBase>();
}

void AdvancedGradientWorker::run()
{
    QTime timer;
    timer.start();
    
    m_result = BackgroundExtractionResult(); // Clear result
    
    try {
        if (!performGradientDomainExtraction()) {
            m_result.success = false;
            m_result.errorMessage = "Gradient domain extraction failed";
            emit finished(m_result);
            return;
        }
        
        m_result.success = true;
        m_result.processingTimeSeconds = timer.elapsed() / 1000.0;
        emit finished(m_result);
        
    } catch (const std::exception& e) {
        m_result.success = false;
        m_result.errorMessage = QString("Advanced extraction error: %1").arg(e.what());
        emit finished(m_result);
    } catch (...) {
        m_result.success = false;
        m_result.errorMessage = "Unknown advanced extraction error";
        emit finished(m_result);
    }
}

bool AdvancedGradientWorker::performGradientDomainExtraction()
{
    if (m_cancelled) return false;
    
    emit gradientProgress("Advanced extraction starting", 5, "Initializing gradient components");
    
    // Choose extraction method based on advanced model
    switch (m_advancedSettings.advancedModel) {
    case HardenedBackgroundExtractor::AdvancedModel::GradientDomain:
        return executeGradientAnalysis();
    case HardenedBackgroundExtractor::AdvancedModel::HDRCompression:
        return executeHDRCompressionMethod();
    case HardenedBackgroundExtractor::AdvancedModel::MultiscaleMedian:
        return executeMultiscaleMethod();
    case HardenedBackgroundExtractor::AdvancedModel::HybridGradient:
        return executeHybridGradientMethod();
    default:
        // Fall back to base class
        return BackgroundExtractionWorker::performExtraction();
    }
}

bool AdvancedGradientWorker::executeGradientAnalysis()
{
    emit gradientProgress("Gradient analysis", 10, "Converting to gradient domain");
    
    try {
        // Convert image to gradient domain format
        GradientsBase::imageType_t workingImage;
        workingImage = HardenedBackgroundExtractor::convertToGradientImage(m_imageData);
        
        emit gradientProgress("Gradient analysis", 30, "Computing gradient fields");
        
        // Compute gradient fields using the gradient base
        if (!computeGradientFields()) {
            return false;
        }
        
        emit gradientProgress("Gradient analysis", 60, "Analyzing gradient statistics");
        
        if (!analyzeGradientStatistics()) {
            return false;
        }
        
        emit gradientProgress("Gradient analysis", 80, "Fitting gradient domain model");
        
        if (!fitGradientDomainModel()) {
            return false;
        }
        
        emit gradientProgress("Gradient analysis", 100, "Gradient analysis complete");
        return true;
        
    } catch (const std::exception& e) {
        m_result.errorMessage = QString("Gradient analysis failed: %1").arg(e.what());
        return false;
    }
}

bool AdvancedGradientWorker::executeHDRCompressionMethod()
{
    emit gradientProgress("HDR compression method", 10, "Setting up HDR compression");
    
    try {
        // Initialize HDR compressor if not already done
        if (!m_hdrCompressor) {
            m_hdrCompressor = std::make_unique<GradientsHdrCompression>();
        }
        
        // Convert to gradient image format
        GradientsBase::imageType_t inputImage = HardenedBackgroundExtractor::convertToGradientImage(m_imageData);
        
        emit gradientProgress("HDR compression method", 40, "Applying HDR compression");
        
        // Set up HDR compression parameters
        m_hdrCompressor->hdrCompressionSetImage(inputImage);
        
        // Apply HDR compression
        GradientsBase::imageType_t compressedImage;
        m_hdrCompressor->hdrCompression(
            m_advancedSettings.logMaxGradient,
            m_advancedSettings.logMinGradient,
            m_advancedSettings.expGradient,
            m_advancedSettings.rescale01,
            compressedImage
        );
        
        emit gradientProgress("HDR compression method", 80, "Converting results");
        
        // Convert back to our format
        ImageData resultData = HardenedBackgroundExtractor::convertFromGradientImage(compressedImage);
        m_result.backgroundData = resultData.pixels;
        
        // Create corrected image
        m_result.correctedData = m_imageData.pixels;
        for (int i = 0; i < m_result.correctedData.size() && i < m_result.backgroundData.size(); ++i) {
            m_result.correctedData[i] -= m_result.backgroundData[i];
        }
        
        emit gradientProgress("HDR compression method", 100, "HDR compression complete");
        return true;
        
    } catch (const std::exception& e) {
        m_result.errorMessage = QString("HDR compression failed: %1").arg(e.what());
        return false;
    }
}

bool AdvancedGradientWorker::executeMultiscaleMethod()
{
    emit gradientProgress("Multiscale method", 10, "Setting up multiscale transform");
    
    // This would use the multiscale median transform
    // For now, delegate to base class implementation
    return BackgroundExtractionWorker::performExtraction();
}

bool AdvancedGradientWorker::executeHybridGradientMethod()
{
    emit gradientProgress("Hybrid gradient method", 10, "Combining multiple methods");
    
    // Try gradient analysis first
    bool gradientSuccess = executeGradientAnalysis();
    
    if (gradientSuccess && !m_cancelled) {
        emit gradientProgress("Hybrid gradient method", 60, "Refining with HDR compression");
        // Could refine the gradient result with HDR compression
        // For now, just use the gradient result
    }
    
    emit gradientProgress("Hybrid gradient method", 100, "Hybrid gradient extraction complete");
    return gradientSuccess;
}

bool AdvancedGradientWorker::computeGradientFields()
{
    try {
        // Convert image data to working format
        int width = m_imageData.width;
        int height = m_imageData.height;
        
        m_workingImage.resize(height);
        for (int y = 0; y < height; ++y) {
            m_workingImage[y].resize(width);
            for (int x = 0; x < width; ++x) {
                int index = y * width + x;
                if (index < m_imageData.pixels.size()) {
                    m_workingImage[y][x] = m_imageData.pixels[index];
                } else {
                    m_workingImage[y][x] = 0.0;
                }
            }
        }
        
        // Initialize gradient images
        m_dxImage.resize(height);
        m_dyImage.resize(height);
        for (int y = 0; y < height; ++y) {
            m_dxImage[y].resize(width);
            m_dyImage[y].resize(width);
        }
        
        // Compute gradients using simple finite differences
        for (int y = 1; y < height - 1; ++y) {
            for (int x = 1; x < width - 1; ++x) {
                // X gradient
                m_dxImage[y][x] = (m_workingImage[y][x+1] - m_workingImage[y][x-1]) * 0.5;
                // Y gradient  
                m_dyImage[y][x] = (m_workingImage[y+1][x] - m_workingImage[y-1][x]) * 0.5;
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        m_result.errorMessage = QString("Gradient field computation failed: %1").arg(e.what());
        return false;
    }
}

bool AdvancedGradientWorker::analyzeGradientStatistics()
{
    try {
        // Calculate gradient statistics
        double sumGradMag = 0.0;
        double sumGradMagSq = 0.0;
        int count = 0;
        
        for (int y = 1; y < m_imageData.height - 1; ++y) {
            for (int x = 1; x < m_imageData.width - 1; ++x) {
                double gx = m_dxImage[y][x];
                double gy = m_dyImage[y][x];
                double gradMag = std::sqrt(gx*gx + gy*gy);
                
                sumGradMag += gradMag;
                sumGradMagSq += gradMag * gradMag;
                count++;
            }
        }
        
        m_gradientMean = sumGradMag / count;
        m_gradientStdDev = std::sqrt((sumGradMagSq / count) - (m_gradientMean * m_gradientMean));
        m_gradientThreshold = m_gradientMean + m_advancedSettings.gradientPercentile * m_gradientStdDev;
        
        return true;
        
    } catch (const std::exception& e) {
        m_result.errorMessage = QString("Gradient statistics analysis failed: %1").arg(e.what());
        return false;
    }
}

bool AdvancedGradientWorker::selectGradientBasedSamples()
{
    // This would select sample points based on gradient analysis
    // For now, just select low-gradient regions
    return true;
}

bool AdvancedGradientWorker::fitGradientDomainModel()
{
    try {
        // Create background model based on gradient analysis
        int totalPixels = m_imageData.width * m_imageData.height * m_imageData.channels;
        m_result.backgroundData.resize(totalPixels);
        
        // Simple approach: smooth the low-gradient regions
        for (int c = 0; c < m_imageData.channels; ++c) {
            for (int y = 0; y < m_imageData.height; ++y) {
                for (int x = 0; x < m_imageData.width; ++x) {
                    int index = (y * m_imageData.width + x) + c * m_imageData.width * m_imageData.height;
                    
                    if (y > 0 && y < m_imageData.height - 1 && x > 0 && x < m_imageData.width - 1) {
                        // Calculate gradient magnitude
                        double gx = m_dxImage[y][x];
                        double gy = m_dyImage[y][x];
                        double gradMag = std::sqrt(gx*gx + gy*gy);
                        
                        if (gradMag < m_gradientThreshold) {
                            // Low gradient - use local average
                            double sum = 0.0;
                            int count = 0;
                            for (int dy = -1; dy <= 1; ++dy) {
                                for (int dx = -1; dx <= 1; ++dx) {
                                    int nx = x + dx;
                                    int ny = y + dy;
                                    if (nx >= 0 && nx < m_imageData.width && ny >= 0 && ny < m_imageData.height) {
                                        int nindex = (ny * m_imageData.width + nx) + c * m_imageData.width * m_imageData.height;
                                        if (nindex < m_imageData.pixels.size()) {
                                            sum += m_imageData.pixels[nindex];
                                            count++;
                                        }
                                    }
                                }
                            }
                            m_result.backgroundData[index] = sum / count;
                        } else {
                            // High gradient - interpolate from nearby low-gradient regions
                            m_result.backgroundData[index] = m_imageData.pixels[index] * 0.5; // Simple fallback
                        }
                    } else {
                        // Edge pixels
                        m_result.backgroundData[index] = m_imageData.pixels[index] * 0.5;
                    }
                }
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        m_result.errorMessage = QString("Gradient domain model fitting failed: %1").arg(e.what());
        return false;
    }
}

// Static utility methods for HardenedBackgroundExtractor
GradientsBase::imageType_t HardenedBackgroundExtractor::convertToGradientImage(const ImageData& imageData)
{
    // Convert ImageData to GradientsBase::imageType_t
    // This is a simplified conversion - in practice you'd need to match the exact type
    GradientsBase::imageType_t result;
    
    // Initialize with the right dimensions
    result.resize(imageData.height);
    for (int y = 0; y < imageData.height; ++y) {
        result[y].resize(imageData.width);
        for (int x = 0; x < imageData.width; ++x) {
            int index = y * imageData.width + x;
            if (index < imageData.pixels.size()) {
                result[y][x] = imageData.pixels[index];
            } else {
                result[y][x] = 0.0;
            }
        }
    }
    
    return result;
}

ImageData HardenedBackgroundExtractor::convertFromGradientImage(const GradientsBase::imageType_t& gradientImage)
{
    ImageData result;
    
    if (gradientImage.empty()) {
        return result;
    }
    
    result.height = gradientImage.size();
    result.width = gradientImage[0].size();
    result.channels = 1; // Assuming single channel from gradient processing
    
    int totalPixels = result.width * result.height;
    result.pixels.resize(totalPixels);
    
    for (int y = 0; y < result.height; ++y) {
        for (int x = 0; x < result.width; ++x) {
            int index = y * result.width + x;
            result.pixels[index] = static_cast<float>(gradientImage[y][x]);
        }
    }
    
    result.format = "Gradient Domain";
    result.colorSpace = "Grayscale";
    
    return result;
}

QVector<float> HardenedBackgroundExtractor::convertGradientToQVector(const GradientsBase::imageType_t& gradientImage)
{
    QVector<float> result;
    
    if (gradientImage.empty()) {
        return result;
    }
    
    int height = gradientImage.size();
    int width = gradientImage[0].size();
    result.resize(width * height);
    
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int index = y * width + x;
            result[index] = static_cast<float>(gradientImage[y][x]);
        }
    }
    
    return result;
}
