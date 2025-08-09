#include <iostream>
#include <fitsio.h>
#include <stellarsolver.h>
#include <parameters.h>
#include <structuredefinitions.h>
#include "OriginStellarSolverInterface.h"

// Implementation
OriginStellarSolverInterface::OriginStellarSolverInterface(QObject* parent)
    : QObject(parent)
{
}

bool OriginStellarSolverInterface::loadOriginTIFF(const QString& tiffFilePath, OriginStellarSolverJob& job)
{
    job.filename = tiffFilePath;
    QFileInfo fileInfo(tiffFilePath);
    
    logInfo(QString("Loading Origin TIFF: %1").arg(fileInfo.fileName()));
    
    // Step 1: Extract Origin metadata hints
    if (!extractOriginHints(job)) {
        logInfo("No Origin metadata found - will use standard solving");
    }
    
    // Step 2: Convert TIFF image for StellarSolver (handled separately)
    // This will be done when creating the StellarSolver instance
    
    return true;
}

bool OriginStellarSolverInterface::extractOriginHints(OriginStellarSolverJob& job)
{
    QDateTime startTime = QDateTime::currentDateTime();
    
    OriginMetadataExtractor extractor;
    extractor.setVerboseLogging(false); // Keep quiet during extraction
    
    if (extractor.extractFromTIFFFile(job.filename.toStdString())) {
        const OriginTelescopeMetadata& meta = extractor.getMetadata();
        
        if (meta.isValid && meta.centerRA >= 0 && meta.centerRA <= 360 && 
            meta.centerDec >= -90 && meta.centerDec <= 90) {
            
            job.hasOriginHints = true;
            job.objectName = QString::fromStdString(meta.objectName);
            job.hintRA = meta.centerRA;
            job.hintDec = meta.centerDec;
            job.hintPixelScale = meta.getAveragePixelScale();
            job.hintOrientation = meta.orientation;
            job.stackedFrames = meta.stackedDepth;
            
            // Assess hint quality
            job.hintQuality = assessHintQuality(meta);
            job.searchRadius = getSearchRadiusForQuality(job.hintQuality);
            job.expectedSpeedup = getExpectedSpeedupForQuality(job.hintQuality);
            job.accelerated = true;
            
            logInfo(QString("✨ Origin hints extracted: %1 (Quality: %2)")
                   .arg(job.objectName).arg(job.hintQuality));
            logInfo(QString("   Coordinates: RA=%1°, Dec=%2°, Scale=%3\"/px")
                   .arg(job.hintRA, 0, 'f', 4)
                   .arg(job.hintDec, 0, 'f', 4)  
                   .arg(job.hintPixelScale, 0, 'f', 2));
            
            emit originHintsExtracted(job);
        }
    }
    
    job.hintExtractionTime = startTime.msecsTo(QDateTime::currentDateTime()) / 1000.0;
    return job.hasOriginHints;
}

void OriginStellarSolverInterface::configureSolverWithOriginHints(StellarSolver* solver, const OriginStellarSolverJob& job)
{
    if (!solver) return;
    
    QString configInfo;
    
    if (job.hasOriginHints) {
        // Set search position from Origin hints
        solver->setSearchPositionInDegrees(job.hintRA, job.hintDec);
        
        // Get current parameters and modify for acceleration
        Parameters params = solver->getCurrentParameters();
        
        // Adjust search radius based on hint quality
        params.search_radius = job.searchRadius;
        
        // Optimize parameters based on hint quality
        if (job.hintQuality == "EXCELLENT") {
            params.solverTimeLimit = 60;      // Very fast timeout
            params.keepNum = 200;             // Fewer stars needed
            params.resort = true;
            params.minwidth = job.hintPixelScale * 0.8 / 3600.0;  // Tighter scale bounds
            params.maxwidth = job.hintPixelScale * 1.2 / 3600.0;
        } else if (job.hintQuality == "GOOD") {
            params.solverTimeLimit = 90;      // Fast timeout
            params.keepNum = 300;
            params.minwidth = job.hintPixelScale * 0.6 / 3600.0;
            params.maxwidth = job.hintPixelScale * 1.4 / 3600.0;
        } else if (job.hintQuality == "POOR") {
            params.solverTimeLimit = 120;     // Moderate timeout
            params.keepNum = 400;
            params.minwidth = job.hintPixelScale * 0.4 / 3600.0;
            params.maxwidth = job.hintPixelScale * 1.6 / 3600.0;
        }
        params.minwidth = 60.0;
	params.maxwidth = 5000.0;
	
        solver->setParameters(params);
        
        configInfo = QString("🚀 Accelerated solving: %1\n")
                    .arg(job.objectName) +
                    QString("   Search: ±%1° around RA=%2°, Dec=%3°\n")
                    .arg(job.searchRadius, 0, 'f', 1)
                    .arg(job.hintRA, 0, 'f', 3)
                    .arg(job.hintDec, 0, 'f', 3) +
                    QString("   Scale hint: %1 arcsec/pixel\n")
                    .arg(job.hintPixelScale, 0, 'f', 2) +
                    QString("   Quality: %1 (expected %2x speedup)")
                    .arg(job.hintQuality)
                    .arg(job.expectedSpeedup, 0, 'f', 1);
                    
    } else {
        configInfo = QString("🐌 Standard solving: %1 (no Origin hints)")
                    .arg(QFileInfo(job.filename).baseName());
    }
    
    logInfo(configInfo);
    emit solverConfigured(job.jobId, configInfo);
}

