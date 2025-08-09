// PracticalOriginIntegration.cpp - Real-world integration example
// This shows how to integrate the Origin TIFF reader into your existing application

#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QTextEdit>
#include <QProgressBar>
#include <QGroupBox>
#include <QGridLayout>
#include <QMessageBox>
#include <QTimer>
#include <QDebug>

// Your existing includes
#include "OriginTIFFReader.h"
#include "StarCatalogValidator.h" // Your existing catalog system

class OriginTIFFIntegrationDemo : public QMainWindow
{
    Q_OBJECT

public:
    OriginTIFFIntegrationDemo(QWidget* parent = nullptr);

private slots:
    void onLoadOriginTIFF();
    void onOriginMetadataExtracted(const OriginMetadata& metadata);
    void onCatalogLookupCompleted(const CatalogLookupResult& results);
    void onExportResults();
    void onClearResults();

private:
    void setupUI();
    void updateMetadataDisplay(const OriginMetadata& metadata);
    void updateCatalogDisplay(const CatalogLookupResult& results);
    void updateStatusDisplay(const QString& status, bool isError = false);

    // UI Components
    QPushButton* m_loadButton;
    QPushButton* m_exportButton;
    QPushButton* m_clearButton;
    QLabel* m_statusLabel;
    QProgressBar* m_progressBar;
    
    // Metadata display group
    QGroupBox* m_metadataGroup;
    QLabel* m_objectNameLabel;
    QLabel* m_coordinatesLabel;
    QLabel* m_fieldOfViewLabel;
    QLabel* m_imageSizeLabel;
    QLabel* m_pixelScaleLabel;
    QLabel* m_exposureInfoLabel;
    QLabel* m_stackInfoLabel;
    QLabel* m_dateTimeLabel;
    QLabel* m_gpsLocationLabel;

    // Catalog results group
    QGroupBox* m_catalogGroup;
    QLabel* m_catalogStatusLabel;
    QLabel* m_starsFoundLabel;
    QLabel* m_searchRadiusLabel;
    QLabel* m_plateSolveStatusLabel;
    QLabel* m_rmsErrorLabel;
    QLabel* m_matchedStarsLabel;

    // Log display
    QTextEdit* m_logDisplay;

    // Core components
    OriginTIFFReader* m_originReader;
    StarCatalogValidator* m_catalogValidator;
    
    // Data
    OriginMetadata m_currentMetadata;
    CatalogLookupResult m_currentResults;
    bool m_hasValidData;
};

OriginTIFFIntegrationDemo::OriginTIFFIntegrationDemo(QWidget* parent)
    : QMainWindow(parent)
    , m_originReader(nullptr)
    , m_catalogValidator(nullptr)
    , m_hasValidData(false)
{
    setupUI();
    
    // Initialize catalog validator
    m_catalogValidator = new StarCatalogValidator(this);
    
    setWindowTitle("Origin TIFF Integration Demo");
    resize(800, 600);
    
    updateStatusDisplay("Ready to load Origin TIFF file");
}

