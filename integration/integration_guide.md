# Origin TIFF Reader Integration Guide

This guide shows how to integrate the Enhanced Origin TIFF Reader into your existing CMake-based Qt application for automated RA/DEC catalog lookups and plate solving.

## 1. Project Structure Integration

### CMakeLists.txt Updates

Add the Origin TIFF reader to your existing CMake configuration:

```cmake
# Add Origin TIFF reader sources
set(ORIGIN_TIFF_SOURCES
    src/OriginTIFFReader.h
    src/OriginTIFFReader.cpp
)

# Add to your existing target
target_sources(${PROJECT_NAME} PRIVATE
    ${ORIGIN_TIFF_SOURCES}
    # ... your existing sources
)

# Ensure required libraries are linked
target_link_libraries(${PROJECT_NAME} 
    ${TIFF_LIBRARIES}
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Network  # For catalog queries
    # ... your existing libraries
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${TIFF_INCLUDE_DIRS}
)
```

## 2. ImageReader Integration

### Enhanced ImageReader.h

Add Origin support to your existing `ImageReader` class:

```cpp
// Add to ImageReader.h
#include "OriginTIFFReader.h"

class ImageReader : public QObject {
    Q_OBJECT
    
public:
    // Existing methods...
    
    // New Origin-specific methods
    bool readOriginTIFF(const QString& filePath);
    bool hasOriginMetadata() const { return m_originMetadata.isValid; }
    const OriginMetadata& getOriginMetadata() const { return m_originMetadata; }
    const CatalogLookupResult& getCatalogResults() const { return m_catalogResults; }

signals:
    // Existing signals...
    
    // New Origin-specific signals
    void originMetadataFound(const OriginMetadata& metadata);
    void catalogLookupCompleted(const CatalogLookupResult& results);
    void plateSolvingCompleted(bool successful, double rmsError);

private:
    // Existing members...
    
    // New Origin integration
    OriginTIFFReader* m_originReader = nullptr;
    OriginMetadata m_originMetadata;
    CatalogLookupResult m_catalogResults;
    StarCatalogValidator* m_catalogValidator = nullptr;
};
```

### Enhanced ImageReader.cpp

```cpp
// Add to ImageReader.cpp
bool ImageReader::readOriginTIFF(const QString& filePath)
{
    // Clean up any existing Origin reader
    if (m_originReader) {
        delete m_originReader;
    }
    
    m_originReader = new OriginTIFFReader(this);
    
    // Connect signals for async processing
    connect(m_originReader, &OriginTIFFReader::metadataExtracted,
            this, [this](const OriginMetadata& metadata) {
        m_originMetadata = metadata;
        qDebug() << "Origin metadata extracted for:" << metadata.objectName;
        qDebug() << "Target: RA=" << metadata.centerRA << "° Dec=" << metadata.centerDec << "°";
        emit originMetadataFound(metadata);
    });
    
    connect(m_originReader, &OriginTIFFReader::catalogLookupCompleted,
            this, [this](const CatalogLookupResult& result) {
        m_catalogResults = result;
        qDebug() << "Catalog lookup completed:" << result.totalStarsFound << "stars found";
        
        if (result.plateSolveSuccessful) {
            qDebug() << "Plate solving successful! RMS error:" << result.solvingErrorArcsec << "arcsec";
            emit plateSolvingCompleted(true, result.solvingErrorArcsec);
        } else if (result.plateSolveAttempted) {
            qDebug() << "Plate solving failed. RMS error:" << result.solvingErrorArcsec << "arcsec";
            emit plateSolvingCompleted(false, result.solvingErrorArcsec);
        }
        
        emit catalogLookupCompleted(result);
    });
    
    connect(m_originReader, &OriginTIFFReader::errorOccurred,
            this, [this](const QString& error) {
        qDebug() << "Origin TIFF Reader error:" << error;
        // Handle error appropriately
    });
    
    // Load the TIFF file
    if (!m_originReader->loadTIFFFile(filePath)) {
        qDebug() << "Failed to load Origin TIFF file:" << m_originReader->getLastError();
        return false;
    }
    
    // Also try to load the image data using existing TIFF reader
    bool imageLoaded = readTIFF(filePath); // Your existing method
    
    // If Origin metadata was found, perform catalog lookup
    if (m_originReader->hasOriginMetadata()) {
        qDebug() << "Origin metadata found, initiating catalog lookup...";
        
        // Create catalog validator if needed
        if (!m_catalogValidator) {
            m_catalogValidator = new StarCatalogValidator(this);
        }
        
        // Configure for Origin telescope data
        m_originReader->setMaxCatalogStars(2000);
        m_originReader->setSearchRadiusMultiplier(2.0); // Generous search radius
        m_originReader->setEnablePlateSolving(true);
        m_originReader->setVerboseLogging(true);
        
        // Start async catalog lookup
        m_originReader->performCatalogLookup(m_catalogValidator);
    }
    
    return imageLoaded;
}
```

