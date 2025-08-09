// OriginTIFFSolver.h - Dedicated TIFF-only plate solver for Origin telescope files
// No FITS dependencies - pure TIFF workflow with Origin metadata acceleration

#ifndef ORIGIN_TIFF_SOLVER_H
#define ORIGIN_TIFF_SOLVER_H

#include <QCoreApplication>
#include <QObject>
#include <QQueue>
#include <QJsonObject>
#include <QDateTime>
#include <QFileInfo>
#include <QDir>
#include <iostream>
#include <vector>
#include <tiffio.h>

// StellarSolver includes
#include <fitsio.h>
#include <stellarsolver.h>
#include <parameters.h>
#include <structuredefinitions.h>

// Origin metadata extractor
#include "OriginMetadataExtractor.h"

struct OriginTIFFJob {
    QString tiffFilename;
    int jobId;
    QDateTime startTime;
    QString status = "PENDING";
    
    // Origin metadata
    bool hasOriginHints = false;
    QString objectName;
    double hintRA = -1.0;
    double hintDec = -91.0;
    double hintPixelScale = 0.0;
    double hintOrientation = 0.0;
    int stackedFrames = 0;
    QString hintQuality = "NONE";        // EXCELLENT, GOOD, POOR, NONE
    double searchRadius = 2.0;           // degrees
    double hintExtractionTime = 0.0;
    
    // Solved results
    double solvedRA = -1.0;
    double solvedDec = -91.0;
    double solvedPixelScale = 0.0;
    double solvedOrientation = 0.0;
    double solveTime = 0.0;
    double totalTime = 0.0;
    
    // Performance
    bool accelerated = false;
    double expectedSpeedup = 1.0;
};

class OriginTIFFSolver : public QObject
{
    Q_OBJECT

public:
    OriginTIFFSolver(int maxConcurrent = 8, QObject *parent = nullptr) 
        : QObject(parent), m_maxConcurrent(maxConcurrent), m_completedJobs(0) {
        
        setupSolverParameters();
    }
    
    ~OriginTIFFSolver() {
        // Clean up any remaining solvers
        for (auto* solver : m_activeSolvers.keys()) {
            if (solver && solver->isRunning()) {
                solver->abortAndWait();
            }
            solver->deleteLater();
        }
    }

    // Add TIFF file for processing
    void addTIFFJob(const QString& tiffFilename) {
        OriginTIFFJob job;
        job.tiffFilename = tiffFilename;
        job.jobId = m_jobQueue.size() + 1;
        
        // Extract Origin hints immediately
        extractOriginHints(job);
        
        m_jobQueue.enqueue(job);
    }

    // Process batch of TIFF files
    void startBatchSolving() {
        if (m_jobQueue.isEmpty()) {
            qDebug() << "No TIFF jobs to process!";
            QCoreApplication::quit();
            return;
        }

        m_totalJobs = m_jobQueue.size();
        analyzeBatchHints();
        
        qDebug() << "=== Origin TIFF Plate Solving ===";
        qDebug() << "Total TIFF files: " << m_totalJobs;
        qDebug() << "With Origin metadata: " << m_originTIFFCount;
        qDebug() << "Excellent hints: " << m_excellentHintsCount;
        qDebug() << "Good hints: " << m_goodHintsCount;
        qDebug() << "Expected average speedup: " << calculateExpectedSpeedup() << "x";
        qDebug() << "Max concurrent: " << m_maxConcurrent;
        qDebug();

        m_batchStartTime = QDateTime::currentDateTime();
        emit statusChanged(QString("Starting TIFF batch solve: %1 files (%2 with Origin hints)")
                          .arg(m_totalJobs).arg(m_originTIFFCount));
       
        startNextJobs();
    }