void OriginTIFFIntegrationDemo::setupUI()
{
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    
    // Control buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_loadButton = new QPushButton("Load Origin TIFF");
    m_exportButton = new QPushButton("Export Results");
    m_clearButton = new QPushButton("Clear");
    
    m_exportButton->setEnabled(false);
    
    buttonLayout->addWidget(m_loadButton);
    buttonLayout->addWidget(m_exportButton);
    buttonLayout->addWidget(m_clearButton);
    buttonLayout->addStretch();
    
    connect(m_loadButton, &QPushButton::clicked, this, &OriginTIFFIntegrationDemo::onLoadOriginTIFF);
    connect(m_exportButton, &QPushButton::clicked, this, &OriginTIFFIntegrationDemo::onExportResults);
    connect(m_clearButton, &QPushButton::clicked, this, &OriginTIFFIntegrationDemo::onClearResults);
    
    mainLayout->addLayout(buttonLayout);
    
    // Status and progress
    m_statusLabel = new QLabel("Ready");
    m_progressBar = new QProgressBar();
    m_progressBar->setVisible(false);
    
    mainLayout->addWidget(m_statusLabel);
    mainLayout->addWidget(m_progressBar);
    
    // Create tabbed content area
    QHBoxLayout* contentLayout = new QHBoxLayout();
    
    // Left side - Metadata and catalog info
    QVBoxLayout* leftLayout = new QVBoxLayout();
    
    // Metadata group
    m_metadataGroup = new QGroupBox("Origin Telescope Metadata");
    QGridLayout* metadataLayout = new QGridLayout(m_metadataGroup);
    
    metadataLayout->addWidget(new QLabel("Object:"), 0, 0);
    m_objectNameLabel = new QLabel("N/A");
    metadataLayout->addWidget(m_objectNameLabel, 0, 1);
    
    metadataLayout->addWidget(new QLabel("Coordinates:"), 1, 0);
    m_coordinatesLabel = new QLabel("N/A");
    metadataLayout->addWidget(m_coordinatesLabel, 1, 1);
    
    metadataLayout->addWidget(new QLabel("Field of View:"), 2, 0);
    m_fieldOfViewLabel = new QLabel("N/A");
    metadataLayout->addWidget(m_fieldOfViewLabel, 2, 1);
    
    metadataLayout->addWidget(new QLabel("Image Size:"), 3, 0);
    m_imageSizeLabel = new QLabel("N/A");
    metadataLayout->addWidget(m_imageSizeLabel, 3, 1);
    
    metadataLayout->addWidget(new QLabel("Pixel Scale:"), 4, 0);
    m_pixelScaleLabel = new QLabel("N/A");
    metadataLayout->addWidget(m_pixelScaleLabel, 4, 1);
    
    metadataLayout->addWidget(new QLabel("Exposure:"), 5, 0);
    m_exposureInfoLabel = new QLabel("N/A");
    metadataLayout->addWidget(m_exposureInfoLabel, 5, 1);
    
    metadataLayout->addWidget(new QLabel("Stack Info:"), 6, 0);
    m_stackInfoLabel = new QLabel("N/A");
    metadataLayout->addWidget(m_stackInfoLabel, 6, 1);
    
    metadataLayout->addWidget(new QLabel("Date/Time:"), 7, 0);
    m_dateTimeLabel = new QLabel("N/A");
    metadataLayout->addWidget(m_dateTimeLabel, 7, 1);
    
    metadataLayout->addWidget(new QLabel("GPS Location:"), 8, 0);
    m_gpsLocationLabel = new QLabel("N/A");
    metadataLayout->addWidget(m_gpsLocationLabel, 8, 1);
    
    leftLayout->addWidget(m_metadataGroup);
    
    // Catalog results group
    m_catalogGroup = new QGroupBox("Catalog Lookup Results");
    QGridLayout* catalogLayout = new QGridLayout(m_catalogGroup);
    
    catalogLayout->addWidget(new QLabel("Status:"), 0, 0);
    m_catalogStatusLabel = new QLabel("Not attempted");
    catalogLayout->addWidget(m_catalogStatusLabel, 0, 1);
    
    catalogLayout->addWidget(new QLabel("Stars Found:"), 1, 0);
    m_starsFoundLabel = new QLabel("N/A");
    catalogLayout->addWidget(m_starsFoundLabel, 1, 1);
    
    catalogLayout->addWidget(new QLabel("Search Radius:"), 2, 0);
    m_searchRadiusLabel = new QLabel("N/A");
    catalogLayout->addWidget(m_searchRadiusLabel, 2, 1);
    
    catalogLayout->addWidget(new QLabel("Plate Solve:"), 3, 0);
    m_plateSolveStatusLabel = new QLabel("N/A");
    catalogLayout->addWidget(m_plateSolveStatusLabel, 3, 1);
    
    catalogLayout->addWidget(new QLabel("RMS Error:"), 4, 0);
    m_rmsErrorLabel = new QLabel("N/A");
    catalogLayout->addWidget(m_rmsErrorLabel, 4, 1);
    
    catalogLayout->addWidget(new QLabel("Matched Stars:"), 5, 0);
    m_matchedStarsLabel = new QLabel("N/A");
    catalogLayout->addWidget(m_matchedStarsLabel, 5, 1);
    
    leftLayout->addWidget(m_catalogGroup);
    leftLayout->addStretch();
    
    contentLayout->addLayout(leftLayout);
    
    // Right side - Log display
    m_logDisplay = new QTextEdit();
    m_logDisplay->setReadOnly(true);
    m_logDisplay->setMaximumWidth(400);
    m_logDisplay->append("Application started. Ready to process Origin TIFF files.");
    
    contentLayout->addWidget(m_logDisplay, 1);
    
    mainLayout->addLayout(contentLayout);
}