## 3. MainWindow Integration

### Enhanced MainWindow.h

Add Origin TIFF support to your main window:

```cpp
// Add to MainWindow.h
private slots:
    // Existing slots...
    
    // New Origin-specific slots
    void onLoadOriginTIFF();
    void onOriginMetadataFound(const OriginMetadata& metadata);
    void onCatalogLookupCompleted(const CatalogLookupResult& results);
    void onPlateSolvingCompleted(bool successful, double rmsError);
    void onExportOriginResults();

private:
    // Existing members...
    
    // New UI elements for Origin integration
    QAction* m_loadOriginTIFFAction = nullptr;
    QLabel* m_originStatusLabel = nullptr;
    QLabel* m_catalogStatusLabel = nullptr;
    QLabel* m_plateSolveStatusLabel = nullptr;
    QPushButton* m_exportOriginResultsButton = nullptr;
    
    // Data
    bool m_hasOriginMetadata = false;
    bool m_hasCatalogResults = false;
};
```

### Enhanced MainWindow.cpp

```cpp
// Add to MainWindow constructor
void MainWindow::setupOriginTIFFIntegration()
{
    // Add menu item for Origin TIFF files
    m_loadOriginTIFFAction = new QAction("Load Origin TIFF...", this);
    m_loadOriginTIFFAction->setShortcut(QKeySequence("Ctrl+Shift+O"));
    m_loadOriginTIFFAction->setToolTip("Load TIFF file from Origin telescope with automatic catalog lookup");
    connect(m_loadOriginTIFFAction, &QAction::triggered, this, &MainWindow::onLoadOriginTIFF);
    
    // Add to File menu
    QMenu* fileMenu = menuBar()->findChild<QMenu*>("fileMenu");
    if (fileMenu) {
        fileMenu->addSeparator();
        fileMenu->addAction(m_loadOriginTIFFAction);
    }
    
    // Add status labels to status bar or info panel
    m_originStatusLabel = new QLabel("Origin: Not loaded");
    m_catalogStatusLabel = new QLabel("Catalog: N/A");
    m_plateSolveStatusLabel = new QLabel("Plate Solve: N/A");
    
    // Add export button
    m_exportOriginResultsButton = new QPushButton("Export Origin Results");
    m_exportOriginResultsButton->setEnabled(false);
    connect(m_exportOriginResultsButton, &QPushButton::clicked, 
            this, &MainWindow::onExportOriginResults);
    
    // Connect ImageReader signals
    connect(m_imageReader, &ImageReader::originMetadataFound,
            this, &MainWindow::onOriginMetadataFound);
    connect(m_imageReader, &ImageReader::catalogLookupCompleted,
            this, &MainWindow::onCatalogLookupCompleted);
    connect(m_imageReader, &ImageReader::plateSolvingCompleted,
            this, &MainWindow::onPlateSolvingCompleted);
}

void MainWindow::onLoadOriginTIFF()
{
    QString fileName = QFileDialog::getOpenFileName(
        this, "Open Origin TIFF File",
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
        "Origin TIFF Files (*.tiff *.tif);;All Files (*)");
    
    if (fileName.isEmpty()) return;
    
    // Show progress
    m_statusLabel->setText("Loading Origin TIFF file...");
    m_originStatusLabel->setText("Origin: Loading...");
    
    // Load with Origin support
    if (m_imageReader->readOriginTIFF(fileName)) {
        m_statusLabel->setText("Origin TIFF loaded, processing metadata...");
    } else {
        QMessageBox::warning(this, "Load Error", 
                           "Failed to load Origin TIFF file.");
        m_statusLabel->setText("Ready");
        m_originStatusLabel->setText("Origin: Load failed");
    }
}

void MainWindow::onOriginMetadataFound(const OriginMetadata& metadata)
{
    m_hasOriginMetadata = true;
    
    m_originStatusLabel->setText(QString("Origin: %1").arg(metadata.objectName));
    m_statusLabel->setText("Origin metadata found, querying catalog...");
    
    // Update coordinate display
    updateCoordinateDisplay(metadata.centerRA, metadata.centerDec);
    
    // Log detailed info
    qDebug() << "=== Origin Telescope Metadata ===";
    qDebug() << "Object:" << metadata.objectName;
    qDebug() << "Coordinates: RA =" << metadata.centerRA << "°, Dec =" << metadata.centerDec << "°";
    qDebug() << "Field of View:" << metadata.fieldOfViewX << "° ×" << metadata.fieldOfViewY << "°";
    qDebug() << "Pixel Scale:" << metadata.getAveragePixelScale() << "arcsec/pixel";
    qDebug() << "Image Size:" << metadata.imageWidth << "×" << metadata.imageHeight;
    qDebug() << "Stacked Frames:" << metadata.stackedDepth;
    qDebug() << "Total Exposure:" << metadata.totalDurationMs / 1000.0 << "seconds";
}

void MainWindow::onCatalogLookupCompleted(const CatalogLookupResult& results)
{
    m_hasCatalogResults = results.success;
    
    if (results.success) {
        m_catalogStatusLabel->setText(QString("Catalog: %1 stars").arg(results.totalStarsFound));
        m_statusLabel->setText(QString("Catalog lookup complete: %1 stars found").arg(results.totalStarsFound));
        
        // Enable export
        m_exportOriginResultsButton->setEnabled(true);
        
        // If we have WCS information, update the plotting system
        if (results.plateSolveSuccessful) {
            updateWCSInfo(results.solvedRA, results.solvedDec, 
                         results.solvedPixelScale, results.solvedOrientation);
        }
        
        qDebug() << "Catalog lookup successful:";
        qDebug() << "  Stars found:" << results.totalStarsFound;
        qDebug() << "  Search radius:" << results.searchRadiusDegrees << "°";
        qDebug() << "  Catalog:" << results.catalogUsed;
        
    } else {
        m_catalogStatusLabel->setText("Catalog: Failed");
        m_statusLabel->setText(QString("Catalog lookup failed: %1").arg(results.errorMessage));
        qDebug() << "Catalog lookup failed:" << results.errorMessage;
    }
}

void MainWindow::onPlateSolvingCompleted(bool successful, double rmsError)
{
    if (successful) {
        m_plateSolveStatusLabel->setText(QString("Plate Solve: ✓ (%.1f\")").arg(rmsError));
        m_statusLabel->setText(QString("Plate solving successful! RMS error: %.1f arcsec").arg(rmsError));
        
        // Enable enhanced features that require accurate WCS
        enableAdvancedAstrometry();
        
        qDebug() << "Plate solving successful! RMS error:" << rmsError << "arcsec";
        
    } else {
        m_plateSolveStatusLabel->setText(QString("Plate Solve: ✗ (%.1f\")").arg(rmsError));
        m_statusLabel->setText(QString("Plate solving failed. RMS error: %.1f arcsec").arg(rmsError));
        
        qDebug() << "Plate solving failed. RMS error:" << rmsError << "arcsec";
    }
}

void MainWindow::onExportOriginResults()
{
    if (!m_hasOriginMetadata) {
        QMessageBox::information(this, "Export Results", 
                                "No Origin metadata available to export.");
        return;
    }
    
    QString fileName = QFileDialog::getSaveFileName(
        this, "Export Origin TIFF Results",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/origin_results.json",
        "JSON Files (*.json);;Text Files (*.txt);;All Files (*)");
    
    if (fileName.isEmpty()) return;
    
    QFileInfo fileInfo(fileName);
    QString suffix = fileInfo.suffix().toLower();
    
    try {
        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::critical(this, "Export Error", 
                                 "Could not open file for writing.");
            return;
        }
        
        QTextStream out(&file);
        
        if (suffix == "json") {
            // Export as JSON
            QJsonDocument doc(m_imageReader->getOriginMetadata().exportAsJson());
            out << doc.toJson();
        } else {
            // Export as text
            out << m_imageReader->getOriginMetadata().exportAsText();
        }
        
        QMessageBox::information(this, "Export Complete", 
                               QString("Results exported to: %1").arg(fileName));
        
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Export Error", 
                             QString("Error exporting results: %1").arg(e.what()));
    }
}
```