void OriginStellarSolverInterface::analyzeResults(OriginStellarSolverJob& job, const FITSImage::Solution& solution)
{
    if (!job.hasOriginHints) return;
    
    // Calculate hint accuracy
    double raDiff = qAbs(solution.ra - job.hintRA);
    double decDiff = qAbs(solution.dec - job.hintDec);
    double scaleDiff = qAbs(solution.pixscale - job.hintPixelScale) / job.hintPixelScale * 100.0;
    
    job.hintAccuracyRA = raDiff * 3600.0;      // Convert to arcseconds
    job.hintAccuracyDec = decDiff * 3600.0;    // Convert to arcseconds  
    job.hintAccuracyScale = scaleDiff;         // Percentage
    
    logInfo(QString("   Hint accuracy: RA=%1\", Dec=%2\", Scale=%3%")
           .arg(job.hintAccuracyRA, 0, 'f', 1)
           .arg(job.hintAccuracyDec, 0, 'f', 1)
           .arg(job.hintAccuracyScale, 0, 'f', 1));
    
    // Assess hint quality based on actual accuracy
    QString accuracyAssessment;
    if (job.hintAccuracyRA < 30 && job.hintAccuracyDec < 30 && job.hintAccuracyScale < 10) {
        accuracyAssessment = "EXCELLENT accuracy - hints very reliable";
    } else if (job.hintAccuracyRA < 120 && job.hintAccuracyDec < 120 && job.hintAccuracyScale < 25) {
        accuracyAssessment = "GOOD accuracy - hints useful";
    } else {
        accuracyAssessment = "POOR accuracy - hints had significant error";
    }
    
    logInfo(QString("   Assessment: %1").arg(accuracyAssessment));
}

// Static utility methods
bool OriginStellarSolverInterface::isOriginTIFF(const QString& filePath)
{
    return OriginMetadataExtractor::containsOriginMetadata(filePath.toStdString());
}

QString OriginStellarSolverInterface::assessHintQuality(const OriginTelescopeMetadata& meta)
{
    int score = 0;
    
    // More stacked frames = better accuracy
    if (meta.stackedDepth >= 100) score += 3;
    else if (meta.stackedDepth >= 50) score += 2;
    else if (meta.stackedDepth >= 20) score += 1;
    
    // Longer total exposure = more stars detected during stacking
    double totalExposure = meta.getTotalExposureSeconds();
    if (totalExposure >= 1800) score += 2;      // 30+ minutes
    else if (totalExposure >= 600) score += 1;  // 10+ minutes
    
    // Reasonable pixel scale suggests good optics/calibration
    double pixelScale = meta.getAveragePixelScale();
    if (pixelScale >= 0.5 && pixelScale <= 5.0) score += 1;
    
    // Field of view consistency check
    if (meta.fieldOfViewX > 0 && meta.fieldOfViewY > 0) score += 1;
    
    // Convert score to quality rating
    if (score >= 6) return "EXCELLENT";
    if (score >= 4) return "GOOD";
    if (score >= 2) return "POOR";
    return "NONE";
}

double OriginStellarSolverInterface::getSearchRadiusForQuality(const QString& quality)
{
    if (quality == "EXCELLENT") return 0.5;    // Very tight search
    if (quality == "GOOD") return 1.0;         // Moderate search  
    if (quality == "POOR") return 1.5;         // Wider search
    return 2.0;                                // Default wide search
}

double OriginStellarSolverInterface::getExpectedSpeedupForQuality(const QString& quality)
{
    if (quality == "EXCELLENT") return 8.0;
    if (quality == "GOOD") return 4.0;
    if (quality == "POOR") return 2.0;
    return 1.0;
}