void OriginTIFFIntegrationDemo::onLoadOriginTIFF()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Select Origin TIFF File",
        QString(),
        "TIFF Files (*.tiff *.tif);;All Files (*)"
    );
    
    if (fileName.isEmpty()) {
        return;
    }
    
    m_logDisplay->append(QString("\n=== Loading Origin TIFF File ==="));
    m_logDisplay->append(QString("File: %1").arg(fileName));
    
    // Clean up previous reader
    if (m_originReader) {
        delete m_originReader;
    }
    
    // Create new reader
    m_originReader = new OriginTIFFReader(this);
    
    // Connect signals
    connect(m_originReader, &OriginTIFFReader::metadataExtracted,
            this, &OriginTIFFIntegrationDemo::onOriginMetadataExtracted);
    
    connect(m_originReader, &OriginTIFFReader::catalogLookupCompleted,
            this, &OriginTIFFIntegrationDemo::onCatalogLookupCompleted);
    
    connect(m_originReader, &OriginTIFFReader::errorOccurred,
            this, [this](const QString& error) {
        m_logDisplay->append(QString("ERROR: %1").arg(error));
        updateStatusDisplay(QString("Error: %1").arg(error), true);
        m_progressBar->setVisible(false);
    });
    
    // Configure reader
    m_originReader->setMaxCatalogStars(1000);
    m_originReader->setSearchRadiusMultiplier(1.5);
    m_originReader->setEnablePlateSolving(true);
    m_originReader->setVerboseLogging(true);
    
    // Show progress
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 0); // Indeterminate
    updateStatusDisplay("Loading TIFF file and extracting metadata...");
    
    // Load the file
    if (!m_originReader->loadTIFFFile(fileName)) {
        m_logDisplay->append(QString("FAILED to load TIFF file: %1").arg(m_originReader->getLastError()));
        updateStatusDisplay("Failed to load TIFF file", true);
        m_progressBar->setVisible(false);
        return;
    }
    
    m_logDisplay->append("TIFF file loaded successfully");
    
    // Check if Origin metadata was found
    if (!m_originReader->hasOriginMetadata()) {
        m_logDisplay->append("No Origin telescope metadata found in this TIFF file");
        updateStatusDisplay("No Origin metadata found", true);
        m_progressBar->setVisible(false);
        
        QMessageBox::information(this, "No Origin Data", 
            "This TIFF file does not contain Origin telescope metadata.\n"
            "Please select a TIFF file exported from Origin telescope software.");
        return;
    }
    
    // Metadata will be processed via signal
    updateStatusDisplay("Origin metadata found, starting catalog lookup...");
}