## 4. Testing and Validation

### Test Cases

Create test cases to validate the integration:

```cpp
// test_origin_integration.cpp
#include <QtTest>
#include "OriginTIFFReader.h"

class TestOriginIntegration : public QObject
{
    Q_OBJECT

private slots:
    void testMetadataExtraction();
    void testCatalogLookup();
    void testPlateSolving();
    void testErrorHandling();

private:
    QString m_testFilePath = "test_data/FinalStackedMaster.tiff";
};

void TestOriginIntegration::testMetadataExtraction()
{
    OriginTIFFReader reader;
    
    // Test loading the TIFF file
    QVERIFY(reader.loadTIFFFile(m_testFilePath));
    
    // Test metadata extraction
    QVERIFY(reader.hasOriginMetadata());
    
    const OriginMetadata& metadata = reader.getOriginMetadata();
    QVERIFY(metadata.isValid);
    
    // Verify expected values from your test file
    QCOMPARE(metadata.objectName, QString("Whirlpool Galaxy - M 51"));
    QVERIFY(qAbs(metadata.centerRA - 202.469) < 0.1); // Approximate RA for M51
    QVERIFY(qAbs(metadata.centerDec - 47.195) < 0.1);  // Approximate Dec for M51
    QCOMPARE(metadata.imageWidth, 3056);
    QCOMPARE(metadata.imageHeight, 2048);
    QCOMPARE(metadata.stackedDepth, 480);
    
    // Test calculated values
    QVERIFY(metadata.getAveragePixelScale() > 0);
    QVERIFY(!metadata.getReadableExposureInfo().isEmpty());
}

void TestOriginIntegration::testCatalogLookup()
{
    OriginTIFFReader reader;
    StarCatalogValidator validator;
    
    QVERIFY(reader.loadTIFFFile(m_testFilePath));
    QVERIFY(reader.hasOriginMetadata());
    
    // Setup catalog lookup
    QSignalSpy spy(&reader, &OriginTIFFReader::catalogLookupCompleted);
    
    reader.setMaxCatalogStars(100);
    reader.setSearchRadiusMultiplier(1.5);
    QVERIFY(reader.performCatalogLookup(&validator));
    
    // Wait for async completion
    QVERIFY(spy.wait(10000)); // 10 second timeout
    
    const CatalogLookupResult& result = reader.getCatalogResult();
    QVERIFY(result.success);
    QVERIFY(result.totalStarsFound > 0);
}

void TestOriginIntegration::testPlateSolving()
{
    OriginTIFFReader reader;
    StarCatalogValidator validator;
    
    reader.setEnablePlateSolving(true);
    QVERIFY(reader.loadTIFFFile(m_testFilePath));
    QVERIFY(reader.performCatalogLookup(&validator));
    
    // Wait for plate solving to complete
    QSignalSpy spy(&reader, &OriginTIFFReader::catalogLookupCompleted);
    QVERIFY(spy.wait(15000)); // 15 second timeout for plate solving
    
    const CatalogLookupResult& result = reader.getCatalogResult();
    if (result.plateSolveAttempted) {
        // If plate solving was attempted, verify results
        if (result.plateSolveSuccessful) {
            QVERIFY(result.solvingErrorArcsec < 30.0); // Should be better than 30 arcsec
            QVERIFY(result.matchedStars >= 3);
        }
    }
}

void TestOriginIntegration::testErrorHandling()
{
    OriginTIFFReader reader;
    
    // Test with non-existent file
    QVERIFY(!reader.loadTIFFFile("nonexistent.tiff"));
    QVERIFY(!reader.getLastError().isEmpty());
    
    // Test with invalid TIFF file
    QVERIFY(!reader.loadTIFFFile("test_data/invalid.tiff"));
}

QTEST_MAIN(TestOriginIntegration)
#include "test_origin_integration.moc"
```

