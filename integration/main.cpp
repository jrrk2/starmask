// main.cpp - Origin TIFF Solver with StellarSolver integration
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QJsonObject>
#include <QFileInfo>
#include <QDebug>
#include <QTimer>
#include <QEventLoop>
#include <iostream>

#include "OriginMetadataExtractor.h"
#include "OriginStellarSolverInterface.h"

// StellarSolver includes
#include <stellarsolver.h>
#include <parameters.h>
#include <structuredefinitions.h>

class OriginTIFFSolverApp : public QObject
{
    Q_OBJECT

public:
    OriginTIFFSolverApp(QObject* parent = nullptr);
    
    int processFiles(const QStringList& tiffFiles, const QString& outputDir, 
                    int numThreads, bool verbose);
    void showOriginInfo(const QString& tiffFile);

private slots:
    void onSolverFinished();
    void onBatchComplete();

private:
    struct SolveJob {
        QString filename;
        OriginStellarSolverJob originJob;
        StellarSolver* solver = nullptr;
        QDateTime startTime;
        bool completed = false;
        bool successful = false;
    };

    void startNextJob();
    void setupSolverParameters();
    QStringList findIndexFiles();
    void finalizeBatch();

    // Processing state
    QList<SolveJob> m_jobQueue;
    QList<SolveJob> m_activeSolvers;
    QList<SolveJob> m_completedJobs;
    
    OriginStellarSolverInterface* m_originInterface;
    Parameters m_solverParams;
    QStringList m_indexPaths;
    QString m_outputDirectory;
    int m_maxConcurrent = 4;
    bool m_verboseLogging = false;
    
    // Statistics
    int m_totalJobs = 0;
    int m_completedCount = 0;
    int m_successfulCount = 0;
    int m_originHintJobs = 0;
    QDateTime m_batchStartTime;
    
    // Event loop for synchronous operation
    QEventLoop* m_eventLoop = nullptr;
    bool m_processingComplete = false;
};

OriginTIFFSolverApp::OriginTIFFSolverApp(QObject* parent)
    : QObject(parent)
{
    m_originInterface = new OriginStellarSolverInterface(this);
    
    connect(m_originInterface, &OriginStellarSolverInterface::originHintsExtracted,
            this, [this](const OriginStellarSolverJob& job) {
        if (m_verboseLogging) {
            qDebug() << "Origin hints extracted for job" << job.jobId << ":" << job.objectName;
        }
    });
    
    setupSolverParameters();
}

int OriginTIFFSolverApp::processFiles(const QStringList& tiffFiles, const QString& outputDir, 
                                     int numThreads, bool verbose)
{
    m_outputDirectory = outputDir;
    m_maxConcurrent = numThreads;
    m_verboseLogging = verbose;
    m_totalJobs = tiffFiles.size();
    m_completedCount = 0;
    m_successfulCount = 0;
    m_originHintJobs = 0;
    
    std::cout << "=== Origin TIFF Plate Solving ===" << std::endl;
    std::cout << "Files to process: " << m_totalJobs << std::endl;
    std::cout << "Output directory: " << outputDir.toStdString() << std::endl;
    std::cout << "Concurrent solvers: " << numThreads << std::endl;
    std::cout << "Verbose logging: " << (verbose ? "Yes" : "No") << std::endl;
    
    // Create output directory
    QDir().mkpath(outputDir);
    
    // Prepare jobs
    m_jobQueue.clear();
    for (const QString& file : tiffFiles) {
        SolveJob job;
        job.filename = file;
        
        // Load Origin TIFF and extract hints
        if (m_originInterface->loadOriginTIFF(file, job.originJob)) {
            if (job.originJob.hasOriginHints) {
                m_originHintJobs++;
                std::cout << "✨ " << QFileInfo(file).fileName().toStdString() 
                         << ": " << job.originJob.objectName.toStdString() 
                         << " (" << job.originJob.hintQuality.toStdString() << " hints)" << std::endl;
            } else {
                std::cout << "📄 " << QFileInfo(file).fileName().toStdString() 
                         << ": Standard TIFF (no Origin metadata)" << std::endl;
            }
            
            m_jobQueue.append(job);
        } else {
            std::cout << "❌ Failed to load: " << QFileInfo(file).fileName().toStdString() << std::endl;
        }
    }
    
    std::cout << std::endl;
    std::cout << "Jobs prepared: " << m_jobQueue.size() << std::endl;
    std::cout << "With Origin hints: " << m_originHintJobs << std::endl;
    
    if (m_originHintJobs > 0) {
        double expectedSavings = m_originHintJobs * 2.5; // ~2.5 minutes per Origin file
        std::cout << "Expected time savings: " << expectedSavings << " minutes" << std::endl;
    }
    
    std::cout << std::endl;
    std::cout << "Starting plate solving..." << std::endl;
    std::cout << std::endl;
    
    // Start processing
    m_batchStartTime = QDateTime::currentDateTime();
    m_processingComplete = false;
    
    // Start initial batch of jobs
    for (int i = 0; i < std::min(m_maxConcurrent, (int)(m_jobQueue.size())); i++) {
        startNextJob();
    }
    
    // Wait for completion using event loop
    m_eventLoop = new QEventLoop(this);
    m_eventLoop->exec();
    
    // Finalize and show results
    finalizeBatch();
    
    return m_successfulCount == m_totalJobs ? 0 : 1;
}