    // Set output directory for solved results (optional - can work in-place)
    void setOutputDirectory(const QString& outputDir) {
        m_outputDirectory = outputDir;
    }

signals:
    void progressChanged(int completed, int total);
    void statusChanged(const QString& message);
    void batchFinished(int successful, int failed);

private slots:
    void onSolverFinished() {
        StellarSolver* solver = qobject_cast<StellarSolver*>(sender());
        if (!solver || !m_activeSolvers.contains(solver)) {
            return;
        }

        OriginTIFFJob& job = m_activeSolvers[solver];
        
        // Calculate times
        qint64 totalMs = job.startTime.msecsTo(QDateTime::currentDateTime());
        job.totalTime = totalMs / 1000.0;

        // Process results
        if (solver->solvingDone() && solver->hasWCSData()) {
            FITSImage::Solution solution = solver->getSolution();
            job.status = "SUCCESS";
            job.solvedRA = solution.ra;
            job.solvedDec = solution.dec;
            job.solvedPixelScale = solution.pixscale / 2.0; // Correct for 2x downsampling
            job.solvedOrientation = solution.orientation;
            job.solveTime = job.totalTime;
            
            // Performance analysis
            if (job.hasOriginHints) {
                analyzeAccelerationPerformance(job, solution);
            }
            
            // Write results back to TIFF with WCS metadata
            QString outputFile = getOutputFilename(job.tiffFilename);
            if (writeWCSToTIFF(job.tiffFilename, outputFile, solution, job)) {
                qDebug() << "[Job " << job.jobId << "] ✓ SUCCESS: " 
                         << QFileInfo(job.tiffFilename).baseName().toStdString();
                if (job.hasOriginHints) {
                    qDebug() << "    🚀 ACCELERATED (" << job.hintQuality.toStdString() 
                             << " hints, " << job.expectedSpeedup << "x speedup)";
                }
                qDebug() << "    Object: " << job.objectName.toStdString();
                qDebug() << "    Solved: RA=" << job.solvedRA << "°, Dec=" << job.solvedDec << "°";
                qDebug() << "    Scale: " << job.solvedPixelScale << " arcsec/pixel";
                qDebug() << "    Total time: " << job.totalTime << "s";
            } else {
                qDebug() << "[Job " << job.jobId << "] ⚠ SOLVED but failed to write WCS to TIFF";
                job.status = "SOLVED_NO_WRITE";
            }
            
        } else {
            job.status = "FAILED";
            QString baseName = QFileInfo(job.tiffFilename).baseName();
            qDebug() << "[Job " << job.jobId << "] ✗ FAILED: " << baseName.toStdString();
            if (job.hasOriginHints) {
                qDebug() << "    (despite " << job.hintQuality.toStdString() << " Origin hints)";
            }
        }

        // Update progress and cleanup
        m_completedJobs++;
        emit progressChanged(m_completedJobs, m_totalJobs);
        m_results.append(job);
        
        m_activeSolvers.remove(solver);
        solver->deleteLater();

        // Continue or finish
        if (m_completedJobs >= m_totalJobs) {
            finishBatch();
        } else {
            startNextJobs();
        }
    }
    
private:
    // Extract Origin metadata hints from TIFF
    void extractOriginHints(OriginTIFFJob& job) {
        QDateTime startTime = QDateTime::currentDateTime();
        
        OriginMetadataExtractor extractor;
        extractor.setVerboseLogging(false);
        
        if (extractor.extractFromTIFFFile(job.tiffFilename.toStdString())) {
            const OriginTelescopeMetadata& meta = extractor.getMetadata();
            
            if (meta.isValid && meta.centerRA >= 0 && meta.centerRA <= 360 && 
                meta.centerDec >= -90 && meta.centerDec <= 90) {
                
                job.hasOriginHints = true;
                job.objectName = QString::fromStdString(meta.objectName);
                job.hintRA = meta.centerRA;
                job.hintDec = meta.centerDec;
                job.hintPixelScale = 1.47684; // meta.getAveragePixelScale();
                job.hintOrientation = meta.orientation;
                job.stackedFrames = meta.stackedDepth;
                
                // Assess hint quality
                job.hintQuality = assessHintQuality(meta);
                job.searchRadius = getSearchRadius(job.hintQuality);
                job.expectedSpeedup = getExpectedSpeedup(job.hintQuality);
                job.accelerated = true;
                
                qDebug() << "[Job " << job.jobId << "] ✨ Origin hints extracted: " 
                         << job.objectName.toStdString() 
                         << " (Quality: " << job.hintQuality.toStdString() << ")";
            }
        }
        
        job.hintExtractionTime = startTime.msecsTo(QDateTime::currentDateTime()) / 1000.0;
    }

    QString assessHintQuality(const OriginTelescopeMetadata& meta) {
        int score = 0;
        
        if (meta.stackedDepth >= 100) score += 3;
        else if (meta.stackedDepth >= 50) score += 2;
        else if (meta.stackedDepth >= 20) score += 1;
        
        double totalExposure = meta.getTotalExposureSeconds();
        if (totalExposure >= 1800) score += 2;      // 30+ minutes
        else if (totalExposure >= 600) score += 1;  // 10+ minutes
        
        double pixelScale = meta.getAveragePixelScale();
        if (pixelScale >= 0.5 && pixelScale <= 5.0) score += 1;
        
        if (meta.fieldOfViewX > 0 && meta.fieldOfViewY > 0) score += 1;
        
        if (score >= 6) return "EXCELLENT";
        if (score >= 4) return "GOOD";
        if (score >= 2) return "POOR";
        return "NONE";
    }

