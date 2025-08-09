
// =============================================================================
// Usage Examples
// =============================================================================

/*
// Example 1: Simple TIFF batch processing
int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    QStringList tiffFiles;
    tiffFiles << "/path/to/FinalStackedMaster.tiff"
              << "/path/to/another_origin_file.tiff"
              << "/path/to/regular_tiff.tiff";
    
    OriginTIFFProcessor::processTIFFBatch(tiffFiles, "./solved_output");
    
    return app.exec();
}

// Example 2: Command line TIFF processor
int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    QCommandLineParser parser;
    parser.setApplicationDescription("Origin TIFF plate solver");
    parser.addHelpOption();
    
    QCommandLineOption inputOption(
        QStringList() << "i" << "input",
        "Input TIFF directory or file",
        "path");
    parser.addOption(inputOption);
    
    QCommandLineOption outputOption(
        QStringList() << "o" << "output",
        "Output directory (optional)",
        "path");
    parser.addOption(outputOption);
    
    QCommandLineOption threadsOption(
        QStringList() << "t" << "threads",
        "Number of concurrent solvers (default: 8)",
        "count", "8");
    parser.addOption(threadsOption);
    
    parser.process(app);
    
    QString inputPath = parser.value(inputOption);
    if (inputPath.isEmpty()) {
        qDebug() << "Error: Input path required";
        parser.showHelp(1);
    }
    
    // Collect TIFF files
    QStringList tiffFiles;
    QFileInfo inputInfo(inputPath);
    
    if (inputInfo.isDir()) {
        QDir inputDir(inputPath);
        QStringList filters;
        filters << "*.tiff" << "*.tif" << "*.TIFF" << "*.TIF";
        
        QFileInfoList files = inputDir.entryInfoList(filters, QDir::Files);
        for (const QFileInfo& file : files) {
            tiffFiles << file.absoluteFilePath();
        }
    } else if (inputInfo.exists()) {
        tiffFiles << inputPath;
    } else {
        qDebug() << "Error: Input path does not exist:" << inputPath.toStdString();
        return 1;
    }
    
    if (tiffFiles.isEmpty()) {
        qDebug() << "No TIFF files found in" << inputPath.toStdString();
        return 1;
    }
    
    QString outputDir = parser.value(outputOption);
    int numThreads = parser.value(threadsOption).toInt();
    
    qDebug() << "=== Origin TIFF Plate Solver ===";
    qDebug() << "TIFF files:" << tiffFiles.size();
    qDebug() << "Output directory:" << (outputDir.isEmpty() ? "In-place" : outputDir.toStdString());
    qDebug() << "Threads:" << numThreads;
    
    // Quick Origin detection scan
    int originCount = 0;
    for (const QString& file : tiffFiles) {
        if (OriginMetadataExtractor::containsOriginMetadata(file.toStdString())) {
            originCount++;
        }
    }
    
    qDebug() << "Origin TIFF files:" << originCount << "/" << tiffFiles.size();
    if (originCount > 0) {
        double expectedSavings = originCount * 2.5; // ~2.5 minutes per Origin file
        qDebug() << "Expected time savings:" << expectedSavings << "minutes";
    }
    qDebug();
    
    // Create and configure solver
    OriginTIFFSolver solver(numThreads);
    if (!outputDir.isEmpty()) {
        solver.setOutputDirectory(outputDir);
    }
    
    // Connect completion
    QObject::connect(&solver, &OriginTIFFSolver::batchFinished,
                     [&app](int successful, int failed) {
        qDebug() << "\n=== PROCESSING COMPLETE ===";
        qDebug() << "Successfully solved:" << successful;
        qDebug() << "Failed:" << failed;
        app.quit();
    });
    
    // Add all TIFF jobs
    for (const QString& file : tiffFiles) {
        solver.addTIFFJob(file);
    }
    
    // Start processing
    solver.startBatchSolving();
    
    return app.exec();
}

// Example 3: Integration with your existing Qt application
class MyTIFFProcessor : public QObject {
    Q_OBJECT
    
public:
    void processTIFFDirectory(const QString& dirPath) {
        // Scan for TIFF files
        QDir dir(dirPath);
        QStringList filters;
        filters << "*.tiff" << "*.tif";
        
        QStringList tiffFiles;
        QFileInfoList files = dir.entryInfoList(filters, QDir::Files);
        for (const QFileInfo& file : files) {
            tiffFiles << file.absoluteFilePath();
        }
        
        if (tiffFiles.isEmpty()) {
            qDebug() << "No TIFF files found in" << dirPath.toStdString();
            return;
        }
        
        // Create solver
        m_solver = new OriginTIFFSolver(6, this); // 6 concurrent
        m_solver->setOutputDirectory("./solved_tiffs");
        
        // Connect signals
        connect(m_solver, &OriginTIFFSolver::progressChanged,
                this, &MyTIFFProcessor::onProgress);
        
        connect(m_solver, &OriginTIFFSolver::statusChanged,
                this, &MyTIFFProcessor::onStatusUpdate);
        
        connect(m_solver, &OriginTIFFSolver::batchFinished,
                this, &MyTIFFProcessor::onBatchComplete);
        
        // Add jobs and start
        for (const QString& file : tiffFiles) {
            m_solver->addTIFFJob(file);
        }
        
        m_solver->startBatchSolving();
    }
    
private slots:
    void onProgress(int completed, int total) {
        qDebug() << "Progress:" << completed << "/" << total << "completed";
        // Update your UI progress bar here
    }
    
    void onStatusUpdate(const QString& status) {
        qDebug() << "Status:" << status.toStdString();
        // Update your UI status label here
    }
    
    void onBatchComplete(int successful, int failed) {
        qDebug() << "Batch complete:" << successful << "successful," << failed << "failed";
        
        // Clean up
        m_solver->deleteLater();
        m_solver = nullptr;
        
        // Notify completion
        emit processingComplete(successful, failed);
    }
    
signals:
    void processingComplete(int successful, int failed);
    
private:
    OriginTIFFSolver* m_solver = nullptr;
};