void OriginTIFFSolverApp::startNextJob()
{
    if (m_jobQueue.isEmpty()) {
        return;
    }
    
    SolveJob job = m_jobQueue.takeFirst();
    job.startTime = QDateTime::currentDateTime();
    
    // Create StellarSolver
    StellarSolver* solver = new StellarSolver(this);
    job.solver = solver;
    
    // Configure solver
    solver->setProperty("ProcessType", SSolver::SOLVE);
    solver->setProperty("ExtractorType", SSolver::EXTRACTOR_INTERNAL);
    solver->setProperty("SolverType", SSolver::SOLVER_STELLARSOLVER);
    solver->setProperty("LogOutput", true);
    solver->setProperty("LogLevel", SSolver::LOG_ALL);

    solver->setIndexFolderPaths(m_indexPaths);
    solver->setParameters(m_solverParams);
    
    // Convert TIFF image for StellarSolver
    FITSImage::Statistic stats;
    std::vector<uint8_t> buffer;
    
    if (!m_originInterface->convertTIFFToStellarSolverFormat(job.filename, stats, buffer)) {
        std::cout << "❌ Failed to convert TIFF: " << QFileInfo(job.filename).fileName().toStdString() << std::endl;
        solver->deleteLater();
        
        job.completed = true;
        job.successful = false;
        m_completedJobs.append(job);
        m_completedCount++;
        
        QTimer::singleShot(0, this, &OriginTIFFSolverApp::onBatchComplete);
        return;
    }
    
    // Store buffer in solver (we'll manage lifetime)
    static std::map<StellarSolver*, std::vector<uint8_t>> s_bufferMap;
    s_bufferMap[solver] = std::move(buffer);
    
    // Load image into solver
    if (!solver->loadNewImageBuffer(stats, s_bufferMap[solver].data())) {
        std::cout << "❌ Failed to load image buffer: " << QFileInfo(job.filename).fileName().toStdString() << std::endl;
        s_bufferMap.erase(solver);
        solver->deleteLater();
        
        job.completed = true;
        job.successful = false;
        m_completedJobs.append(job);
        m_completedCount++;
        
        QTimer::singleShot(0, this, &OriginTIFFSolverApp::onBatchComplete);
        return;
    }
    
    // Configure with Origin hints if available
    m_originInterface->configureSolverWithOriginHints(solver, job.originJob);
    
    // Connect completion signal
    connect(solver, &StellarSolver::finished, this, &OriginTIFFSolverApp::onSolverFinished);
    
    // Add to active solvers
    m_activeSolvers.append(job);
    
    // Start solving
    std::cout << "🚀 Starting: " << QFileInfo(job.filename).fileName().toStdString();
    if (job.originJob.hasOriginHints) {
        std::cout << " (" << job.originJob.objectName.toStdString() 
                 << ", " << job.originJob.expectedSpeedup << "x speedup expected)";
    }
    std::cout << std::endl;
    
    solver->start();
}