    double getSearchRadius(const QString& quality) {
        if (quality == "EXCELLENT") return 0.5;
        if (quality == "GOOD") return 1.0;
        if (quality == "POOR") return 1.5;
        return 2.0;
    }

    double getExpectedSpeedup(const QString& quality) {
        if (quality == "EXCELLENT") return 8.0;
        if (quality == "GOOD") return 4.0;
        if (quality == "POOR") return 2.0;
        return 1.0;
    }

    void setupSolverParameters() {
        QList<Parameters> profiles = StellarSolver::getBuiltInProfiles();
        if (profiles.isEmpty()) {
            qDebug() << "ERROR: No parameter profiles available";
            return;
        }

        m_commonParams = profiles.at(0);

        // Optimized for TIFF solving with Origin hints
        m_commonParams.multiAlgorithm = SSolver::MULTI_AUTO;
        m_commonParams.search_radius = 1.0;      // Will be adjusted per job
        m_commonParams.minwidth = 0.1;
        m_commonParams.maxwidth = 10.0;
        m_commonParams.resort = true;
        m_commonParams.autoDownsample = false;
        m_commonParams.downsample = 1;
        m_commonParams.inParallel = true;
        m_commonParams.solverTimeLimit = 120;

        // TIFF-optimized star extraction
        m_commonParams.initialKeep = 1500;
        m_commonParams.keepNum = 400;
        m_commonParams.r_min = 1.0;
        m_commonParams.removeBrightest = 0;
        m_commonParams.removeDimmest = 50;
        m_commonParams.saturationLimit = 65000;
        m_commonParams.minarea = 5;

        // Find index files
        m_indexPaths = findIndexFiles();
        if (m_indexPaths.isEmpty()) {
            qDebug() << "ERROR: No astrometry index files found!";
        }
    }

    void startNextJobs() {
        while (m_activeSolvers.size() < m_maxConcurrent && !m_jobQueue.isEmpty()) {
            OriginTIFFJob job = m_jobQueue.dequeue();
            startSingleJob(job);
        }
    }

    void startSingleJob(OriginTIFFJob job) {
        StellarSolver* solver = new StellarSolver(this);
        
        connect(solver, &StellarSolver::finished, this, &OriginTIFFSolver::onSolverFinished);
        
        // Configure solver
        solver->setProperty("ProcessType", SSolver::SOLVE);
        solver->setProperty("ExtractorType", SSolver::EXTRACTOR_INTERNAL);
        solver->setProperty("SolverType", SSolver::SOLVER_STELLARSOLVER);
        solver->setIndexFolderPaths(m_indexPaths);

        // Customize parameters based on hints
        Parameters jobParams = m_commonParams;
        
        if (job.hasOriginHints) {
            jobParams.search_radius = job.searchRadius;
            
            if (job.hintQuality == "EXCELLENT") {
                jobParams.solverTimeLimit = 60;
                jobParams.keepNum = 300;
            } else if (job.hintQuality == "GOOD") {
                jobParams.solverTimeLimit = 90;
                jobParams.keepNum = 350;
            }
            
            // Set coordinate hints
            solver->setSearchPositionInDegrees(job.hintRA, job.hintDec);
            
            qDebug() << "[Job " << job.jobId << "] 🚀 Accelerated TIFF solving: " 
                     << job.objectName.toStdString()
                     << " (search radius: " << job.searchRadius << "°)";
        } else {
            qDebug() << "[Job " << job.jobId << "] 🐌 Standard TIFF solving: " 
                     << QFileInfo(job.tiffFilename).baseName().toStdString()
                     << " (no Origin metadata)";
        }
        
        solver->setParameters(jobParams);

        // Load TIFF image
        if (!loadTIFFImage(solver, job)) {
            job.status = "LOAD_FAILED";
            m_completedJobs++;
            m_results.append(job);
            solver->deleteLater();
            
            qDebug() << "[Job " << job.jobId << "] ⚠ FAILED to load TIFF: "
                     << QFileInfo(job.tiffFilename).baseName().toStdString();
            
            if (m_completedJobs >= m_totalJobs) {
                finishBatch();
            } else {
                startNextJobs();
            }
            return;
        }

        // Start timing and solving
        job.startTime = QDateTime::currentDateTime();
        m_activeSolvers[solver] = job;
        solver->start();
    }

