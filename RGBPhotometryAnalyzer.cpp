#include "RGBPhotometryAnalyzer.h"

// RGBPhotometryAnalyzer.cpp Implementation
RGBPhotometryAnalyzer::RGBPhotometryAnalyzer(QObject *parent)
    : QObject(parent)
    , m_apertureRadius(8.0)
    , m_bgInnerRadius(12.0) 
    , m_bgOuterRadius(20.0)
    , m_colorIndexType("B-V")
{
}

bool RGBPhotometryAnalyzer::analyzeStarColors(const ImageData* imageData, 
                                             const QVector<QPoint>& starCenters,
                                             const QVector<float>& starRadii)
{
    if (imageData->channels < 3) {
        qDebug() << "RGB color analysis requires 3-channel image";
        return false;
    }
    
    qDebug() << "Starting RGB color analysis on" << starCenters.size() << "stars";
    
    m_starColors.clear();
    m_starColors.reserve(starCenters.size());
    
    int validStars = 0;
    
    for (int i = 0; i < starCenters.size(); ++i) {
        StarColorData colorData;
        colorData.starIndex = i;
        colorData.position = starCenters[i];
        
        // Use provided radius or default
        double aperture = (i < starRadii.size()) ? starRadii[i] * 2.0 : m_apertureRadius;
        
        // Extract RGB photometry
        if (extractPhotometry(imageData, starCenters[i], aperture,
                             colorData.redValue, colorData.greenValue, colorData.blueValue)) {
            
            // Calculate color indices
            calculateColorIndices(colorData);
            
            // Try to match with catalog
            if (!m_catalogStars.isEmpty()) {
                colorData.hasValidCatalogColor = matchWithCatalog(colorData, m_catalogStars);
            }
            
            m_starColors.append(colorData);
            validStars++;
        }
    }
    
    qDebug() << "Successfully analyzed" << validStars << "stars for color";
    
    emit colorAnalysisCompleted(validStars);
    return validStars > 0;
}

bool RGBPhotometryAnalyzer::extractPhotometry(const ImageData* imageData, 
                                             const QPoint& center, 
                                             double radius,
                                             double& red, double& green, double& blue)
{
    if (center.x() < radius || center.y() < radius || 
        center.x() >= imageData->width - radius || 
        center.y() >= imageData->height - radius) {
        return false; // Too close to edge
    }
    
    // Calculate background levels
    double bgRed, bgGreen, bgBlue;
    if (!calculateBackgroundLevel(imageData, center, m_bgInnerRadius, m_bgOuterRadius,
                                 bgRed, bgGreen, bgBlue)) {
        // Use simple background estimate
        bgRed = bgGreen = bgBlue = 0.0;
    }
    
    // Sum pixels within aperture
    double sumRed = 0, sumGreen = 0, sumBlue = 0;
    int pixelCount = 0;
    
    int r = static_cast<int>(radius);
    double radiusSquared = radius * radius;
    
    for (int dy = -r; dy <= r; ++dy) {
        for (int dx = -r; dx <= r; ++dx) {
            if (dx*dx + dy*dy <= radiusSquared) {
                int x = center.x() + dx;
                int y = center.y() + dy;
                
                if (x >= 0 && y >= 0 && x < imageData->width && y < imageData->height) {
                    // ImageData format: [R_pixels, G_pixels, B_pixels]
                    int pixelIdx = y * imageData->width + x;
                    int redIdx = pixelIdx;
                    int greenIdx = pixelIdx + imageData->width * imageData->height;
                    int blueIdx = pixelIdx + 2 * imageData->width * imageData->height;
                    
                    sumRed += imageData->pixels[redIdx] - bgRed;
                    sumGreen += imageData->pixels[greenIdx] - bgGreen; 
                    sumBlue += imageData->pixels[blueIdx] - bgBlue;
                    pixelCount++;
                }
            }
        }
    }
    
    if (pixelCount == 0) return false;
    
    // Average values within aperture (background subtracted)
    red = sumRed / pixelCount;
    green = sumGreen / pixelCount;
    blue = sumBlue / pixelCount;
    
    return true;
}