void OriginTIFFSolverApp::onSolverFinished()
{
    StellarSolver* solver = qobject_cast<StellarSolver*>(sender());
    if (!solver) return;
    
    // Find the corresponding job
    int jobIndex = -1;
    for (int i = 0; i < m_activeSolvers.size(); i++) {
        if (m_activeSolvers[i].solver == solver) {
            jobIndex = i;
            break;
        }
    }
    
    if (jobIndex == -1) {
        solver->deleteLater();
        return;
    }
    
    SolveJob job = m_activeSolvers.takeAt(jobIndex);
    job.completed = true;
    
    qint64 totalMs = job.startTime.msecsTo(QDateTime::currentDateTime());
    double totalTime = totalMs / 1000.0;
    
    QFileInfo fileInfo(job.filename);
    
    if (solver->solvingDone() && solver->hasWCSData()) {
        FITSImage::Solution solution = solver->getSolution();
        job.successful = true;
        
        std::cout << "✅ SOLVED: " << fileInfo.fileName().toStdString();
        std::cout << " (" << totalTime << "s)" << std::endl;
        std::cout << "   RA: " << solution.ra << "°, Dec: " << solution.dec << "°" << std::endl;
        std::cout << "   Scale: " << solution.pixscale << " arcsec/pixel" << std::endl;
        std::cout << "   Orientation: " << solution.orientation << "°" << std::endl;
        
        // Analyze Origin hint accuracy if available
        if (job.originJob.hasOriginHints) {
            m_originInterface->analyzeResults(job.originJob, solution);
        }
        
        // Create output filename
        QString outputFile = QString("%1/solved_%2.json").arg(m_outputDirectory).arg(fileInfo.baseName());
        
        // Write solution to JSON file
        QJsonObject solutionJson;
        solutionJson["input_file"] = job.filename;
        solutionJson["object_name"] = job.originJob.objectName;
        solutionJson["solved_ra"] = solution.ra;
        solutionJson["solved_dec"] = solution.dec;
        solutionJson["pixel_scale"] = solution.pixscale;
        solutionJson["orientation"] = solution.orientation;
        solutionJson["field_width"] = solution.fieldWidth;
        solutionJson["field_height"] = solution.fieldHeight;
        solutionJson["solve_time"] = totalTime;
        solutionJson["accelerated"] = job.originJob.hasOriginHints;
        solutionJson["hint_quality"] = job.originJob.hintQuality;
        
        if (job.originJob.hasOriginHints) {
            QJsonObject hints;
            hints["ra"] = job.originJob.hintRA;
            hints["dec"] = job.originJob.hintDec;
            hints["pixel_scale"] = job.originJob.hintPixelScale;
            hints["expected_speedup"] = job.originJob.expectedSpeedup;
            solutionJson["origin_hints"] = hints;
        }
        
        QFile file(outputFile);
        if (file.open(QIODevice::WriteOnly)) {
            QJsonDocument doc(solutionJson);
            file.write(doc.toJson());
            file.close();
            std::cout << "   💾 Solution saved: " << QFileInfo(outputFile).fileName().toStdString() << std::endl;
        }
        
        m_successfulCount++;
        
    } else {
        job.successful = false;
        std::cout << "❌ FAILED: " << fileInfo.fileName().toStdString();
        std::cout << " (" << totalTime << "s)" << std::endl;
        
        if (job.originJob.hasOriginHints) {
            std::cout << "   (Despite " << job.originJob.hintQuality.toStdString() << " Origin hints)" << std::endl;
        }
    }
    
    m_completedJobs.append(job);
    m_completedCount++;
    
    // Clean up solver
    static std::map<StellarSolver*, std::vector<uint8_t>> s_bufferMap;
    s_bufferMap.erase(solver);
    solver->deleteLater();
    
    std::cout << std::endl;
    
    // Check if we should start another job or finish
    QTimer::singleShot(0, this, &OriginTIFFSolverApp::onBatchComplete);
}

void OriginTIFFSolverApp::onBatchComplete()
{
    // Start next job if available
    if (!m_jobQueue.isEmpty() && m_activeSolvers.size() < m_maxConcurrent) {
        startNextJob();
        return;
    }
    
    // Check if all jobs are complete
    if (m_activeSolvers.isEmpty() && m_jobQueue.isEmpty()) {
        m_processingComplete = true;
        if (m_eventLoop) {
            m_eventLoop->quit();
        }
    }
}

