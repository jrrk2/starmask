#include "StarMaskGenerator.h"
#include "StarCorrelator.h"
#include "PCLMockAPI.h"

#include <pcl/Image.h>
#include <pcl/StarDetector.h>
#include <pcl/api/APIInterface.h>

#include <algorithm>
#include <cmath>
#include <QDebug>

StarCorrelator correlator;

void StarMaskGenerator::validateStarDetection() {
    
    // Run analysis
    correlator.correlateStars();
    correlator.printDetailedStatistics();
    correlator.printMatchDetails();
    correlator.analyzePhotometricAccuracy();
    correlator.exportMatches("star_matches.csv");
    
    // Get results for your UI
    int matchCount = correlator.getMatchCount();
    double avgError = correlator.getAverageError();
    double matchRate = correlator.getMatchRate();
    
    qDebug() << "Validation complete:" << matchCount << "matches," 
             << QString::number(avgError, 'f', 2) << "px avg error,"
             << QString::number(matchRate, 'f', 1) << "% match rate";
}

StarMaskResult StarMaskGenerator::detectStars(const ImageData& imageData, float threshold)
{
    StarMaskResult result;

    if (!imageData.isValid()) {
        qDebug() << "Invalid image data for star detection";
        return result;
    }

    // Configure Validator
    correlator.setImageDimensions(imageData.width, imageData.height);
    correlator.setMatchThreshold(2.0);
    correlator.setZeroPoint(25.0);
    correlator.setAutoCalibrate(true);
    
    try {
        // Initialize PCL Mock API
        pcl_mock::InitializeMockAPI();
        if (!API) {
            API = new pcl::APIInterface(pcl_mock::GetMockFunctionResolver());
        }

        qDebug() << "Using PCL StarDetector for star detection";

        // Create PCL Image from our ImageData
        pcl::Image pclImage(imageData.width, imageData.height, 
                           imageData.channels == 1 ? pcl::ColorSpace::Gray : pcl::ColorSpace::RGB);

        // Copy pixel data to PCL Image
        // ImageData stores pixels as [channel0_pixels, channel1_pixels, channel2_pixels]
        const float* src = imageData.pixels.constData();
        
        for (int c = 0; c < imageData.channels; ++c) {
            for (int y = 0; y < imageData.height; ++y) {
                for (int x = 0; x < imageData.width; ++x) {
                    int srcIndex = c * imageData.width * imageData.height + y * imageData.width + x;
                    pclImage.Pixel(x, y, c) = src[srcIndex];
                }
            }
        }

        qDebug() << "Created PCL Image:" << imageData.width << "x" << imageData.height 
                 << "channels:" << imageData.channels;

        // Create and configure StarDetector
        pcl::StarDetector detector;
        
        // Configure detection parameters based on the threshold parameter
        // The threshold parameter (0.0-1.0) maps to sensitivity
        detector.SetSensitivity(threshold);
        
        // Set reasonable defaults for astronomical imaging
        detector.SetStructureLayers(5);          // Detect structures up to ~32 pixels
        detector.SetNoiseLayers(1);              // Light noise reduction
        detector.SetHotPixelFilterRadius(1);     // Remove hot pixels
        detector.SetMinStructureSize(0);         // Automatic minimum size
        detector.SetPeakResponse(0.5f);          // Standard peak response
        detector.SetMinSNR(0.0f);               // No SNR limit
        detector.SetBrightThreshold(3.0f);       // Include bright stars
        detector.SetMaxDistortion(0.8f);         // Allow slightly elongated stars
        detector.DisableClusteredSources();      // Prevent multiple detections
        detector.SetLocalDetectionFilterRadius(2); // 5x5 local maxima filter
        detector.SetUpperLimit(1.0f);           // No upper limit
        detector.DisableImageInversion();        // Normal bright stars
        
        // Enable PSF fitting for better centroid accuracy (optional)
        detector.EnablePSFFitting(true);
        detector.SetPSFType(pcl::PSFunction::Gaussian);
        detector.DisableEllipticPSF();          // Use circular PSF for simplicity
        detector.SetPSFCentroidTolerance(2.0f); // Allow 2 pixel centroid deviation

        qDebug() << "StarDetector configured with sensitivity:" << threshold;

        // Perform star detection
        pcl::ImageVariant imageVariant(&pclImage);  // Convert to ImageVariant
        pcl::StarDetector::star_list stars = detector.DetectStars(imageVariant);

        qDebug() << "PCL StarDetector found" << stars.Length() << "stars";

        // Create mask image
        result.maskImage = QImage(imageData.width, imageData.height, QImage::Format_Grayscale8);
        result.maskImage.fill(0);

        // Convert PCL stars to our format
        result.starCenters.reserve(stars.Length());
        result.starRadii.reserve(stars.Length());
        result.starValid.reserve(stars.Length());

        // Calculate image statistics for radius estimation
        double maxFlux = 0.0;
        for (const auto& star : stars) {
            maxFlux = std::max(maxFlux, static_cast<double>(star.flux));
        }

        for (const auto& star : stars) {
            // Convert star position to QPoint (PCL uses double, we use int for centers)
            QPoint center(static_cast<int>(std::round(star.pos.x)), 
                         static_cast<int>(std::round(star.pos.y)));

            // Estimate star radius from the detection area or flux
            float starRadius;
            if (star.area > 0) {
                // Use actual detected area to estimate radius
                starRadius = std::sqrt(star.area / M_PI);
            } else {
                // Fallback: estimate from flux (normalized to image brightness)
                double relativeFlux = star.flux / maxFlux;
                starRadius = 2.0f + relativeFlux * 4.0f; // 2-6 pixel radius range
            }
            
            // Clamp radius to reasonable bounds
            starRadius = std::max(1.0f, std::min(starRadius, 12.0f));

            result.starCenters.append(center);
            result.starRadii.append(starRadius);
            result.starValid.append(true);

            // Draw star in mask using the detection rectangle if available
            if (star.rect.IsRect()) {
                // Use the actual detection rectangle
                for (int y = star.rect.y0; y < star.rect.y1; ++y) {
                    for (int x = star.rect.x0; x < star.rect.x1; ++x) {
                        if (x >= 0 && y >= 0 && x < imageData.width && y < imageData.height) {
                            result.maskImage.setPixel(x, y, 255);
                        }
                    }
                }
            } else {
                // Fallback: draw circular region around center
                int r = static_cast<int>(std::ceil(starRadius));
                for (int dy = -r; dy <= r; ++dy) {
                    for (int dx = -r; dx <= r; ++dx) {
                        int px = center.x() + dx;
                        int py = center.y() + dy;
                        if (px >= 0 && py >= 0 && px < imageData.width && py < imageData.height) {
                            if (dx * dx + dy * dy <= r * r) {
                                result.maskImage.setPixel(px, py, 255);
                            }
                        }
                    }
                }
            }

	    correlator.addDetectedStar(result.starCenters.size(),
				       star.pos.x,
				       star.pos.y,
				       star.flux,
				       star.area,
				       starRadius,
				       ((star.flux - 0) / std::max(1.0f, star.mad))); // Rough SNR estimate
            }

        // Report automatically calculated minimum star size if available
        if (detector.MinStarSize() > 0) {
            qDebug() << "Automatically calculated minimum star size:" << detector.MinStarSize() << "pixels";
        }

        qDebug() << "Star detection completed:" << result.starCenters.size() << "stars detected";
        qDebug() << "Detection parameters used:";
        qDebug() << "  - Sensitivity:" << detector.Sensitivity();
        qDebug() << "  - Structure layers:" << detector.StructureLayers();
        qDebug() << "  - Noise layers:" << detector.NoiseLayers();
        qDebug() << "  - Hot pixel filter:" << detector.HotPixelFilterRadius();
        qDebug() << "  - Min structure size:" << detector.MinStructureSize() 
                 << "(auto calculated:" << detector.MinStarSize() << ")";
        qDebug() << "  - PSF fitting:" << (detector.IsPSFFittingEnabled() ? "enabled" : "disabled");

    } catch (const pcl::Error& e) {
        qDebug() << "PCL Error in star detection:" << e.Message().c_str();
        qDebug() << "Falling back to simple star detection algorithm";
        
        // Fallback to your original simple algorithm
        return detectStarsSimple(imageData, threshold);
        
    } catch (const std::exception& e) {
        qDebug() << "Standard error in star detection:" << e.what();
        return detectStarsSimple(imageData, threshold);
        
    } catch (...) {
        qDebug() << "Unknown error in PCL star detection";
        return detectStarsSimple(imageData, threshold);
    }

    return result;
}