## 5. Performance Optimization

### Caching and Efficiency

Add caching to improve performance for repeated operations:

```cpp
// OriginTIFFCache.h - Optional caching layer
class OriginTIFFCache : public QObject
{
    Q_OBJECT
    
public:
    struct CacheEntry {
        QString filePath;
        QDateTime lastModified;
        OriginMetadata metadata;
        CatalogLookupResult catalogResult;
        QDateTime cacheTime;
    };
    
    bool hasCachedMetadata(const QString& filePath) const;
    OriginMetadata getCachedMetadata(const QString& filePath) const;
    void cacheMetadata(const QString& filePath, const OriginMetadata& metadata);
    
    bool hasCachedCatalogResults(const QString& filePath) const;
    CatalogLookupResult getCachedCatalogResults(const QString& filePath) const;
    void cacheCatalogResults(const QString& filePath, const CatalogLookupResult& results);
    
    void clearCache();
    void setCacheTimeout(int minutes) { m_cacheTimeoutMinutes = minutes; }

private:
    QHash<QString, CacheEntry> m_cache;
    int m_cacheTimeoutMinutes = 60; // 1 hour default
    
    bool isCacheValid(const CacheEntry& entry) const;
};
```

## 6. Advanced Features

### Batch Processing

Add support for processing multiple Origin TIFF files:

```cpp
// OriginBatchProcessor.h
class OriginBatchProcessor : public QObject
{
    Q_OBJECT
    
public:
    struct BatchJob {
        QString filePath;
        OriginMetadata metadata;
        CatalogLookupResult catalogResult;
        bool completed = false;
        bool successful = false;
        QString errorMessage;
    };
    
    void addFile(const QString& filePath);
    void addFiles(const QStringList& filePaths);
    void startBatchProcessing();
    void stopBatchProcessing();
    
    QVector<BatchJob> getResults() const { return m_jobs; }
    bool isProcessing() const { return m_isProcessing; }

signals:
    void fileCompleted(int index, const BatchJob& job);
    void batchCompleted(const QVector<BatchJob>& results);
    void progressChanged(int completed, int total);

private slots:
    void processNextFile();
    void onFileProcessed();

private:
    QVector<BatchJob> m_jobs;
    int m_currentJobIndex = 0;
    bool m_isProcessing = false;
    OriginTIFFReader* m_currentReader = nullptr;
    StarCatalogValidator* m_catalogValidator = nullptr;
};
```

### GUI Enhancements

Add a dedicated Origin TIFF information panel:

```cpp
// OriginInfoWidget.h
class OriginInfoWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit OriginInfoWidget(QWidget* parent = nullptr);
    
    void setOriginMetadata(const OriginMetadata& metadata);
    void setCatalogResults(const CatalogLookupResult& results);
    void clear();

private:
    void setupUI();
    void updateMetadataDisplay();
    void updateCatalogDisplay();
    
    // UI elements
    QLabel* m_objectNameLabel;
    QLabel* m_coordinatesLabel;
    QLabel* m_fieldOfViewLabel;
    QLabel* m_pixelScaleLabel;
    QLabel* m_exposureInfoLabel;
    QLabel* m_stackInfoLabel;
    QLabel* m_catalogStatsLabel;
    QLabel* m_plateSolveStatusLabel;
    QProgressBar* m_plateSolveQuality;
    QPushButton* m_exportButton;
    QPushButton* m_showCatalogButton;
    
    // Data
    OriginMetadata m_metadata;
    CatalogLookupResult m_catalogResults;
};
```

## 7. Building and Deployment

### Updated CMakeLists.txt with all features

```cmake
cmake_minimum_required(VERSION 3.16)
project(StarMask VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Qt5 REQUIRED COMPONENTS Core Widgets Network)
find_package(TIFF REQUIRED)
find_package(PkgConfig REQUIRED)

# Optional: Find PCL for advanced features
find_package(PCL QUIET)
if(PCL_FOUND)
    add_definitions(-DHAS_PCL)
endif()

# Source files
set(ORIGIN_TIFF_SOURCES
    src/OriginTIFFReader.h
    src/OriginTIFFReader.cpp
    src/OriginTIFFCache.h
    src/OriginTIFFCache.cpp
    src/OriginBatchProcessor.h
    src/OriginBatchProcessor.cpp
    src/OriginInfoWidget.h
    src/OriginInfoWidget.cpp
)

set(EXISTING_SOURCES
    # Your existing source files...
    src/main.cpp
    src/MainWindow.cpp
    src/ImageReader.cpp
    src/StarCatalogValidator.cpp
    # ... etc
)

# Create executable
add_executable(${PROJECT_NAME}
    ${EXISTING_SOURCES}
    ${ORIGIN_TIFF_SOURCES}
)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    Qt5::Core
    Qt5::Widgets
    Qt5::Network
    ${TIFF_LIBRARIES}
)

if(PCL_FOUND)
    target_link_libraries(${PROJECT_NAME} ${PCL_LIBRARIES})
    target_include_directories(${PROJECT_NAME} PRIVATE ${PCL_INCLUDE_DIRS})
endif()

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${TIFF_INCLUDE_DIRS}
)

# Compiler definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE
    QT_NO_DEBUG_OUTPUT  # Remove in debug builds
)

# Installation
install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

# Testing (optional)
enable_testing()
add_subdirectory(tests)
```

### Build Instructions

```bash
# Clone your repository
git clone <your-repo-url>
cd starmask

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
make -j$(nproc)

# Run tests
ctest

# Install
make install
```