void OriginTIFFSolverApp::finalizeBatch()
{
    qint64 totalBatchTime = m_batchStartTime.msecsTo(QDateTime::currentDateTime());
    double batchTimeSeconds = totalBatchTime / 1000.0;
    
    std::cout << "=== Batch Processing Complete ===" << std::endl;
    std::cout << "Total time: " << batchTimeSeconds << " seconds" << std::endl;
    std::cout << "Total jobs: " << m_totalJobs << std::endl;
    std::cout << "Successful: " << m_successfulCount << std::endl;
    std::cout << "Failed: " << (m_totalJobs - m_successfulCount) << std::endl;
    
    if (m_originHintJobs > 0) {
        int originSuccessful = 0;
        int standardSuccessful = 0;
        double originSolveTime = 0.0;
        double standardSolveTime = 0.0;
        
        for (const SolveJob& job : m_completedJobs) {
            if (job.successful) {
                double jobTime = job.startTime.msecsTo(QDateTime::currentDateTime()) / 1000.0;
                
                if (job.originJob.hasOriginHints) {
                    originSuccessful++;
                    originSolveTime += jobTime;
                } else {
                    standardSuccessful++;
                    standardSolveTime += jobTime;
                }
            }
        }
        
        std::cout << std::endl;
        std::cout << "Performance Analysis:" << std::endl;
        std::cout << "Origin TIFF files: " << originSuccessful << "/" << m_originHintJobs << " successful" << std::endl;
        std::cout << "Standard files: " << standardSuccessful << "/" << (m_totalJobs - m_originHintJobs) << " successful" << std::endl;
        
        if (originSuccessful > 0) {
            double avgOriginTime = originSolveTime / originSuccessful;
            std::cout << "Avg Origin solve time: " << avgOriginTime << "s" << std::endl;
            
            if (standardSuccessful > 0) {
                double avgStandardTime = standardSolveTime / standardSuccessful;
                double actualSpeedup = avgStandardTime / avgOriginTime;
                std::cout << "Avg standard solve time: " << avgStandardTime << "s" << std::endl;
                std::cout << "Actual speedup factor: " << actualSpeedup << "x" << std::endl;
            }
        }
    }
    
    if (m_successfulCount > 0) {
        std::cout << std::endl;
        std::cout << "✅ Processing completed successfully!" << std::endl;
        std::cout << "Solution files saved in: " << m_outputDirectory.toStdString() << std::endl;
    } else {
        std::cout << std::endl;
        std::cout << "❌ No files were successfully solved." << std::endl;
    }
}

void OriginTIFFSolverApp::setupSolverParameters()
{
    // Get built-in profiles
    QList<Parameters> profiles = StellarSolver::getBuiltInProfiles();
    if (profiles.isEmpty()) {
        std::cerr << "ERROR: No StellarSolver parameter profiles available" << std::endl;
        return;
    }

    m_solverParams = profiles.at(0); // Use first profile as base

    // Configure for TIFF solving with Origin hints
    m_solverParams.multiAlgorithm = SSolver::MULTI_AUTO;
    m_solverParams.search_radius = 1.0;      // Will be adjusted per job based on hints
    m_solverParams.minwidth = 0.1;
    m_solverParams.maxwidth = 10.0;
    m_solverParams.resort = true;
    m_solverParams.autoDownsample = false;
    m_solverParams.downsample = 1;
    m_solverParams.inParallel = true;
    m_solverParams.solverTimeLimit = 120;    // Will be adjusted based on hint quality

    // Star extraction parameters optimized for TIFF
    m_solverParams.initialKeep = 1500;
    m_solverParams.keepNum = 400;
    m_solverParams.r_min = 1.0;
    m_solverParams.removeBrightest = 0;
    m_solverParams.removeDimmest = 50;
    m_solverParams.saturationLimit = 65000;
    m_solverParams.minarea = 5;

    // Find index files
    m_indexPaths = findIndexFiles();
    if (m_indexPaths.isEmpty()) {
        std::cerr << "ERROR: No astrometry index files found!" << std::endl;
        std::cerr << "Please install astrometry.net index files" << std::endl;
    } else {
        std::cout << "Using index files from: " << m_indexPaths.first().toStdString() << std::endl;
    }
}