void StarMaskGenerator::dumpcat(QVector<CatalogStar> &catalogStars)
{
  for (const auto& star : catalogStars) {
    correlator.addCatalogStar(star.id,
			      star.pixelPos.x(),
			      star.pixelPos.y(),
			      star.magnitude);
  }
  validateStarDetection();
}

// Fallback simple star detection (your original algorithm)
StarMaskResult StarMaskGenerator::detectStarsSimple(const ImageData& imageData, float threshold)
{
    StarMaskResult result;
    
    qDebug() << "Using fallback simple star detection algorithm";

    // Use first channel only for star detection
    int width = imageData.width;
    int height = imageData.height;
    const float* pixels = imageData.pixels.constData();

    // Simple star detection algorithm
    // Find local maxima above threshold
    std::vector<float> localMaxima;
    std::vector<QPoint> candidates;

    // Calculate image statistics for adaptive threshold
    float sum = 0.0f;
    float sumSq = 0.0f;
    int count = 0;

    for (int i = 0; i < width * height; ++i) {
        float val = pixels[i];
        sum += val;
        sumSq += val * val;
        count++;
    }

    float mean = sum / count;
    float variance = (sumSq / count) - (mean * mean);
    float stddev = std::sqrt(variance);
    
    // Adaptive threshold based on image statistics
    float detectionThreshold = mean + threshold * stddev * 5.0f;
    
    qDebug() << "Simple detection: mean=" << mean << "stddev=" << stddev << "threshold=" << detectionThreshold;

    // Find local maxima
    int radius = 3; // Search radius for local maxima
    for (int y = radius; y < height - radius; y += 2) { // Skip pixels for speed
        for (int x = radius; x < width - radius; x += 2) {
            int centerIdx = y * width + x;
            float centerVal = pixels[centerIdx];
            
            if (centerVal < detectionThreshold) continue;
            
            // Check if this is a local maximum
            bool isLocalMax = true;
            for (int dy = -radius; dy <= radius && isLocalMax; ++dy) {
                for (int dx = -radius; dx <= radius && isLocalMax; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    
                    int checkIdx = (y + dy) * width + (x + dx);
                    if (pixels[checkIdx] >= centerVal) {
                        isLocalMax = false;
                    }
                }
            }
            
            if (isLocalMax) {
                candidates.push_back(QPoint(x, y));
                localMaxima.push_back(centerVal);
            }
        }
    }

    qDebug() << "Simple detection found" << candidates.size() << "star candidates";

    // Sort by brightness and take the brightest ones
    std::vector<int> indices(candidates.size());
    std::iota(indices.begin(), indices.end(), 0);
    
    std::sort(indices.begin(), indices.end(), [&](int a, int b) {
        return localMaxima[a] > localMaxima[b];
    });

    // Limit to reasonable number of stars
    int maxStars = std::min(500, static_cast<int>(candidates.size()));
    
    // Create mask image
    result.maskImage = QImage(width, height, QImage::Format_Grayscale8);
    result.maskImage.fill(0);

    // Process the brightest candidates
    for (int i = 0; i < maxStars; ++i) {
        int idx = indices[i];
        QPoint center = candidates[idx];
        float brightness = localMaxima[idx];
        
        // Estimate star radius based on brightness
        float starRadius = 2.0f + (brightness - mean) / stddev * 0.5f;
        starRadius = std::max(1.0f, std::min(starRadius, 8.0f));
        
        result.starCenters.append(center);
        result.starRadii.append(starRadius);
        result.starValid.append(true);

        // Draw star in mask
        int r = static_cast<int>(std::ceil(starRadius));
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                int px = center.x() + dx;
                int py = center.y() + dy;
                if (px >= 0 && py >= 0 && px < width && py < height) {
                    if (dx * dx + dy * dy <= r * r) {
                        result.maskImage.setPixel(px, py, 255);
                    }
                }
            }
        }
    }

    qDebug() << "Simple star detection completed:" << result.starCenters.size() << "stars detected";
    return result;
}