bool RGBPhotometryAnalyzer::calculateBackgroundLevel(const ImageData* imageData,
                                                    const QPoint& center,
                                                    double innerRadius, 
                                                    double outerRadius,
                                                    double& bgRed, double& bgGreen, double& bgBlue)
{
    QVector<double> redValues, greenValues, blueValues;
    
    int outerR = static_cast<int>(outerRadius);
    double innerRadSq = innerRadius * innerRadius;
    double outerRadSq = outerRadius * outerRadius;
    
    for (int dy = -outerR; dy <= outerR; ++dy) {
        for (int dx = -outerR; dx <= outerR; ++dx) {
            double distSq = dx*dx + dy*dy;
            if (distSq >= innerRadSq && distSq <= outerRadSq) {
                int x = center.x() + dx;
                int y = center.y() + dy;
                
                if (x >= 0 && y >= 0 && x < imageData->width && y < imageData->height) {
                    int pixelIdx = y * imageData->width + x;
                    redValues.append(imageData->pixels[pixelIdx]);
                    greenValues.append(imageData->pixels[pixelIdx + imageData->width * imageData->height]);
                    blueValues.append(imageData->pixels[pixelIdx + 2 * imageData->width * imageData->height]);
                }
            }
        }
    }
    
    if (redValues.size() < 10) return false;
    
    // Use median for robust background estimation
    std::sort(redValues.begin(), redValues.end());
    std::sort(greenValues.begin(), greenValues.end());
    std::sort(blueValues.begin(), blueValues.end());
    
    int medianIdx = redValues.size() / 2;
    bgRed = redValues[medianIdx];
    bgGreen = greenValues[medianIdx];
    bgBlue = blueValues[medianIdx];
    
    return true;
}

void RGBPhotometryAnalyzer::calculateColorIndices(StarColorData& star)
{
    // Avoid division by zero
    if (star.redValue <= 0 || star.greenValue <= 0 || star.blueValue <= 0) {
        return;
    }
    
    // Convert to magnitude scale: mag = -2.5 * log10(flux)
    double redMag = -2.5 * std::log10(star.redValue);
    double greenMag = -2.5 * std::log10(star.greenValue);
    double blueMag = -2.5 * std::log10(star.blueValue);
    
    // Calculate standard color indices
    // Note: These are instrumental and need calibration to standard system
    star.bv_index = blueMag - greenMag;  // Approximation of B-V
    star.vr_index = greenMag - redMag;   // Approximation of V-R  
    star.gr_index = greenMag - redMag;   // Green-Red (instrumental)
}

bool RGBPhotometryAnalyzer::matchWithCatalog(StarColorData& star, const QVector<CatalogStar>& catalog)
{
    // Find closest catalog star (within 5 pixels)
    double minDistance = 5.0;
    int bestMatch = -1;
    
    for (int i = 0; i < catalog.size(); ++i) {
        if (!catalog[i].isValid || catalog[i].pixelPos.x() < 0) continue;
        
        double dx = star.position.x() - catalog[i].pixelPos.x();
        double dy = star.position.y() - catalog[i].pixelPos.y();
        double distance = std::sqrt(dx*dx + dy*dy);
        
        if (distance < minDistance) {
            minDistance = distance;
            bestMatch = i;
        }
    }
    
    if (bestMatch >= 0) {
        const CatalogStar& catalogStar = catalog[bestMatch];
        star.magnitude = catalogStar.magnitude;
        star.spectralType = catalogStar.spectralType;
        
        // Get catalog color indices from spectral type
        star.catalogBV = spectralTypeToColorIndex(catalogStar.spectralType);
        star.catalogVR = star.catalogBV * 0.5; // Rough V-R approximation
        
        // Calculate differences
        star.bv_difference = star.bv_index - star.catalogBV;
        star.colorError = std::sqrt(star.bv_difference * star.bv_difference);
        
        return true;
    }
    
    return false;
}

double RGBPhotometryAnalyzer::spectralTypeToColorIndex(const QString& spectralType)
{
    // Standard B-V color indices for main sequence stars
    QString type = spectralType.left(1).toUpper();
    
    static const QMap<QString, double> colorMap = {
        {"O", -0.30},  // Very blue
        {"B", -0.10},  // Blue  
        {"A", 0.00},   // White
        {"F", 0.30},   // Yellow-white
        {"G", 0.65},   // Yellow (Sun = 0.65)
        {"K", 1.00},   // Orange
        {"M", 1.40}    // Red
    };
    
    return colorMap.value(type, 0.65); // Default to solar
}