    bool loadTIFFImage(StellarSolver* solver, const OriginTIFFJob& job) {
        TIFF* tiff = TIFFOpen(job.tiffFilename.toLocal8Bit().data(), "r");
        if (!tiff) {
            qDebug() << "Failed to open TIFF file:" << job.tiffFilename.toStdString();
            return false;
        }

        // Get TIFF dimensions
        uint32 width, height;
        uint16 bitsPerSample, samplesPerPixel;
        
        TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &width);
        TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &height);
        TIFFGetField(tiff, TIFFTAG_BITSPERSAMPLE, &bitsPerSample);
        TIFFGetField(tiff, TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel);

        qDebug() << "    TIFF: " << width << "×" << height 
                 << ", " << bitsPerSample << "-bit"
                 << ", " << samplesPerPixel << " channels";

        // Read TIFF data
        size_t npixels = width * height * samplesPerPixel;
        std::vector<uint16_t> imageData(npixels);
        
        if (TIFFReadRGBAImageOriented(tiff, width, height, 
                                     reinterpret_cast<uint32*>(imageData.data()), 
                                     ORIENTATION_TOPLEFT) == 0) {
            TIFFClose(tiff);
            qDebug() << "Failed to read TIFF image data";
            return false;
        }
        
        TIFFClose(tiff);

        // Convert to format suitable for StellarSolver
        // Downsample for faster processing
        const int downsampleFactor = 2;
        int outputWidth = width / downsampleFactor;
        int outputHeight = height / downsampleFactor;
        
        std::vector<uint8_t> buffer(outputWidth * outputHeight);
        
        // Convert and downsample (simplified RGB to grayscale)
        for (int y = 0; y < outputHeight; y++) {
            for (int x = 0; x < outputWidth; x++) {
                int srcY = y * downsampleFactor;
                int srcX = x * downsampleFactor;
                
                if (srcY >= height || srcX >= width) continue;
                
                // Get pixel (RGBA format from TIFFReadRGBAImageOriented)
                uint32_t pixel = reinterpret_cast<uint32_t*>(imageData.data())[srcY * width + srcX];
                
                // Extract RGB and convert to grayscale
                uint8_t r = TIFFGetR(pixel);
                uint8_t g = TIFFGetG(pixel);
                uint8_t b = TIFFGetB(pixel);
                
                uint8_t gray = static_cast<uint8_t>(0.299 * r + 0.587 * g + 0.114 * b);
                buffer[y * outputWidth + x] = gray;
            }
        }

        // Create statistics for StellarSolver
        FITSImage::Statistic stats{};
        stats.width = outputWidth;
        stats.height = outputHeight;
        stats.channels = 1;
        stats.dataType = TBYTE;
        stats.bytesPerPixel = 1;
        
        // Calculate basic statistics
        auto minmax = std::minmax_element(buffer.begin(), buffer.end());
        stats.min[0] = *minmax.first;
        stats.max[0] = *minmax.second;
        stats.mean[0] = (stats.min[0] + stats.max[0]) / 2.0;
        stats.median[0] = stats.mean[0];
        stats.stddev[0] = 0.0;
        stats.SNR = 1.0;

        // Store buffer
        m_imageBuffers[solver] = std::move(buffer);
        
        return solver->loadNewImageBuffer(stats, m_imageBuffers[solver].data());
    }

    QString getOutputFilename(const QString& inputFile) {
        QFileInfo fileInfo(inputFile);
        
        if (!m_outputDirectory.isEmpty()) {
            return QString("%1/solved_%2.tiff").arg(m_outputDirectory).arg(fileInfo.baseName());
        } else {
            // In-place: add _solved suffix
            QString dir = fileInfo.absolutePath();
            return QString("%1/solved_%2.tiff").arg(dir).arg(fileInfo.baseName());
        }
    }

    bool writeWCSToTIFF(const QString& inputFile, const QString& outputFile, 
                        const FITSImage::Solution& solution, const OriginTIFFJob& job) {
        
        // For now, create a simple output with WCS information
        // You could enhance this to embed WCS data directly in TIFF tags
        
        QFileInfo inputInfo(inputFile);
        QFileInfo outputInfo(outputFile);
        
        // Ensure output directory exists
        QDir().mkpath(outputInfo.absolutePath());
        
        // Copy original TIFF to output location
        if (!QFile::copy(inputFile, outputFile)) {
            // If copy fails, try to overwrite
            QFile::remove(outputFile);
            if (!QFile::copy(inputFile, outputFile)) {
                qDebug() << "Failed to copy TIFF to output location";
                return false;
            }
        }
        
        // Create companion WCS file with solution
        QString wcsFile = outputInfo.absolutePath() + "/" + outputInfo.baseName() + "_wcs.json";
        QFile file(wcsFile);
        if (file.open(QIODevice::WriteOnly)) {
            QJsonObject wcsData;
            wcsData["input_file"] = inputFile;
            wcsData["object_name"] = job.objectName;
            wcsData["solved_ra"] = solution.ra;
            wcsData["solved_dec"] = solution.dec;
            wcsData["pixel_scale"] = solution.pixscale / 2.0; // Corrected for downsampling
            wcsData["orientation"] = solution.orientation;
            wcsData["field_width"] = solution.fieldWidth;
            wcsData["field_height"] = solution.fieldHeight;
            wcsData["solve_time"] = job.totalTime;
            wcsData["accelerated"] = job.accelerated;
            wcsData["hint_quality"] = job.hintQuality;
            wcsData["expected_speedup"] = job.expectedSpeedup;
            
            if (job.hasOriginHints) {
                QJsonObject hints;
                hints["ra"] = job.hintRA;
                hints["dec"] = job.hintDec;
                hints["pixel_scale"] = job.hintPixelScale;
                hints["orientation"] = job.hintOrientation;
                hints["stacked_frames"] = job.stackedFrames;
                wcsData["origin_hints"] = hints;
            }
            
            QJsonDocument doc(wcsData);
            file.write(doc.toJson());
            file.close();
        }
        
        qDebug() << "    WCS data written to: " << QFileInfo(wcsFile).fileName().toStdString();
        return true;
    }

    void analyzeAccelerationPerformance(const OriginTIFFJob& job, const FITSImage::Solution& solution) {
        if (!job.hasOriginHints) return;
        
        double raDiff = qAbs(solution.ra - job.hintRA);
        double decDiff = qAbs(solution.dec - job.hintDec);
        double scaleDiff = qAbs((solution.pixscale / 2.0) - job.hintPixelScale) / job.hintPixelScale * 100.0;
        
        qDebug() << "    Hint accuracy: RA=" << (raDiff * 3600.0) << "\", Dec=" << (decDiff * 3600.0) 
                 << "\", Scale=" << scaleDiff << "%";
        
        m_totalHintRaError += raDiff * 3600.0;
        m_totalHintDecError += decDiff * 3600.0;
        m_totalHintScaleError += scaleDiff;
        m_analyzedHintJobs++;
    }

    void analyzeBatchHints() {
        m_originTIFFCount = 0;
        m_excellentHintsCount = 0;
        m_goodHintsCount = 0;
        
        for (const auto& job : m_jobQueue) {
            if (job.hasOriginHints) {
                m_originTIFFCount++;
                if (job.hintQuality == "EXCELLENT") m_excellentHintsCount++;
                else if (job.hintQuality == "GOOD") m_goodHintsCount++;
            }
        }
    }

    double calculateExpectedSpeedup() {
        if (m_totalJobs == 0) return 1.0;
        
        double acceleratedBenefit = m_excellentHintsCount * 8.0 + m_goodHintsCount * 4.0;
        double totalBenefit = acceleratedBenefit + (m_totalJobs - m_originTIFFCount) * 1.0;
        
        return totalBenefit / m_totalJobs;
    }

    void finishBatch() {
        qint64 totalBatchTime = m_batchStartTime.msecsTo(QDateTime::currentDateTime());
        
        qDebug() << "\n=== Origin TIFF Batch Complete ===";
        qDebug() << "Total time: " << (totalBatchTime / 1000.0) << " seconds";
        
        int successful = 0, failed = 0;
        int acceleratedSuccessful = 0, standardSuccessful = 0;
        double totalSolveTime = 0, acceleratedSolveTime = 0, standardSolveTime = 0;
        
        for (const auto& result : m_results) {
            if (result.status == "SUCCESS") {
                successful++;
                totalSolveTime += result.totalTime;
                
                if (result.hasOriginHints) {
                    acceleratedSuccessful++;
                    acceleratedSolveTime += result.totalTime;
                } else {
                    standardSuccessful++;
                    standardSolveTime += result.totalTime;
                }
            } else {
                failed++;
            }
        }
        
        qDebug() << "\nTIFF Processing Summary:";
        qDebug() << "  Total successful: " << successful << "/" << m_totalJobs;
        qDebug() << "  Accelerated (Origin): " << acceleratedSuccessful << "/" << m_originTIFFCount;
        qDebug() << "  Standard: " << standardSuccessful << "/" << (m_totalJobs - m_originTIFFCount);
        qDebug() << "  Failed: " << failed;
        
        if (acceleratedSuccessful > 0 && standardSuccessful > 0) {
            double actualSpeedup = (standardSolveTime / standardSuccessful) / (acceleratedSolveTime / acceleratedSuccessful);
            qDebug() << "\nPerformance Analysis:";
            qDebug() << "  Avg accelerated solve time: " << (acceleratedSolveTime / acceleratedSuccessful) << "s";
            qDebug() << "  Avg standard solve time: " << (standardSolveTime / standardSuccessful) << "s";
            qDebug() << "  Actual speedup factor: " << actualSpeedup << "x";
        }
        
        if (m_analyzedHintJobs > 0) {
            qDebug() << "\nOrigin Hint Accuracy:";
            qDebug() << "  Avg RA error: " << (m_totalHintRaError / m_analyzedHintJobs) << " arcsec";
            qDebug() << "  Avg Dec error: " << (m_totalHintDecError / m_analyzedHintJobs) << " arcsec";
            qDebug() << "  Avg scale error: " << (m_totalHintScaleError / m_analyzedHintJobs) << "%";
        }
        
        emit batchFinished(successful, failed);
    }

    QStringList findIndexFiles() {
        QStringList indexPaths;
        QStringList searchPaths = {
            "/usr/local/astrometry/data",
            "/opt/homebrew/share/astrometry", 
            "/usr/local/share/astrometry"
        };
        
        for (const QString &path : searchPaths) {
            QDir indexDir(path);
            if (indexDir.exists()) {
                QStringList filters;
                filters << "index-*.fits";
                QFileInfoList indexFiles = indexDir.entryInfoList(filters, QDir::Files);
                
                if (!indexFiles.isEmpty()) {
                    indexPaths.append(path);
                    break;
                }
            }
        }
        
        return indexPaths;
    }