void OriginTIFFIntegrationDemo::onOriginMetadataExtracted(const OriginMetadata& metadata)
{
    m_currentMetadata = metadata;
    m_logDisplay->append("\n=== Origin Metadata Extracted ===");
    m_logDisplay->append(QString("Object: %1").arg(metadata.objectName));
    m_logDisplay->append(QString("RA: %1° (%2h %3m %4s)")
        .arg(metadata.centerRA, 0, 'f', 6)
        .arg(static_cast<int>(metadata.centerRA / 15.0))
        .arg(static_cast<int>(fmod(metadata.centerRA / 15.0, 1.0) * 60.0))
        .arg(fmod(fmod(metadata.centerRA / 15.0, 1.0) * 60.0, 1.0) * 60.0, 0, 'f', 1));
    
    m_logDisplay->append(QString("Dec: %1°").arg(metadata.centerDec, 0, 'f', 6));
    m_logDisplay->append(QString("Field of View: %1° × %2°").arg(metadata.fieldOfViewX).arg(metadata.fieldOfViewY));
    m_logDisplay->append(QString("Image: %1 × %2 pixels").arg(metadata.imageWidth).arg(metadata.imageHeight));
    m_logDisplay->append(QString("Pixel Scale: %1 arcsec/pixel").arg(metadata.getAveragePixelScale(), 0, 'f', 2));
    m_logDisplay->append(QString("Stacked: %1 frames, %2 total seconds")
        .arg(metadata.stackedDepth).arg(metadata.totalDurationMs / 1000.0, 0, 'f', 1));
    
    // Update UI
    updateMetadataDisplay(metadata);
    
    // Start catalog lookup
    updateStatusDisplay("Querying star catalog...");
    m_logDisplay->append("\n=== Starting Catalog Lookup ===");
    m_logDisplay->append(QString("Search center: RA=%1°, Dec=%2°").arg(metadata.centerRA).arg(metadata.centerDec));
    
    double searchRadius = qMax(metadata.fieldOfViewX, metadata.fieldOfViewY) * 1.5;
    m_logDisplay->append(QString("Search radius: %1°").arg(searchRadius, 0, 'f', 4));
    
    // Perform catalog lookup
    if (!m_originReader->performCatalogLookup(m_catalogValidator)) {
        m_logDisplay->append("FAILED to start catalog lookup");
        updateStatusDisplay("Catalog lookup failed", true);
        m_progressBar->setVisible(false);
    }
}

void OriginTIFFIntegrationDemo::onCatalogLookupCompleted(const CatalogLookupResult& results)
{
    m_currentResults = results;
    m_progressBar->setVisible(false);
    
    m_logDisplay->append("\n=== Catalog Lookup Completed ===");
    
    if (results.success) {
        m_logDisplay->append(QString("SUCCESS: Found %1 catalog stars").arg(results.totalStarsFound));
        m_logDisplay->append(QString("Catalog: %1").arg(results.catalogUsed));
        m_logDisplay->append(QString("Search radius: %1°").arg(results.searchRadiusDegrees, 0, 'f', 4));
        
        // Plate solving results
        if (results.plateSolveAttempted) {
            m_logDisplay->append("\n=== Plate Solving Results ===");
            if (results.plateSolveSuccessful) {
                m_logDisplay->append(QString("SUCCESS: Plate solve completed"));
                m_logDisplay->append(QString("RMS Error: %1 arcsec").arg(results.solvingErrorArcsec, 0, 'f', 2));
                m_logDisplay->append(QString("Matched Stars: %1").arg(results.matchedStars));
                m_logDisplay->append(QString("Solved Position: RA=%1°, Dec=%2°")
                    .arg(results.solvedRA, 0, 'f', 6).arg(results.solvedDec, 0, 'f', 6));
                m_logDisplay->append(QString("Solved Pixel Scale: %1 arcsec/pixel")
                    .arg(results.solvedPixelScale, 0, 'f', 2));
                m_logDisplay->append(QString("Solved Orientation: %1°")
                    .arg(results.solvedOrientation, 0, 'f', 2));
                
                updateStatusDisplay("Plate solving successful!");
            } else {
                m_logDisplay->append(QString("FAILED: Plate solve unsuccessful"));
                m_logDisplay->append(QString("RMS Error: %1 arcsec").arg(results.solvingErrorArcsec, 0, 'f', 2));
                m_logDisplay->append(QString("Matched Stars: %1").arg(results.matchedStars));
                updateStatusDisplay("Catalog lookup successful, plate solving failed");
            }
        } else {
            updateStatusDisplay("Catalog lookup successful");
        }
        
        m_hasValidData = true;
        m_exportButton->setEnabled(true);
        
    } else {
        m_logDisplay->append(QString("FAILED: %1").arg(results.errorMessage));
        updateStatusDisplay(QString("Catalog lookup failed: %1").arg(results.errorMessage), true);
        m_hasValidData = false;
    }
    
    // Update UI
    updateCatalogDisplay(results);
    
    m_logDisplay->append("\n=== Processing Complete ===");
    m_logDisplay->ensureCursorVisible();
}