StarMaskResult StarMaskGenerator::detectStarsAdvanced(const ImageData& imageData, 
                                                     float sensitivity,
                                                     int structureLayers,
                                                     int noiseLayers,
                                                     float peakResponse,
                                                     float maxDistortion,
                                                     bool enablePSFFitting)
{
    StarMaskResult result;

    if (!imageData.isValid()) {
        qDebug() << "Invalid image data for advanced star detection";
        return result;
    }

    try {
        // Initialize PCL Mock API
        pcl_mock::InitializeMockAPI();
        if (!API) {
            API = new pcl::APIInterface(pcl_mock::GetMockFunctionResolver());
        }

        qDebug() << "Using PCL StarDetector (Advanced Mode)";

        // Create PCL Image from our ImageData
        pcl::Image pclImage(imageData.width, imageData.height, 
                           imageData.channels == 1 ? pcl::ColorSpace::Gray : pcl::ColorSpace::RGB);

        // Copy pixel data to PCL Image
        const float* src = imageData.pixels.constData();
        
        for (int c = 0; c < imageData.channels; ++c) {
            for (int y = 0; y < imageData.height; ++y) {
                for (int x = 0; x < imageData.width; ++x) {
                    int srcIndex = c * imageData.width * imageData.height + y * imageData.width + x;
                    pclImage.Pixel(x, y, c) = src[srcIndex];
                }
            }
        }

        // Create and configure StarDetector with advanced parameters
        pcl::StarDetector detector;
        
        // Core detection parameters
        detector.SetSensitivity(sensitivity);
        detector.SetStructureLayers(structureLayers);
        detector.SetNoiseLayers(noiseLayers);
        detector.SetPeakResponse(peakResponse);
        detector.SetMaxDistortion(maxDistortion);
        
        // Noise reduction and filtering
        detector.SetHotPixelFilterRadius(1);      // Always remove hot pixels
        detector.SetNoiseReductionFilterRadius(0); // No additional noise reduction by default
        
        // Structure detection parameters
        detector.SetMinStructureSize(0);          // Automatic minimum size
        detector.SetMinSNR(0.0f);                // No SNR limit by default
        detector.SetBrightThreshold(3.0f);       // Standard bright star threshold
        
        // Local maxima detection
        detector.DisableClusteredSources();      // Prevent overlapping detections
        detector.SetLocalDetectionFilterRadius(2); // 5x5 local maxima filter
        detector.SetLocalMaximaDetectionLimit(0.75f);
        detector.EnableLocalMaximaDetection();
        
        // Detection limits
        detector.SetUpperLimit(1.0f);           // No upper limit
        detector.DisableImageInversion();       // Normal bright stars on dark background
        
        // PSF fitting configuration
        if (enablePSFFitting) {
            detector.EnablePSFFitting();
            detector.SetPSFType(pcl::PSFunction::Gaussian);  // Gaussian PSF
            detector.DisableEllipticPSF();       // Use circular PSF
            detector.SetPSFCentroidTolerance(2.0f); // Allow 2 pixel deviation
        } else {
            detector.DisablePSFFitting();
        }

        qDebug() << "Advanced StarDetector configured:";
        qDebug() << "  - Sensitivity:" << sensitivity;
        qDebug() << "  - Structure layers:" << structureLayers;
        qDebug() << "  - Noise layers:" << noiseLayers;
        qDebug() << "  - Peak response:" << peakResponse;
        qDebug() << "  - Max distortion:" << maxDistortion;
        qDebug() << "  - PSF fitting:" << (enablePSFFitting ? "enabled" : "disabled");

        // Perform star detection
        pcl::ImageVariant imageVariant(&pclImage);  // Convert to ImageVariant
        pcl::StarDetector::star_list stars = detector.DetectStars(imageVariant);

        qDebug() << "Advanced PCL StarDetector found" << stars.Length() << "stars";

        // Create mask image
        result.maskImage = QImage(imageData.width, imageData.height, QImage::Format_Grayscale8);
        result.maskImage.fill(0);

        // Convert PCL stars to our format with enhanced information
        result.starCenters.reserve(stars.Length());
        result.starRadii.reserve(stars.Length());
        result.starValid.reserve(stars.Length());

        // Sort stars by flux (brightest first) - PCL already does this but let's be sure
        pcl::StarDetector::star_list sortedStars = stars;
        sortedStars.Sort();

        // Calculate statistics for radius estimation
        double maxFlux = 0.0, minFlux = 1e10;
        double maxArea = 0.0;
        for (const auto& star : sortedStars) {
            maxFlux = std::max(maxFlux, static_cast<double>(star.flux));
            minFlux = std::min(minFlux, static_cast<double>(star.flux));
            maxArea = std::max(maxArea, static_cast<double>(star.area));
        }

        int validStars = 0;
        for (const auto& star : sortedStars) {
            // Convert star position to QPoint 
            QPoint center(static_cast<int>(std::round(star.pos.x)), 
                         static_cast<int>(std::round(star.pos.y)));

            // Validate star position is within image bounds
            if (center.x() < 0 || center.y() < 0 || 
                center.x() >= imageData.width || center.y() >= imageData.height) {
                continue; // Skip stars outside image bounds
            }

            // Estimate star radius using multiple factors
            float starRadius = 2.0f; // Default minimum radius
            
            if (star.area > 0) {
                // Primary: Use actual detected area
                starRadius = std::sqrt(star.area / M_PI);
            } else if (maxFlux > minFlux) {
                // Fallback: Estimate from relative flux
                double relativeFlux = (star.flux - minFlux) / (maxFlux - minFlux);
                starRadius = 1.5f + relativeFlux * 6.0f; // 1.5-7.5 pixel radius range
            }
            
            // If we have a bounding rectangle, use that for additional validation
            if (star.rect.IsRect()) {
                float rectRadius = std::max(star.rect.Width(), star.rect.Height()) / 2.0f;
                // Use the larger of area-based or rectangle-based radius
                starRadius = std::max(starRadius, rectRadius);
            }
            
            // Clamp radius to reasonable astronomical bounds
            starRadius = std::max(0.8f, std::min(starRadius, 15.0f));

            result.starCenters.append(center);
            result.starRadii.append(starRadius);
            result.starValid.append(true);

            // Draw star in mask using detection rectangle if available
            if (star.rect.IsRect()) {
                // Use the actual detection rectangle
                for (int y = star.rect.y0; y < star.rect.y1; ++y) {
                    for (int x = star.rect.x0; x < star.rect.x1; ++x) {
                        if (x >= 0 && y >= 0 && x < imageData.width && y < imageData.height) {
                            result.maskImage.setPixel(x, y, 255);
                        }
                    }
                }
            } else {
                // Fallback: draw circular region
                int r = static_cast<int>(std::ceil(starRadius));
                for (int dy = -r; dy <= r; ++dy) {
                    for (int dx = -r; dx <= r; ++dx) {
                        int px = center.x() + dx;
                        int py = center.y() + dy;
                        if (px >= 0 && py >= 0 && px < imageData.width && py < imageData.height) {
                            if (dx * dx + dy * dy <= r * r) {
                                result.maskImage.setPixel(px, py, 255);
                            }
                        }
                    }
                }
            }

            // Debug output for first few stars
            if (validStars < 5) {
                QString psfInfo = "";
                if (enablePSFFitting) {
                    psfInfo = QString(" signal=%1 mad=%2").arg(star.signal, 0, 'f', 2).arg(star.mad, 0, 'f', 3);
                }
                
                qDebug() << QString("Star %1: pos=(%2,%3) flux=%4 area=%5 radius=%6%7")
                            .arg(validStars + 1)
                            .arg(star.pos.x, 0, 'f', 2)
                            .arg(star.pos.y, 0, 'f', 2)
                            .arg(star.flux, 0, 'f', 1)
                            .arg(star.area, 0, 'f', 1)
                            .arg(starRadius, 0, 'f', 1)
                            .arg(psfInfo);
                
                if (star.rect.IsRect()) {
                    qDebug() << QString("    rect=(%1,%2,%3,%4)")
                                .arg(star.rect.x0).arg(star.rect.y0)
                                .arg(star.rect.x1).arg(star.rect.y1);
                }
            }
            
            validStars++;
        }

        // Report detection statistics
        if (detector.MinStarSize() > 0) {
            qDebug() << "Automatically calculated minimum star size:" << detector.MinStarSize() << "pixels";
        }

        qDebug() << "Advanced star detection completed:" << validStars << "valid stars";
        qDebug() << "Flux range:" << minFlux << "to" << maxFlux;
        qDebug() << "Max detected area:" << maxArea << "pixels";

    } catch (const pcl::Error& e) {
        qDebug() << "PCL Error in advanced star detection:" << e.Message().c_str();
        qDebug() << "Falling back to simple detection";
        return detectStarsSimple(imageData, sensitivity);
        
    } catch (const std::exception& e) {
        qDebug() << "Standard error in advanced star detection:" << e.what();
        return detectStarsSimple(imageData, sensitivity);
        
    } catch (...) {
        qDebug() << "Unknown error in advanced PCL star detection";
        return detectStarsSimple(imageData, sensitivity);
    }

    return result;
}