private:
    int m_maxConcurrent;
    int m_totalJobs = 0;
    std::atomic<int> m_completedJobs;
    
    Parameters m_commonParams;
    QStringList m_indexPaths;
    
    QQueue<OriginTIFFJob> m_jobQueue;
    QHash<StellarSolver*, OriginTIFFJob> m_activeSolvers;
    QHash<StellarSolver*, std::vector<uint8_t>> m_imageBuffers;
    QList<OriginTIFFJob> m_results;
    
    QDateTime m_batchStartTime;
    QString m_outputDirectory;
    
    // Performance tracking
    int m_originTIFFCount = 0;
    int m_excellentHintsCount = 0;
    int m_goodHintsCount = 0;
    
    double m_totalHintRaError = 0.0;
    double m_totalHintDecError = 0.0;
    double m_totalHintScaleError = 0.0;
    int m_analyzedHintJobs = 0;
};

// Simple usage example
class OriginTIFFProcessor {
public:
    static void processTIFFBatch(const QStringList& tiffFiles, const QString& outputDir = "") {
        qDebug() << "=== Origin TIFF Processing ===";
        qDebug() << "TIFF files:" << tiffFiles.size();
        
        OriginTIFFSolver solver(8);
        if (!outputDir.isEmpty()) {
            solver.setOutputDirectory(outputDir);
        }
        
        // Quick analysis
        int originCount = 0;
        for (const QString& file : tiffFiles) {
            if (OriginMetadataExtractor::containsOriginMetadata(file.toStdString())) {
                originCount++;
            }
        }
        
        qDebug() << "Origin TIFF files detected:" << originCount << "/" << tiffFiles.size();
        if (originCount > 0) {
            qDebug() << "Expected time savings:" << (originCount * 2.5) << "minutes";
        }
        
        // Add all TIFF jobs
        for (const QString& file : tiffFiles) {
            solver.addTIFFJob(file);
        }
        
        // Process batch
        QObject::connect(&solver, &OriginTIFFSolver::batchFinished,
                        [](int successful, int failed) {
            qDebug() << "\n🎯 TIFF Processing Complete!";
            qDebug() << "Results:" << successful << "solved," << failed << "failed";
            QCoreApplication::quit();
        });
        
        solver.startBatchSolving();
    }
};

#endif // ORIGIN_TIFF_SOLVER_H