void OriginTIFFIntegrationDemo::updateMetadataDisplay(const OriginMetadata& metadata)
{
    m_objectNameLabel->setText(metadata.objectName);
    
    // Format coordinates nicely
    double raHours = metadata.centerRA / 15.0;
    int hours = static_cast<int>(raHours);
    int minutes = static_cast<int>((raHours - hours) * 60.0);
    double seconds = ((raHours - hours) * 60.0 - minutes) * 60.0;
    
    int decDegrees = static_cast<int>(metadata.centerDec);
    int decMinutes = static_cast<int>(qAbs(metadata.centerDec - decDegrees) * 60.0);
    double decSeconds = (qAbs(metadata.centerDec - decDegrees) * 60.0 - decMinutes) * 60.0;
    
    QString coordText = QString("RA: %1h %2m %3s\nDec: %4° %5' %6\"")
        .arg(hours, 2, 10, QChar('0'))
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 0, 'f', 1)
        .arg(decDegrees, 3, 10, metadata.centerDec >= 0 ? QChar(' ') : QChar('-'))
        .arg(decMinutes, 2, 10, QChar('0'))
        .arg(decSeconds, 0, 'f', 1);
    
    m_coordinatesLabel->setText(coordText);
    
    // Format field of view
    QString fovText = QString("%1° × %2°")
        .arg(metadata.fieldOfViewX, 0, 'f', 4)
        .arg(metadata.fieldOfViewY, 0, 'f', 4);
    m_fieldOfViewLabel->setText(fovText);
    
    // Image size and pixel scale
    m_imageSizeLabel->setText(QString("%1 × %2 pixels")
        .arg(metadata.imageWidth).arg(metadata.imageHeight));
    
    m_pixelScaleLabel->setText(QString("%1 arcsec/pixel")
        .arg(metadata.getAveragePixelScale(), 0, 'f', 2));
    
    // Exposure info
    m_exposureInfoLabel->setText(metadata.getReadableExposureInfo());
    
    // Stack info
    m_stackInfoLabel->setText(QString("%1 frames (%2 min total)")
        .arg(metadata.stackedDepth)
        .arg(metadata.totalDurationMs / 60000.0, 0, 'f', 1));
    
    // Date/time
    m_dateTimeLabel->setText(metadata.dateTime);
    
    // GPS location
    if (metadata.gpsLatitude != 0.0 || metadata.gpsLongitude != 0.0) {
        m_gpsLocationLabel->setText(QString("%1°, %2° (alt: %3m)")
            .arg(metadata.gpsLatitude, 0, 'f', 6)
            .arg(metadata.gpsLongitude, 0, 'f', 6)
            .arg(metadata.gpsAltitude, 0, 'f', 1));
    } else {
        m_gpsLocationLabel->setText("Not available");
    }
}