QColor RGBPhotometryAnalyzer::colorIndexToRGB(double bv_index)
{
    // Convert B-V color index to approximate RGB values
    // Based on stellar blackbody colors
    
    double temp = 4600 * (1.0 / (0.92 * bv_index + 1.7) + 1.0 / (0.92 * bv_index + 0.62));
    
    // Temperature to RGB conversion (simplified)
    int red, green, blue;
    
    if (temp >= 6600) {
        red = 255;
        green = static_cast<int>(std::min(255.0, 99.4708025861 * std::log(temp / 100) - 623.6));
        blue = static_cast<int>(std::min(255.0, 138.5177312231 * std::log(temp / 100 - 10) - 305.0));
    } else if (temp >= 3500) {
        red = static_cast<int>(std::min(255.0, 329.698727446 * std::pow(temp / 100 - 60, -0.1332047592)));
        green = static_cast<int>(std::min(255.0, 288.1221695283 * std::log(temp / 100) - 631.4));
        blue = 255;
    } else {
        red = 255;
        green = 100;
        blue = 50;
    }
    
    return QColor(qBound(0, red, 255), qBound(0, green, 255), qBound(0, blue, 255));
}

ColorCalibrationResult RGBPhotometryAnalyzer::calculateColorCalibration()
{
    ColorCalibrationResult result;
    
    if (m_starColors.isEmpty()) {
        result.calibrationQuality = "No Data";
        result.recommendations.append("Run color analysis first");
        return result;
    }
    
    // Collect valid star measurements
    QVector<StarColorData> validStars;
    for (const auto& star : m_starColors) {
        if (star.hasValidCatalogColor && star.colorError < 2.0) {
            validStars.append(star);
        }
    }
    
    result.starsUsed = validStars.size();
    
    if (result.starsUsed < 5) {
        result.calibrationQuality = "Insufficient Data";
        result.recommendations.append("Need at least 5 stars with catalog colors");
        return result;
    }
    
    // Calculate systematic errors
    double sumBVError = 0, sumVRError = 0;
    double sumBVErrorSq = 0;
    
    for (const auto& star : validStars) {
        sumBVError += star.bv_difference;
        sumVRError += (star.vr_index - star.catalogVR);
        sumBVErrorSq += star.bv_difference * star.bv_difference;
    }
    
    result.systematicBVError = sumBVError / result.starsUsed;
    result.systematicVRError = sumVRError / result.starsUsed;
    result.rmsColorError = std::sqrt(sumBVErrorSq / result.starsUsed);
    
    // Simple linear calibration
    result.redScale = 1.0;
    result.greenScale = 1.0 + result.systematicBVError * 0.1; 
    result.blueScale = 1.0 - result.systematicBVError * 0.1;
    
    // Assess calibration quality
    if (result.rmsColorError < 0.05) {
        result.calibrationQuality = "Excellent";
    } else if (result.rmsColorError < 0.1) {
        result.calibrationQuality = "Good";
    } else if (result.rmsColorError < 0.2) {
        result.calibrationQuality = "Fair";
    } else {
        result.calibrationQuality = "Poor";
    }
    
    // Generate recommendations
    generateRecommendations();
    
    qDebug() << "Color calibration completed:";
    qDebug() << "  Stars used:" << result.starsUsed;
    qDebug() << "  RMS color error:" << result.rmsColorError;
    qDebug() << "  Systematic B-V error:" << result.systematicBVError;
    qDebug() << "  Quality:" << result.calibrationQuality;
    
    m_lastCalibration = result;
    emit calibrationCompleted(result);
    
    return result;
}

void RGBPhotometryAnalyzer::generateRecommendations()
{
    ColorCalibrationResult& result = m_lastCalibration;
    result.recommendations.clear();
    
    if (std::abs(result.systematicBVError) > 0.1) {
        result.recommendations.append("Significant color bias detected - consider filter response calibration");
    }
    
    if (result.rmsColorError > 0.2) {
        result.recommendations.append("High color scatter - check for systematic issues");
        result.recommendations.append("Consider: flat field correction, focus quality, atmospheric extinction");
    }
    
    if (result.starsUsed < 10) {
        result.recommendations.append("Use more reference stars for better calibration");
    }
    
    if (result.calibrationQuality == "Good" || result.calibrationQuality == "Excellent") {
        result.recommendations.append("Apply calculated color corrections to improve accuracy");
    }
}

bool RGBPhotometryAnalyzer::setStarCatalogData(const QVector<CatalogStar>& catalogStars)
{
    m_catalogStars = catalogStars;
    qDebug() << "Set catalog with" << catalogStars.size() << "stars for color analysis";
    return !catalogStars.isEmpty();
}