bool OriginStellarSolverInterface::convertTIFFToStellarSolverFormat(const QString& tiffPath, 
                                                                   FITSImage::Statistic& stats, 
                                                                   std::vector<uint8_t>& buffer)
{
    uint32_t width, height;
    std::vector<uint16_t> imageData;
    
    if (!readTIFFImageData(tiffPath, width, height, imageData)) {
        logError("Failed to read TIFF image data");
        return false;
    }
    
    logInfo(QString("TIFF image: %1×%2 pixels").arg(width).arg(height));
    
    // Downsample for faster processing (similar to your StellarSolverManager approach)
    const int downsampleFactor = 2;
    int outputWidth = width / downsampleFactor;
    int outputHeight = height / downsampleFactor;
    
    buffer.resize(outputWidth * outputHeight);
    
    // Find min/max for normalization
    uint16_t minVal = *std::min_element(imageData.begin(), imageData.end());
    uint16_t maxVal = *std::max_element(imageData.begin(), imageData.end());
    float range = maxVal - minVal;
    
    if (range > 0) {
        // Downsample with simple averaging
        for (int y = 0; y < outputHeight; y++) {
            for (int x = 0; x < outputWidth; x++) {
                int srcY = y * downsampleFactor;
                int srcX = x * downsampleFactor;
                
                if (srcY + 1 >= height || srcX + 1 >= width) continue;
                
                // Simple 2x2 average
                uint16_t p1 = imageData[srcY * width + srcX];
                uint16_t p2 = imageData[srcY * width + srcX + 1];
                uint16_t p3 = imageData[(srcY + 1) * width + srcX];
                uint16_t p4 = imageData[(srcY + 1) * width + srcX + 1];
                
                float avg = (p1 + p2 + p3 + p4) / 4.0f;
                float normalized = (avg - minVal) / range;
                buffer[y * outputWidth + x] = static_cast<uint8_t>(std::clamp(normalized * 255.0f, 0.0f, 255.0f));
            }
        }
    } else {
        std::fill(buffer.begin(), buffer.end(), 128);
    }
    
    // Create statistics for StellarSolver
    stats.width = outputWidth;
    stats.height = outputHeight;
    stats.channels = 1;
    stats.dataType = TBYTE;
    stats.bytesPerPixel = 1;
    
    for (int i = 0; i < 3; i++) {
        stats.min[i] = (i == 0) ? minVal : 0.0;
        stats.max[i] = (i == 0) ? maxVal : 0.0;
        stats.mean[i] = (i == 0) ? (minVal + maxVal) / 2.0 : 0.0;
        stats.stddev[i] = 0.0;
        stats.median[i] = (i == 0) ? (minVal + maxVal) / 2.0 : 0.0;
    }
    stats.SNR = 1.0;
    
    logInfo(QString("Converted to %1×%2 for solving").arg(outputWidth).arg(outputHeight));
    return true;
}

bool OriginStellarSolverInterface::readTIFFImageData(const QString& tiffPath, 
                                                    uint32_t& width, uint32_t& height,
                                                    std::vector<uint16_t>& imageData)
{
    TIFF* tiff = TIFFOpen(tiffPath.toLocal8Bit().data(), "r");
    if (!tiff) {
        logError(QString("Failed to open TIFF: %1").arg(tiffPath));
        return false;
    }
    
    // Get TIFF properties
    uint16_t bitsPerSample, samplesPerPixel;
    TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &height);
    TIFFGetField(tiff, TIFFTAG_BITSPERSAMPLE, &bitsPerSample);
    TIFFGetField(tiff, TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel);
    
    logInfo(QString("TIFF properties: %1×%2, %3-bit, %4 channels")
           .arg(width).arg(height).arg(bitsPerSample).arg(samplesPerPixel));
    
    // Read image data
    size_t npixels = width * height * samplesPerPixel;
    imageData.resize(npixels);
    
    // Simple approach: read as RGBA and convert to grayscale
    std::vector<uint32_t> rgbaData(width * height);
    if (TIFFReadRGBAImageOriented(tiff, width, height, rgbaData.data(), ORIENTATION_TOPLEFT) == 0) {
        TIFFClose(tiff);
        logError("Failed to read TIFF image data");
        return false;
    }
    
    // Convert RGBA to grayscale uint16
    for (size_t i = 0; i < width * height; i++) {
        uint32_t pixel = rgbaData[i];
        uint8_t r = TIFFGetR(pixel);
        uint8_t g = TIFFGetG(pixel);
        uint8_t b = TIFFGetB(pixel);
        
        // Convert to grayscale and scale to 16-bit
        uint16_t gray = static_cast<uint16_t>((0.299 * r + 0.587 * g + 0.114 * b) * 257);
        imageData[i] = gray;
    }
    
    TIFFClose(tiff);
    return true;
}

void OriginStellarSolverInterface::logInfo(const QString& message)
{
    if (m_verboseLogging) {
        qDebug() << "[OriginInterface]" << message;
    }
}

void OriginStellarSolverInterface::logError(const QString& message)
{
    qDebug() << "[OriginInterface ERROR]" << message;
}

// #include "OriginStellarSolverInterface.moc"