void OriginTIFFIntegrationDemo::updateCatalogDisplay(const CatalogLookupResult& results)
{
    if (results.success) {
        m_catalogStatusLabel->setText("✓ Success");
        m_catalogStatusLabel->setStyleSheet("color: green; font-weight: bold;");
        
        m_starsFoundLabel->setText(QString("%1 stars").arg(results.totalStarsFound));
        m_searchRadiusLabel->setText(QString("%1°").arg(results.searchRadiusDegrees, 0, 'f', 4));
        
        if (results.plateSolveAttempted) {
            if (results.plateSolveSuccessful) {
                m_plateSolveStatusLabel->setText("✓ Successful");
                m_plateSolveStatusLabel->setStyleSheet("color: green; font-weight: bold;");
                m_rmsErrorLabel->setText(QString("%1 arcsec").arg(results.solvingErrorArcsec, 0, 'f', 2));
                m_matchedStarsLabel->setText(QString("%1 matches").arg(results.matchedStars));
            } else {
                m_plateSolveStatusLabel->setText("✗ Failed");
                m_plateSolveStatusLabel->setStyleSheet("color: orange; font-weight: bold;");
                m_rmsErrorLabel->setText(QString("%1 arcsec (high)").arg(results.solvingErrorArcsec, 0, 'f', 2));
                m_matchedStarsLabel->setText(QString("%1 matches (insufficient)").arg(results.matchedStars));
            }
        } else {
            m_plateSolveStatusLabel->setText("Not attempted");
            m_plateSolveStatusLabel->setStyleSheet("color: gray;");
            m_rmsErrorLabel->setText("N/A");
            m_matchedStarsLabel->setText("N/A");
        }
        
    } else {
        m_catalogStatusLabel->setText("✗ Failed");
        m_catalogStatusLabel->setStyleSheet("color: red; font-weight: bold;");
        m_starsFoundLabel->setText("0 stars");
        m_searchRadiusLabel->setText("N/A");
        m_plateSolveStatusLabel->setText("N/A");
        m_rmsErrorLabel->setText("N/A");
        m_matchedStarsLabel->setText("N/A");
    }
}

void OriginTIFFIntegrationDemo::updateStatusDisplay(const QString& status, bool isError)
{
    m_statusLabel->setText(status);
    if (isError) {
        m_statusLabel->setStyleSheet("color: red; font-weight: bold;");
    } else {
        m_statusLabel->setStyleSheet("color: black;");
    }
}

void OriginTIFFIntegrationDemo::onExportResults()
{
    if (!m_hasValidData) {
        QMessageBox::information(this, "Export Results", 
            "No valid data available to export. Please load an Origin TIFF file first.");
        return;
    }
    
    QString fileName = QFileDialog::getSaveFileName(
        this,
        "Export Origin TIFF Results",
        QString("origin_results_%1.json").arg(m_currentMetadata.objectName.replace(" ", "_")),
        "JSON Files (*.json);;Text Files (*.txt);;All Files (*)"
    );
    
    if (fileName.isEmpty()) {
        return;
    }
    
    try {
        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::critical(this, "Export Error", 
                "Could not open file for writing.");
            return;
        }
        
        QTextStream out(&file);
        QFileInfo fileInfo(fileName);
        
        if (fileInfo.suffix().toLower() == "json") {
            // Export as JSON
            QJsonDocument doc(m_originReader->exportMetadataAsJson());
            out << doc.toJson();
        } else {
            // Export as text
            out << m_originReader->exportMetadataAsText();
        }
        
        m_logDisplay->append(QString("\nResults exported to: %1").arg(fileName));
        updateStatusDisplay(QString("Results exported to %1").arg(fileInfo.fileName()));
        
        QMessageBox::information(this, "Export Complete", 
            QString("Results successfully exported to:\n%1").arg(fileName));
        
    } catch (const std::exception& e) {
        QString error = QString("Error exporting results: %1").arg(e.what());
        m_logDisplay->append(QString("ERROR: %1").arg(error));
        QMessageBox::critical(this, "Export Error", error);
    }
}