## 8. Usage Examples

### Command Line Usage

If you want to add command line support:

```cpp
// main.cpp additions
#include <QCommandLineParser>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("StarMask");
    app.setApplicationVersion("1.0.0");
    
    QCommandLineParser parser;
    parser.setApplicationDescription("Astronomical image processing with Origin telescope support");
    parser.addHelpOption();
    parser.addVersionOption();
    
    QCommandLineOption originFileOption(
        QStringList() << "o" << "origin",
        "Process Origin TIFF file with automatic catalog lookup",
        "file");
    parser.addOption(originFileOption);
    
    QCommandLineOption batchOption(
        QStringList() << "b" << "batch",
        "Batch process multiple Origin TIFF files",
        "directory");
    parser.addOption(batchOption);
    
    QCommandLineOption outputOption(
        QStringList() << "output",
        "Output directory for results",
        "directory");
    parser.addOption(outputOption);
    
    parser.process(app);
    
    if (parser.isSet(originFileOption)) {
        // Command line processing mode
        QString filePath = parser.value(originFileOption);
        return processOriginFileCommandLine(filePath, parser.value(outputOption));
    } else if (parser.isSet(batchOption)) {
        // Batch processing mode
        QString directory = parser.value(batchOption);
        return processBatchCommandLine(directory, parser.value(outputOption));
    } else {
        // GUI mode
        MainWindow window;
        window.show();
        return app.exec();
    }
}

int processOriginFileCommandLine(const QString& filePath, const QString& outputDir)
{
    OriginTIFFReader reader;
    StarCatalogValidator validator;
    
    if (!reader.loadTIFFFile(filePath)) {
        qCritical() << "Failed to load Origin TIFF file:" << reader.getLastError();
        return 1;
    }
    
    if (!reader.hasOriginMetadata()) {
        qCritical() << "No Origin metadata found in TIFF file";
        return 1;
    }
    
    // Perform catalog lookup
    reader.performCatalogLookup(&validator);
    
    // Wait for completion (simplified for command line)
    QEventLoop loop;
    QObject::connect(&reader, &OriginTIFFReader::catalogLookupCompleted, &loop, &QEventLoop::quit);
    loop.exec();
    
    // Export results
    QString outputPath = outputDir.isEmpty() ? "." : outputDir;
    QFileInfo fileInfo(filePath);
    QString resultPath = QString("%1/%2_results.json").arg(outputPath).arg(fileInfo.baseName());
    
    QFile file(resultPath);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(reader.exportMetadataAsJson());
        file.write(doc.toJson());
        qInfo() << "Results saved to:" << resultPath;
        return 0;
    } else {
        qCritical() << "Failed to save results to:" << resultPath;
        return 1;
    }
}
```

## 9. Troubleshooting

### Common Issues and Solutions

1. **TIFF Library Not Found**
   ```bash
   # Ubuntu/Debian
   sudo apt-get install libtiff-dev
   
   # macOS with Homebrew
   brew install libtiff
   
   # Then reconfigure CMake
   cmake .. -DTIFF_ROOT=/usr/local
   ```

2. **Qt Network Module Missing**
   ```bash
   # Ubuntu/Debian
   sudo apt-get install qtbase5-dev qttools5-dev
   
   # macOS
   brew install qt5
   ```

3. **Origin JSON Not Found**
   - Verify the TIFF file is from Origin telescope software
   - Check that the TIFF file contains EXIF data
   - Enable verbose logging: `reader.setVerboseLogging(true)`

4. **Catalog Lookup Fails**
   - Check internet connectivity
   - Verify coordinates are reasonable (RA: 0-360°, Dec: -90°+90°)
   - Try increasing search radius multiplier

5. **Plate Solving Issues**
   - Ensure at least 3 bright stars are detected
   - Check that catalog contains stars in the magnitude range
   - Verify pixel scale is reasonable (typically 0.5-10 arcsec/pixel)

### Debug Logging

Enable comprehensive logging for troubleshooting:

```cpp
// In your application initialization
QLoggingCategory::setFilterRules("*.debug=true");

// Or for specific components
QLoggingCategory::setFilterRules("origin.tiff.debug=true");
```

This comprehensive integration guide provides everything needed to add Origin telescope TIFF support with automated RA/DEC catalog lookups to your existing Qt/CMake application. The solution handles the JSON metadata extraction from TIFF files, performs catalog queries based on the object name and coordinates, and provides plate solving capabilities for accurate astrometric calibration.