QStringList OriginTIFFSolverApp::findIndexFiles()
{
    QStringList indexPaths;
    QStringList searchPaths = {
        "/usr/local/astrometry/data",
        "/opt/homebrew/share/astrometry", 
        "/usr/local/share/astrometry",
        "/usr/share/astrometry"
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

void OriginTIFFSolverApp::showOriginInfo(const QString& tiffFile)
{
    QFileInfo fileInfo(tiffFile);
    std::cout << "=== " << fileInfo.fileName().toStdString() << " ===" << std::endl;
    
    OriginMetadataExtractor extractor;
    extractor.setVerboseLogging(false);
    
    if (extractor.extractFromTIFFFile(tiffFile.toStdString())) {
        const OriginTelescopeMetadata& meta = extractor.getMetadata();
        
        if (meta.isValid) {
            std::cout << "Object: " << meta.objectName << std::endl;
            std::cout << "Coordinates: RA " << meta.centerRA << "°, Dec " << meta.centerDec << "°" << std::endl;
            std::cout << "Field of view: " << meta.fieldOfViewX << "° × " << meta.fieldOfViewY << "°" << std::endl;
            std::cout << "Image size: " << meta.imageWidth << " × " << meta.imageHeight << " pixels" << std::endl;
            std::cout << "Pixel scale: " << meta.getAveragePixelScale() << " arcsec/pixel" << std::endl;
            std::cout << "Orientation: " << meta.orientation << "°" << std::endl;
            std::cout << "Date/Time: " << meta.dateTime << std::endl;
            std::cout << "Filter: " << meta.filter << std::endl;
            std::cout << "Exposure: " << meta.exposure << "s × " << meta.stackedDepth << " frames" << std::endl;
            std::cout << "Total exposure: " << meta.getTotalExposureSeconds() << " seconds" << std::endl;
            std::cout << "ISO: " << meta.iso << std::endl;
            std::cout << "Temperature: " << meta.temperature << "°C" << std::endl;
            
            if (meta.gpsLatitude != 0.0 || meta.gpsLongitude != 0.0) {
                std::cout << "GPS: " << meta.gpsLatitude << "°, " << meta.gpsLongitude 
                         << "° (alt: " << meta.gpsAltitude << "m)" << std::endl;
            }
            
            std::cout << "UUID: " << meta.uuid << std::endl;
            std::cout << "Status: ✅ Valid Origin metadata" << std::endl;
            
            // Quality assessment
            QString quality = OriginStellarSolverInterface::assessHintQuality(meta);
            double expectedSpeedup = OriginStellarSolverInterface::getExpectedSpeedupForQuality(quality);
            
            std::cout << "Hint quality: " << quality.toStdString() << std::endl;
            std::cout << "Expected speedup: " << expectedSpeedup << "x" << std::endl;
            
        } else {
            std::cout << "Status: ❌ Invalid Origin metadata found" << std::endl;
        }
    } else {
        std::cout << "Status: ℹ️ No Origin metadata in this TIFF file" << std::endl;
    }
    
    std::cout << std::endl;
}

// Main function
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("Origin TIFF Solver");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("Astronomy Tools");

    QCommandLineParser parser;
    parser.setApplicationDescription("Plate solver for Origin telescope TIFF files with metadata acceleration");
    parser.addHelpOption();
    parser.addVersionOption();

    // Command line options
    QCommandLineOption inputOption(
        QStringList() << "i" << "input",
        "Input TIFF file or directory",
        "path");
    parser.addOption(inputOption);

    QCommandLineOption outputOption(
        QStringList() << "o" << "output",
        "Output directory (optional, defaults to input directory)",
        "path");
    parser.addOption(outputOption);

    QCommandLineOption infoOption(
        QStringList() << "info",
        "Show Origin metadata information only (no solving)");
    parser.addOption(infoOption);

    QCommandLineOption threadsOption(
        QStringList() << "t" << "threads",
        "Number of concurrent solvers (default: 4)",
        "count", "4");
    parser.addOption(threadsOption);

    QCommandLineOption verboseOption(
        QStringList() << "verbose",
        "Verbose output");
    parser.addOption(verboseOption);

    // Parse command line
    parser.process(app);

    // Validate input
    QString inputPath = parser.value(inputOption);
    if (inputPath.isEmpty()) {
        std::cerr << "Error: Input path is required. Use --help for usage information." << std::endl;
        return 1;
    }

    // Collect TIFF files
    QStringList tiffFiles;
    QFileInfo inputInfo(inputPath);

    if (!inputInfo.exists()) {
        std::cerr << "Error: Input path does not exist: " << inputPath.toStdString() << std::endl;
        return 1;
    }

    if (inputInfo.isDir()) {
        QDir inputDir(inputPath);
        QStringList filters;
        filters << "*.tiff" << "*.tif" << "*.TIFF" << "*.TIF";
        
        QFileInfoList files = inputDir.entryInfoList(filters, QDir::Files);
        for (const QFileInfo& file : files) {
            tiffFiles << file.absoluteFilePath();
        }
    } else if (inputInfo.isFile()) {
        tiffFiles << inputPath;
    }

    if (tiffFiles.isEmpty()) {
        std::cerr << "No TIFF files found in: " << inputPath.toStdString() << std::endl;
        return 1;
    }

    // Create solver app
    OriginTIFFSolverApp solverApp;

    // Info mode - just show metadata
    if (parser.isSet(infoOption)) {
        for (const QString& file : tiffFiles) {
            solverApp.showOriginInfo(file);
        }
        return 0;
    }

    // Solving mode
    QString outputDir = parser.value(outputOption);
    if (outputDir.isEmpty()) {
        outputDir = inputInfo.isDir() ? inputPath : inputInfo.absolutePath();
    }

    int numThreads = parser.value(threadsOption).toInt();
    bool verbose = parser.isSet(verboseOption);

    // Process files with actual StellarSolver
    return solverApp.processFiles(tiffFiles, outputDir, numThreads, verbose);
}

#include "main.moc"