void OriginTIFFIntegrationDemo::onClearResults()
{
    // Clear data
    m_currentMetadata = OriginMetadata();
    m_currentResults = CatalogLookupResult();
    m_hasValidData = false;
    
    // Reset UI
    m_objectNameLabel->setText("N/A");
    m_coordinatesLabel->setText("N/A");
    m_fieldOfViewLabel->setText("N/A");
    m_imageSizeLabel->setText("N/A");
    m_pixelScaleLabel->setText("N/A");
    m_exposureInfoLabel->setText("N/A");
    m_stackInfoLabel->setText("N/A");
    m_dateTimeLabel->setText("N/A");
    m_gpsLocationLabel->setText("N/A");
    
    m_catalogStatusLabel->setText("Not attempted");
    m_catalogStatusLabel->setStyleSheet("");
    m_starsFoundLabel->setText("N/A");
    m_searchRadiusLabel->setText("N/A");
    m_plateSolveStatusLabel->setText("N/A");
    m_plateSolveStatusLabel->setStyleSheet("");
    m_rmsErrorLabel->setText("N/A");
    m_matchedStarsLabel->setText("N/A");
    
    m_exportButton->setEnabled(false);
    m_progressBar->setVisible(false);
    
    // Clear log
    m_logDisplay->clear();
    m_logDisplay->append("Results cleared. Ready to load new Origin TIFF file.");
    
    updateStatusDisplay("Ready to load Origin TIFF file");
    
    // Clean up reader
    if (m_originReader) {
        delete m_originReader;
        m_originReader = nullptr;
    }
}

// Example of how to integrate with your existing CMakeLists.txt
/*
# Add this to your existing CMakeLists.txt:

# Find required Qt components
find_package(Qt5 REQUIRED COMPONENTS Core Widgets Network)

# Add the Origin TIFF integration sources
set(ORIGIN_INTEGRATION_SOURCES
    src/OriginTIFFReader.h
    src/OriginTIFFReader.cpp
    src/PracticalOriginIntegration.cpp  # This demo file
)

# Add to your existing executable
target_sources(your_existing_target PRIVATE
    ${ORIGIN_INTEGRATION_SOURCES}
    # ... your existing sources
)

# Link libraries
target_link_libraries(your_existing_target
    Qt5::Core
    Qt5::Widgets  
    Qt5::Network
    ${TIFF_LIBRARIES}
    # ... your existing libraries
)
*/

// Main function for standalone demo
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    app.setApplicationName("Origin TIFF Integration Demo");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("Your Organization");
    
    OriginTIFFIntegrationDemo window;
    window.show();
    
    return app.exec();
}

#include "PracticalOriginIntegration.moc"

/*
=== USAGE INSTRUCTIONS ===

1. Compile and run this demo application
2. Click "Load Origin TIFF" and select your FinalStackedMaster.tiff file
3. The application will:
   - Extract the Origin JSON metadata from the TIFF file
   - Display the object name, coordinates, field of view, etc.
   - Automatically query a star catalog based on the coordinates
   - Attempt plate solving to verify the astrometric solution
   - Display the results in real-time

4. Use "Export Results" to save the complete metadata and catalog results

=== INTEGRATION WITH YOUR EXISTING APP ===

To integrate this into your existing application:

1. Add the OriginTIFFReader class to your project
2. Modify your existing ImageReader to detect Origin TIFF files
3. Add UI elements to display the metadata
4. Connect to your existing StarCatalogValidator
5. Use the extracted coordinates for automated guiding plate solving

The extracted JSON contains:
- objectName: "Whirlpool Galaxy - M 51"
- celestial.first: RA in degrees (202.469 for M51)
- celestial.second: Dec in degrees (47.195 for M51)
- fovX/fovY: Field of view in degrees
- imageWidth/Height: Pixel dimensions
- orientation: Position angle
- gps: Location coordinates
- stackedDepth: Number of stacked frames
- totalDurationMs: Total exposure time

This provides everything needed for automated catalog lookup and plate solving!
*/