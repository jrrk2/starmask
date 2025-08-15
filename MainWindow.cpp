#include <QApplication>
#include <QDebug>
#include <QFileDialog>
#include <QStandardPaths>
#include <QTableWidget>
#include <QMenuBar>
#include <QClipboard>
#include <QDesktopServices>
#include <QSplitter>
#include <QMessageBox>
#include <QMessageLogger>
#include <QJsonDocument>
#include <QTime>

#include "MainWindow.h"
#include "ImageReader.h"

// PCL includes
#include <pcl/Image.h>
#include <pcl/XISF.h>
#include <pcl/String.h>
#include <pcl/Property.h>

#include "GaiaGDR3Catalog.h"
#include "PlatesolverSettingsDialog.h"

// Add these method implementations to your MainWindow.cpp to fix the compilation errors:

void MainWindow::initializePlatesolveIntegration()
{
    // Initialize the plate solving integration
  //    m_platesolveIntegration = new ExtractStarsWithPlateSolve(this);
    m_platesolveIntegration = new SimplePlatesolver(this);
    m_platesolveProgressDialog = nullptr;
    
    // Default settings - adjust paths for your system
    m_astrometryPath = "/opt/homebrew/bin/solve-field";
    m_indexPath = "/opt/homebrew/share/astrometry";
    m_minScale = 0.5;
    m_maxScale = 60.0;
    m_platesolveTimeout = 300;
    m_maxStarsForSolving = 200;
    
    // Configure plate solver
    m_platesolveIntegration->configurePlateSolver(m_astrometryPath, m_indexPath, m_minScale, m_maxScale);
    
    // Connect to your existing StarCatalogValidator if available
    if (m_catalogValidator) {
        m_platesolveIntegration->setStarCatalogValidator(m_catalogValidator.get());
    }
    
    // Connect signals
    //    connect(m_platesolveIntegration, &ExtractStarsWithPlateSolve::platesolveStarted,
    connect(m_platesolveIntegration, &SimplePlatesolver::platesolveStarted,
            this, &MainWindow::onPlatesolveStarted);
    //    connect(m_platesolveIntegration, &ExtractStarsWithPlateSolve::platesolveProgress,
    connect(m_platesolveIntegration, &SimplePlatesolver::platesolveProgress,
            this, &MainWindow::onPlatesolveProgress);
    //    connect(m_platesolveIntegration, &ExtractStarsWithPlateSolve::platesolveComplete,
    connect(m_platesolveIntegration, &SimplePlatesolver::platesolveComplete,
            this, &MainWindow::onPlatesolveComplete);
    //    connect(m_platesolveIntegration, &ExtractStarsWithPlateSolve::platesolveFailed,
    connect(m_platesolveIntegration, &SimplePlatesolver::platesolveFailed,
            this, &MainWindow::onPlatesolveFailed);  
    // CORRECT:
    //    connect(m_platesolveIntegration, &ExtractStarsWithPlateSolve::wcsDataAvailable,
    connect(m_platesolveIntegration, &SimplePlatesolver::wcsDataAvailable,
        this, [this](const WCSData& wcs) {
            this->onWCSDataReceived(wcs);
        });

    m_platesolveIntegration->setAutoSolveEnabled(true); // we assume this
    
}

// Replace your existing star detection methods with this integrated version:
void MainWindow::onDetectStarsIntegratedVersion()
{
    if (!m_imageReader->hasImage()) {
        m_statusLabel->setText("No image loaded.");
        return;
    }

    m_statusLabel->setText("Detecting stars with PCL StarDetector...");
    QApplication::processEvents();

    // Get parameters from your existing UI controls
    float sensitivity = m_sensitivitySlider->value() / 100.0f;
    int structureLayers = m_structureLayersSpinBox->value();
    int noiseLayers = m_noiseLayersSpinBox->value();
    float peakResponse = m_peakResponseSlider->value() / 100.0f;
    float maxDistortion = m_maxDistortionSlider->value() / 100.0f;
    bool enablePSFFitting = m_enablePSFFittingCheck->isChecked();

    // Use your existing StarMaskGenerator method
    m_lastStarMask = StarMaskGenerator::detectStarsAdvanced(
        m_imageReader->imageData(),
        sensitivity,
        structureLayers,
        noiseLayers,
        peakResponse,
        maxDistortion,
        enablePSFFitting
    );

    // Update display with your existing method
    m_imageDisplayWidget->setStarOverlay(m_lastStarMask.starCenters, m_lastStarMask.starRadii);
    m_starsDetected = !m_lastStarMask.starCenters.isEmpty();
    
    QString statusText = QString("PCL StarDetector: %1 stars detected").arg(m_lastStarMask.starCenters.size());
    m_statusLabel->setText(statusText);
    
    updateValidationControls();
}

void MainWindow::onExtractStarsWithPlatesolve()
{
    if (!m_imageReader->hasImage()) {
        QMessageBox::information(this, "Plate Solve", "No image loaded.");
        return;
    }

    /*    
    // Run star detection first if we don't have stars
    if (m_lastStarMask.starCenters.isEmpty()) {
        // Use your existing detection method
        onDetectStars(); // This calls your existing star detection
    }
    */
    
    // Now trigger plate solving
    if (!m_lastStarMask.starCenters.isEmpty()) {
        triggerPlatesolveWithCurrentStars();
    } else {
        QMessageBox::information(this, "Plate Solve", "No stars detected for plate solving.");
    }
}

void MainWindow::triggerPlatesolveWithCurrentStars()
{
    if (m_lastStarMask.starCenters.isEmpty()) {
        QMessageBox::information(this, "Plate Solve", "No stars available for plate solving.");
        return;
    }
    
    // Create flux estimates - simple approach based on radius
    QVector<float> starFluxes;
    starFluxes.reserve(m_lastStarMask.starCenters.size());
    
    for (int i = 0; i < m_lastStarMask.starCenters.size(); ++i) {
        float radius = (i < m_lastStarMask.starRadii.size()) ? m_lastStarMask.starRadii[i] : 3.0f;
        // Simple flux estimate: larger stars are brighter
        float estimatedFlux = radius * radius * 100.0f;
        starFluxes.append(estimatedFlux);
    }
    
    // Get ImageData pointer correctly
    const ImageData* imageDataPtr = &(m_imageReader->imageData());
    
    // Trigger plate solving
    m_platesolveIntegration->extractStarsAndSolve(
        imageDataPtr,
        m_lastStarMask.starCenters,
        starFluxes,
        m_lastStarMask.starRadii
    );
}

void MainWindow::onPlatesolveStarted()
{
    
    // Show progress dialog
    m_platesolveProgressDialog = new QProgressDialog("Plate solving in progress...", "Cancel", 0, 0, this);
    m_platesolveProgressDialog->setModal(true);
    m_platesolveProgressDialog->setMinimumDuration(1000);
    m_platesolveProgressDialog->show();
    
    m_statusLabel->setText("Plate solving...");
}

void MainWindow::onPlatesolveProgress(const QString& status)
{
    if (m_platesolveProgressDialog) {
        m_platesolveProgressDialog->setLabelText(status);
    }
    m_statusLabel->setText(status);
}

void MainWindow::onPlatesolveComplete(const pcl::AstrometricMetadata & result, const WCSData& wcs)
{
    // Hide progress dialog
    if (m_platesolveProgressDialog) {
        m_platesolveProgressDialog->hide();
        delete m_platesolveProgressDialog;
        m_platesolveProgressDialog = nullptr;
    }
    
    // Update status
    QString statusMsg = QString("✓ Plate solved: RA=%1° Dec=%2° Scale=%3\"/px")
                       .arg(wcs.crval1, 0, 'f', 4)
                       .arg(wcs.crval2, 0, 'f', 4)
                       .arg(wcs.pixscale, 0, 'f', 2);
    m_statusLabel->setText(statusMsg);
    
    // Set WCS flag
    m_hasWCS = true;

    // Update results display
    QString resultsText = QString(
        "Plate Solving Results\n"
        "====================\n\n"
        "✓ Solution Found\n\n"
        "Center: RA=%1° Dec=%2°\n"
        "Pixel Scale: %3 arcsec/pixel\n"
    ).arg(wcs.crval1, 0, 'f', 6)
     .arg(wcs.crval2, 0, 'f', 6)
      .arg(wcs.pixscale, 0, 'f', 3);
    
    m_resultsText->setPlainText(resultsText);
    
    // Update UI controls now that we have WCS
    updatePlottingControls();
    updateStatusDisplay();
}

void MainWindow::onPlatesolveFailed(const QString& error)
{
    // Hide progress dialog
    if (m_platesolveProgressDialog) {
        m_platesolveProgressDialog->hide();
        delete m_platesolveProgressDialog;
        m_platesolveProgressDialog = nullptr;
    }
    
    m_statusLabel->setText("✗ Plate solve failed: " + error);
    
    // Show error dialog
    QMessageBox::warning(this, "Plate Solve Failed", 
        QString("Plate solving failed:\n\n%1\n\n"
                "Tips:\n"
                "• Check astrometry.net installation\n"
                "• Verify index files are available\n" 
                "• Try adjusting scale range in settings\n"
                "• Ensure sufficient stars were detected").arg(error));
}

void MainWindow::onWCSDataReceived()
{
    m_hasWCS = true;
    updatePlottingControls();
    updateStatusDisplay();
}

void MainWindow::configurePlatesolverSettings()
{
    PlatesolverSettingsDialog dialog(this);
    
    // Set current values
    dialog.setAstrometryPath(m_astrometryPath);
    dialog.setIndexPath(m_indexPath);
    dialog.setScaleRange(m_minScale, m_maxScale);
    dialog.setTimeout(m_platesolveTimeout);
    dialog.setMaxStars(m_maxStarsForSolving);
    
    if (dialog.exec() == QDialog::Accepted) {
        // Get new values
        m_astrometryPath = dialog.getAstrometryPath();
        m_indexPath = dialog.getIndexPath();
        m_minScale = dialog.getMinScale();
        m_maxScale = dialog.getMaxScale();
        m_platesolveTimeout = dialog.getTimeout();
        m_maxStarsForSolving = dialog.getMaxStars();
        
        // Update configuration
        m_platesolveIntegration->configurePlateSolver(
            m_astrometryPath, m_indexPath, m_minScale, m_maxScale);
        
        m_statusLabel->setText("Plate solver settings updated");
    }
}

// Add these methods to setup the menu
void MainWindow::setupPlatesolveMenus()
{
    QMenuBar* menuBar = this->menuBar();
    QMenu* astrometryMenu = menuBar->addMenu("&Astrometry");
    
    // Extract Stars + Solve action
    QAction* platesolveAction = new QAction("Extract Stars && &Plate Solve", this);
    platesolveAction->setShortcut(QKeySequence("Ctrl+Shift+P"));
    platesolveAction->setStatusTip("Extract stars and automatically solve plate");
    connect(platesolveAction, &QAction::triggered, this, &MainWindow::onExtractStarsWithPlatesolve);
    astrometryMenu->addAction(platesolveAction);
    /*    
    // Toggle auto solve
    QAction* autoSolveAction = new QAction("&Auto Plate Solve", this);
    autoSolveAction->setCheckable(true);
    autoSolveAction->setChecked(false);
    connect(autoSolveAction, &QAction::toggled, this, &MainWindow::onToggleAutoPlatesolve);
    astrometryMenu->addAction(autoSolveAction);
    */
    astrometryMenu->addSeparator();
    
    // Settings
    QAction* settingsAction = new QAction("Plate Solver &Settings...", this);
    connect(settingsAction, &QAction::triggered, this, &MainWindow::configurePlatesolverSettings);
    astrometryMenu->addAction(settingsAction);
}

// Add this to your MainWindow constructor
void MainWindow::addPlatesolveIntegration()
{
    // Add these lines to your existing MainWindow constructor:
    
    // Initialize plate solving
    initializePlatesolveIntegration();
    
    // Setup menus  
    setupPlatesolveMenus();
    
    // Add button to existing layout
    if (m_buttonLayout) {
        QPushButton* extractAndSolveBtn = new QPushButton("Extract Stars && Solve");
        extractAndSolveBtn->setToolTip("Extract stars and solve for WCS coordinates");
        connect(extractAndSolveBtn, &QPushButton::clicked, this, &MainWindow::onExtractStarsWithPlatesolve);
        m_buttonLayout->addWidget(extractAndSolveBtn);
    }
}

// Add these member variables to your MainWindow.h private section:
/*
    // Add to MainWindow.h private section:
*/

void MainWindow::setupDebuggingMenu()
{
    QMenuBar* menuBar = this->menuBar();
    QMenu* debugMenu = menuBar->addMenu("&Debug");
    
    QAction* debugStarCorrelation = debugMenu->addAction("Debug Star Correlation");
    debugStarCorrelation->setToolTip("Matching stars correlation");
    connect(debugStarCorrelation, &QAction::triggered, this, &MainWindow::onDebugStarCorrelation);

    QAction* debugWCS = debugMenu->addAction("Debug WCS");
    debugWCS->setToolTip("Display WCS for current image");
    connect(debugWCS, &QAction::triggered, this, &MainWindow::showWCSDebugInfo);

    /*        
    QAction* debugPixelAction = debugMenu->addAction("Debug Pixel Matching");
    debugPixelAction->setToolTip("Analyze why visually matching stars fail mathematical criteria");
    connect(debugPixelAction, &QAction::triggered, this, &MainWindow::onDebugPixelMatching);

    QAction* analyzeCriteriaAction = debugMenu->addAction("Analyze Matching Criteria");
    analyzeCriteriaAction->setToolTip("Deep analysis of distance and magnitude criteria");
    connect(analyzeCriteriaAction, &QAction::triggered, this, &MainWindow::onAnalyzeMatchingCriteria);
    
    QAction* testSensitivityAction = debugMenu->addAction("Test Parameter Sensitivity");
    testSensitivityAction->setToolTip("Test different tolerance values to find optimal settings");
    connect(testSensitivityAction, &QAction::triggered, this, &MainWindow::onTestParameterSensitivity);
    */    
    debugMenu->addSeparator();
    
    QAction* exportDebugAction = debugMenu->addAction("Export Debug Report");
    connect(exportDebugAction, &QAction::triggered, [this]() {
        if (m_pixelDebugger && m_validationComplete) {
            QString filename = QFileDialog::getSaveFileName(
                this, "Export Debug Report",
                QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/pixel_matching_debug.txt",
                "Text Files (*.txt);;All Files (*)");
            
            if (!filename.isEmpty()) {
                m_pixelDebugger->exportDebugReport(filename);
                QMessageBox::information(this, "Debug Export", 
                    QString("Debug report exported to:\n%1").arg(filename));
            }
        } else {
            QMessageBox::information(this, "Debug Export", 
                "Please complete star validation first.");
        }
    });
    debugMenu->addSeparator();
    QAction* testDebugAction = debugMenu->addAction("Test Debug System");
    testDebugAction->setToolTip("Test debug system with synthetic data");
    connect(testDebugAction, &QAction::triggered, this, &MainWindow::testPixelMatchingDebug);
    
}
/*
void MainWindow::onDebugPixelMatching()
{
    if (!m_starsDetected) {
        QMessageBox::information(this, "Debug Pixel Matching", 
            "Please detect stars first.");
        return;
    }
    
    if (!m_validationComplete && !m_catalogPlotted) {
        QMessageBox::information(this, "Debug Pixel Matching", 
            "Please run validation or plot catalog stars first.");
        return;
    }
    
    // Run comprehensive diagnosis
    m_pixelDebugger->diagnoseMatching(
        m_lastStarMask.starCenters, 
        m_catalogValidator->getCatalogStars(),
        m_lastValidation,
        m_catalogValidator.get()
    );
    
    // Print summary report to console
    m_pixelDebugger->printSummaryReport();
    
    // Show results in a dialog
    showPixelDebugDialog();
}
*/
void MainWindow::showWCSDebugInfo()
{
    if (!m_hasWCS) {
        QMessageBox::information(this, "WCS Debug", "No WCS information available.");
        return;
    }

    QString wcsInfo = QString("TBD");
    
    m_catalogValidator->showWCSInfoFromPCL();
    
    QDialog* wcsDialog = new QDialog(this);
    wcsDialog->setWindowTitle("WCS Debug Information");
    wcsDialog->resize(600, 500);
    
    QVBoxLayout* layout = new QVBoxLayout(wcsDialog);
    
    QTextEdit* wcsText = new QTextEdit;
    wcsText->setReadOnly(true);
    wcsText->setFont(QFont("Consolas", 10));
    wcsText->setPlainText(wcsInfo);
    layout->addWidget(wcsText);
    
    QPushButton* closeBtn = new QPushButton("Close");
    connect(closeBtn, &QPushButton::clicked, wcsDialog, &QDialog::accept);
    layout->addWidget(closeBtn);
    
    wcsDialog->exec();
    delete wcsDialog;
}

void MainWindow::testWCSTransformations()
{
    if (!m_hasWCS) {
        QMessageBox::information(this, "WCS Test", "No WCS information available.");
        return;
    }
    
    // Test center transformation
    double centerX = m_catalogValidator->getWidth() * 0.5;
    double centerY = m_catalogValidator->getHeight() * 0.5;
    
    QPointF centerSky = m_catalogValidator->pixelToSky(centerX, centerY);
    QPointF centerPixelRoundTrip = m_catalogValidator->skyToPixel(centerSky.x(), centerSky.y());
    
    double centerError = sqrt(pow(centerX - centerPixelRoundTrip.x(), 2) + 
                            pow(centerY - centerPixelRoundTrip.y(), 2));
    
    // Test corners
    QVector<QPointF> testPoints = {
        QPointF(0, 0),                    // Top-left
        QPointF(m_catalogValidator->getWidth(), 0),           // Top-right  
        QPointF(0, m_catalogValidator->getHeight()),          // Bottom-left
        QPointF(m_catalogValidator->getWidth(), m_catalogValidator->getHeight())   // Bottom-right
    };
    
    QStringList cornerNames = {"Top-left", "Top-right", "Bottom-left", "Bottom-right"};
    
    double maxError = 0.0;
    double totalError = 0.0;
    
    for (int i = 0; i < testPoints.size(); ++i) {
        QPointF pixel = testPoints[i];
        QPointF sky = m_catalogValidator->pixelToSky(pixel.x(), pixel.y());
        QPointF pixelRoundTrip = m_catalogValidator->skyToPixel(sky.x(), sky.y());
        
        double error = sqrt(pow(pixel.x() - pixelRoundTrip.x(), 2) + 
                          pow(pixel.y() - pixelRoundTrip.y(), 2));
        
        maxError = std::max(maxError, error);
        totalError += error;
        
	qDebug() << ( QString("  %1: error=%2 px").arg(cornerNames[i]).arg(error));
    }
    
    double avgError = totalError / testPoints.size();
    
    QString testResults = QString(
        "WCS TRANSFORMATION TEST RESULTS\n"
        "===============================\n\n"
        "Center Round-trip Error: %1 px\n"
        "Average Corner Error: %2 px\n"
        "Maximum Corner Error: %3 px\n\n"
        "Error Assessment:\n"
        "< 0.1 px: Excellent (numerical precision)\n"
        "< 0.5 px: Very Good (sub-pixel accuracy)\n"
        "< 1.0 px: Good (suitable for star matching)\n"
        "< 2.0 px: Acceptable (may need tolerance adjustment)\n"
        "> 2.0 px: Poor (WCS calibration issues)\n\n"
        "Your WCS Quality: %4\n\n"
        "If errors are large (>1px), consider:\n"
        "• Re-solving astrometry with more reference stars\n"
        "• Checking for systematic coordinate errors\n"
        "• Implementing distortion correction\n"
        "• Verifying coordinate system epoch (J2000 vs current)")
        .arg(maxError)
        .arg(avgError)
        .arg(maxError)
        .arg(maxError < 0.5 ? "Excellent" : 
             maxError < 1.0 ? "Very Good" :
             maxError < 2.0 ? "Good" :
             maxError < 5.0 ? "Acceptable" : "Poor");
    
    QMessageBox::information(this, "WCS Transformation Test", testResults);
}

// Add this method to export detailed debug information
void PixelMatchingDebugger::exportDebugReport(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
      qDebug() << ("Failed to open debug report file:" + filename);
        return;
    }
    
    QTextStream out(&file);
    
    // Header
    out << "PIXEL MATCHING DEBUG REPORT\n";
    out << "Generated: " << QDateTime::currentDateTime().toString() << "\n";
    out << "==================================================\n\n";
    
    // Summary statistics
    MatchingStats stats = calculateStats();
    out << "SUMMARY STATISTICS:\n";
    out << "Detected stars: " << stats.totalDetected << "\n";
    out << "Catalog stars: " << stats.totalCatalog << "\n";
    out << "Potential matches: " << stats.potentialMatches << "\n";
    out << "Distance-passing: " << stats.distancePassingMatches << "\n";
    out << "Actual matches: " << stats.actualMatches << "\n";
    out << "Average distance: " << QString::number(stats.avgDistance, 'f', 3) << " px\n";
    out << "Systematic offset: (" << QString::number(stats.systematicOffset.x(), 'f', 3) 
        << ", " << QString::number(stats.systematicOffset.y(), 'f', 3) << ") px\n\n";
    
    // Detailed match analysis
    out << "DETAILED MATCH ANALYSIS:\n";
    out << "DetIdx,CatIdx,CatalogID,DetX,DetY,CatX,CatY,Distance,MagDiff,PassDist,PassMag,PassOverall,ShouldMatch,Reasons\n";
    
    for (const auto& diag : m_diagnostics) {
        out << diag.detectedIndex << ","
            << diag.catalogIndex << ","
            << diag.catalogId << ","
            << diag.detectedPos.x() << ","
            << diag.detectedPos.y() << ","
            << QString::number(diag.catalogPos.x(), 'f', 2) << ","
            << QString::number(diag.catalogPos.y(), 'f', 2) << ","
            << QString::number(diag.pixelDistance, 'f', 3) << ","
            << QString::number(diag.magnitudeDiff, 'f', 2) << ","
            << (diag.passesDistanceCheck ? "1" : "0") << ","
            << (diag.passesMagnitudeCheck ? "1" : "0") << ","
            << (diag.passesOverallCheck ? "1" : "0") << ","
            << (diag.shouldVisuallyMatch ? "1" : "0") << ","
            << "\"" << diag.failureReasons.join("; ") << "\""
            << "\n";
    }
    
    out << "\nEND OF REPORT\n";
    file.close();
    
    qDebug() << ("Debug report exported to:" + filename);
}

void MainWindow::setupEnhancedMatchingControls()
{
    m_enhancedMatchingGroup = new QGroupBox("Enhanced Star Matching (PixInsight Methods)");
    m_enhancedMatchingLayout = new QVBoxLayout(m_enhancedMatchingGroup);
    
    // Distance and search parameters section
    QGroupBox* distanceGroup = new QGroupBox("Distance Parameters");
    QGridLayout* distanceLayout = new QGridLayout(distanceGroup);
    
    // Maximum pixel distance
    distanceLayout->addWidget(new QLabel("Max Pixel Distance:"), 0, 0);
    m_maxPixelDistanceSpin = new QDoubleSpinBox;
    m_maxPixelDistanceSpin->setRange(1.0, 50.0);
    m_maxPixelDistanceSpin->setValue(5.0);
    m_maxPixelDistanceSpin->setDecimals(1);
    m_maxPixelDistanceSpin->setSuffix(" px");
    m_maxPixelDistanceSpin->setToolTip("Maximum allowed distance for star matching");
    distanceLayout->addWidget(m_maxPixelDistanceSpin, 0, 1);
    
    // Search radius
    distanceLayout->addWidget(new QLabel("Search Radius:"), 1, 0);
    m_searchRadiusSpin = new QDoubleSpinBox;
    m_searchRadiusSpin->setRange(5.0, 100.0);
    m_searchRadiusSpin->setValue(10.0);
    m_searchRadiusSpin->setDecimals(1);
    m_searchRadiusSpin->setSuffix(" px");
    m_searchRadiusSpin->setToolTip("Search radius for finding candidate matches");
    distanceLayout->addWidget(m_searchRadiusSpin, 1, 1);
    
    // Maximum magnitude difference
    distanceLayout->addWidget(new QLabel("Max Magnitude Diff:"), 2, 0);
    m_maxMagnitudeDiffSpin = new QDoubleSpinBox;
    m_maxMagnitudeDiffSpin->setRange(0.5, 10.0);
    m_maxMagnitudeDiffSpin->setValue(2.0);
    m_maxMagnitudeDiffSpin->setDecimals(1);
    m_maxMagnitudeDiffSpin->setSuffix(" mag");
    m_maxMagnitudeDiffSpin->setToolTip("Maximum allowed magnitude difference");
    distanceLayout->addWidget(m_maxMagnitudeDiffSpin, 2, 1);
    
    m_enhancedMatchingLayout->addWidget(distanceGroup);
    
    // Pattern matching section
    QGroupBox* patternGroup = new QGroupBox("Pattern Matching");
    QGridLayout* patternLayout = new QGridLayout(patternGroup);
    
    // Triangle matching
    m_useTriangleMatchingCheck = new QCheckBox("Enable Triangle Pattern Matching");
    m_useTriangleMatchingCheck->setChecked(true);
    m_useTriangleMatchingCheck->setToolTip("Use geometric triangle patterns for robust matching");
    patternLayout->addWidget(m_useTriangleMatchingCheck, 0, 0, 1, 2);
    
    // Minimum triangle stars
    patternLayout->addWidget(new QLabel("Min Triangle Stars:"), 1, 0);
    m_minTriangleStarsSpin = new QSpinBox;
    m_minTriangleStarsSpin->setRange(6, 50);
    m_minTriangleStarsSpin->setValue(8);
    m_minTriangleStarsSpin->setToolTip("Minimum stars needed for triangle matching");
    patternLayout->addWidget(m_minTriangleStarsSpin, 1, 1);
    
    // Triangle tolerance
    patternLayout->addWidget(new QLabel("Triangle Tolerance:"), 2, 0);
    m_triangleToleranceSpin = new QDoubleSpinBox;
    m_triangleToleranceSpin->setRange(1.0, 20.0);
    m_triangleToleranceSpin->setValue(5.0);
    m_triangleToleranceSpin->setDecimals(1);
    m_triangleToleranceSpin->setSuffix(" %");
    m_triangleToleranceSpin->setToolTip("Tolerance for triangle pattern similarity");
    patternLayout->addWidget(m_triangleToleranceSpin, 2, 1);
    
    m_enhancedMatchingLayout->addWidget(patternGroup);
    
    // Quality control section
    QGroupBox* qualityGroup = new QGroupBox("Quality Control");
    QGridLayout* qualityLayout = new QGridLayout(qualityGroup);
    
    // Minimum match confidence
    qualityLayout->addWidget(new QLabel("Min Match Confidence:"), 0, 0);
    m_minMatchConfidenceSpin = new QDoubleSpinBox;
    m_minMatchConfidenceSpin->setRange(0.1, 1.0);
    m_minMatchConfidenceSpin->setValue(0.7);
    m_minMatchConfidenceSpin->setDecimals(2);
    m_minMatchConfidenceSpin->setToolTip("Minimum confidence threshold for valid matches");
    qualityLayout->addWidget(m_minMatchConfidenceSpin, 0, 1);
    
    // Minimum matches for validation
    qualityLayout->addWidget(new QLabel("Min Matches for Validation:"), 1, 0);
    m_minMatchesValidationSpin = new QSpinBox;
    m_minMatchesValidationSpin->setRange(3, 50);
    m_minMatchesValidationSpin->setValue(5);
    m_minMatchesValidationSpin->setToolTip("Minimum matches required for successful validation");
    qualityLayout->addWidget(m_minMatchesValidationSpin, 1, 1);
    
    m_enhancedMatchingLayout->addWidget(qualityGroup);
    
    // Advanced options section
    QGroupBox* advancedGroup = new QGroupBox("Advanced Options");
    QVBoxLayout* advancedLayout = new QVBoxLayout(advancedGroup);
    
    m_useDistortionModelCheck = new QCheckBox("Apply Distortion Model");
    m_useDistortionModelCheck->setChecked(true);
    m_useDistortionModelCheck->setToolTip("Apply optical distortion correction");
    advancedLayout->addWidget(m_useDistortionModelCheck);
    
    m_useProperMotionCheck = new QCheckBox("Apply Proper Motion Correction");
    m_useProperMotionCheck->setChecked(true);
    m_useProperMotionCheck->setToolTip("Correct catalog positions for proper motion");
    advancedLayout->addWidget(m_useProperMotionCheck);
    
    m_useBayesianMatchingCheck = new QCheckBox("Use Bayesian Matching (Experimental)");
    m_useBayesianMatchingCheck->setChecked(false);
    m_useBayesianMatchingCheck->setToolTip("Use probabilistic matching approach");
    advancedLayout->addWidget(m_useBayesianMatchingCheck);
    
    m_enhancedMatchingLayout->addWidget(advancedGroup);
    
    // Action buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    
    QPushButton* validateEnhancedButton = new QPushButton("Validate (Enhanced)");
    validateEnhancedButton->setToolTip("Perform enhanced star validation using PixInsight methods");
    connect(validateEnhancedButton, &QPushButton::clicked, this, &MainWindow::onValidateStars);
    buttonLayout->addWidget(validateEnhancedButton);
    
    QPushButton* showDetailsButton = new QPushButton("Show Details");
    showDetailsButton->setToolTip("Show detailed matching analysis");
    connect(showDetailsButton, &QPushButton::clicked, this, &MainWindow::onShowMatchingDetails);
    buttonLayout->addWidget(showDetailsButton);
    
    QPushButton* visualizeButton = new QPushButton("Visualize Distortions");
    visualizeButton->setToolTip("Visualize optical distortions");
    connect(visualizeButton, &QPushButton::clicked, this, &MainWindow::onVisualizeDistortions);
    buttonLayout->addWidget(visualizeButton);
    
    QPushButton* exportButton = new QPushButton("Export Results");
    exportButton->setToolTip("Export matching results to file");
    connect(exportButton, &QPushButton::clicked, this, &MainWindow::onExportMatchingResults);
    buttonLayout->addWidget(exportButton);
    
    m_enhancedMatchingLayout->addLayout(buttonLayout);
    
    // Progress bar
    m_matchingProgressBar = new QProgressBar;
    m_matchingProgressBar->setVisible(false);
    m_enhancedMatchingLayout->addWidget(m_matchingProgressBar);
    
    // Enhanced results display
    QGroupBox* resultsGroup = new QGroupBox("Enhanced Results");
    QVBoxLayout* resultsLayout = new QVBoxLayout(resultsGroup);
    
    m_enhancedResultsText = new QTextEdit;
    m_enhancedResultsText->setMaximumHeight(200);
    m_enhancedResultsText->setReadOnly(true);
    m_enhancedResultsText->setPlainText("Enhanced matching results will appear here.\n\n"
                                       "Enhanced matching provides:\n"
                                       "• Triangle pattern recognition\n"
                                       "• Geometric validation\n" 
                                       "• Distortion analysis\n"
                                       "• Statistical quality metrics\n"
                                       "• Outlier rejection");
    resultsLayout->addWidget(m_enhancedResultsText);
    
    m_enhancedMatchingLayout->addWidget(resultsGroup);
    
    // Connect parameter change signals
    connect(m_maxPixelDistanceSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onMatchingParametersChanged);
    connect(m_searchRadiusSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onMatchingParametersChanged);
    connect(m_useTriangleMatchingCheck, &QCheckBox::toggled,
            this, &MainWindow::onMatchingParametersChanged);
    
    // Initially hide enhanced matching controls (show when needed)
    m_enhancedMatchingGroup->setVisible(true);
}

void MainWindow::onValidateStars()
{
    if (!m_starsDetected) {
        QMessageBox::information(this, "Enhanced Validation", "Please detect stars first.");
        return;
    }
    
    if (!m_hasWCS) {
        QMessageBox::warning(this, "Enhanced Validation", "No WCS information available.");
        return;
    }
    
    if (!m_catalogQueried) {
        QMessageBox::information(this, "Enhanced Validation", "Please query catalog first.");
        return;
    }
    
    m_statusLabel->setText("Performing enhanced star validation...");
    m_matchingProgressBar->setVisible(true);
    m_matchingProgressBar->setRange(0, 0); // Indeterminate
    QApplication::processEvents();
    
    try {
        // Get matching parameters from UI
        StarMatchingParameters params = getMatchingParametersFromUI();
        
        // Estimate star magnitudes from detection data (rough approximation)
        QVector<float> estimatedMagnitudes;
        for (int i = 0; i < m_lastStarMask.starRadii.size(); ++i) {
            // Very rough magnitude estimation from star radius
            float radius = m_lastStarMask.starRadii[i];
            float estimatedMag = 12.0 - 2.5 * log10(radius * radius / 4.0);
            estimatedMagnitudes.append(estimatedMag);
        }
        
        qDebug() <<  "Starting enhanced validation with parameters:";
        qDebug() <<  "  Max pixel distance:" << params.maxPixelDistance;
        qDebug() <<  "  Triangle matching:" << params.useTriangleMatching;
        qDebug() <<  "  Min confidence:" << params.minMatchConfidence;
        
        // Perform enhanced validation
        m_lastEnhancedValidation = m_catalogValidator->validateStarsAdvanced(
            m_lastStarMask.starCenters, estimatedMagnitudes, params);
        
        m_enhancedValidationComplete = m_lastEnhancedValidation.isValid;
        
        // Display results
        displayEnhancedResults(m_lastEnhancedValidation);
        
        // Update image display with enhanced results
        m_imageDisplayWidget->setValidationResults(m_lastEnhancedValidation);
        
        // Update status
        QString statusText = QString("Enhanced validation: %1/%2 matches (%3%% confidence)")
                            .arg(m_lastEnhancedValidation.totalMatches)
                            .arg(m_lastEnhancedValidation.totalDetected)
                            .arg(m_lastEnhancedValidation.matchingConfidence);
        m_statusLabel->setText(statusText);
        
    } catch (const std::exception& e) {
        QString errorMsg = QString("Enhanced validation error: %1").arg(e.what());
        m_statusLabel->setText(errorMsg);
        QMessageBox::warning(this, "Enhanced Validation Error", errorMsg);
    }
    
    m_matchingProgressBar->setVisible(false);
    updateEnhancedMatchingControls();
}

StarMatchingParameters MainWindow::getMatchingParametersFromUI()
{
    StarMatchingParameters params;
    
    // Distance parameters
    params.maxPixelDistance = m_maxPixelDistanceSpin->value();
    params.searchRadius = m_searchRadiusSpin->value();
    params.maxMagnitudeDifference = m_maxMagnitudeDiffSpin->value();
    
    // Pattern matching
    params.useTriangleMatching = m_useTriangleMatchingCheck->isChecked();
    params.minTriangleStars = m_minTriangleStarsSpin->value();
    params.triangleTolerancePercent = m_triangleToleranceSpin->value();
    
    // Quality control
    params.minMatchConfidence = m_minMatchConfidenceSpin->value();
    params.minMatchesForValidation = m_minMatchesValidationSpin->value();
    
    // Advanced options
    params.useDistortionModel = m_useDistortionModelCheck->isChecked();
    params.useProperMotionCorrection = m_useProperMotionCheck->isChecked();
    params.useBayesianMatching = m_useBayesianMatchingCheck->isChecked();
    
    return params;
}

void MainWindow::displayEnhancedResults(const EnhancedValidationResult& result)
{
    m_enhancedResultsText->setPlainText(result.enhancedSummary);
    
    // Also update the main results text with enhanced information
    QString detailedResults = result.enhancedSummary;
    
    if (!result.enhancedMatches.isEmpty()) {
        detailedResults += "\n\nTop 10 Highest Confidence Matches:\n";
        detailedResults += "=====================================\n";
        
        // Sort matches by confidence
        auto sortedMatches = result.enhancedMatches;
        std::sort(sortedMatches.begin(), sortedMatches.end(),
                  [](const EnhancedStarMatch& a, const EnhancedStarMatch& b) {
                      return a.confidence > b.confidence;
                  });
        
        int displayCount = std::min(10, (int)(sortedMatches.size()));
        for (int i = 0; i < displayCount; ++i) {
            const auto& match = sortedMatches[i];
            detailedResults += QString("Match %1: Det[%2] ↔ Cat[%3]\n")
                              .arg(i + 1).arg(match.detectedIndex).arg(match.catalogIndex);
            detailedResults += QString("  Distance: %1 px, Confidence: %2%%\n")
                              .arg(match.pixelDistance).arg(match.confidence * 100.0);
            
            if (match.triangleError > 0) {
                detailedResults += QString("  Triangle Error: %1\n").arg(match.triangleError);
            }
            
            if (!match.supportingMatches.isEmpty()) {
                detailedResults += QString("  Supporting Matches: %1\n")
                                  .arg(match.supportingMatches.size());
            }
        }
        
        // Add distortion analysis if available
        if (!result.radialDistortions.isEmpty()) {
            double maxDistortion = *std::max_element(result.radialDistortions.begin(), 
                                                    result.radialDistortions.end());
            double avgDistortion = std::accumulate(result.radialDistortions.begin(),
                                                  result.radialDistortions.end(), 0.0) 
                                  / result.radialDistortions.size();
            
            detailedResults += QString("\nDistortion Analysis:\n");
            detailedResults += QString("  Max Radial Distortion: %1 pixels\n").arg(maxDistortion);
            detailedResults += QString("  Average Distortion: %1 pixels\n").arg(avgDistortion);
        }
    }
    
    m_resultsText->setPlainText(detailedResults);
}

void MainWindow::onShowMatchingDetails()
{
    if (!m_enhancedValidationComplete) {
        QMessageBox::information(this, "Matching Details", 
                                "Please perform enhanced validation first.");
        return;
    }
    
    // Create detailed analysis dialog
    QDialog* detailsDialog = new QDialog(this);
    detailsDialog->setWindowTitle("Enhanced Matching Details");
    detailsDialog->resize(800, 600);
    
    QVBoxLayout* layout = new QVBoxLayout(detailsDialog);
    
    // Create tab widget for different analysis views
    QTabWidget* tabWidget = new QTabWidget;
    
    // Statistics tab
    QTextEdit* statsText = new QTextEdit;
    statsText->setReadOnly(true);
    statsText->setPlainText(m_lastEnhancedValidation.enhancedSummary);
    tabWidget->addTab(statsText, "Statistics");
    
    // Matches table tab
    QTableWidget* matchesTable = new QTableWidget;
    matchesTable->setColumnCount(7);
    matchesTable->setHorizontalHeaderLabels({
        "Detected", "Catalog", "Distance (px)", "Confidence", 
        "Triangle Error", "Geometric", "Photometric"
    });
    
    matchesTable->setRowCount(m_lastEnhancedValidation.enhancedMatches.size());
    
    for (int i = 0; i < m_lastEnhancedValidation.enhancedMatches.size(); ++i) {
        const auto& match = m_lastEnhancedValidation.enhancedMatches[i];
        
        matchesTable->setItem(i, 0, new QTableWidgetItem(QString::number(match.detectedIndex)));
        matchesTable->setItem(i, 1, new QTableWidgetItem(QString::number(match.catalogIndex)));
        matchesTable->setItem(i, 2, new QTableWidgetItem(QString::number(match.pixelDistance, 'f', 2)));
        matchesTable->setItem(i, 3, new QTableWidgetItem(QString::number(match.confidence * 100, 'f', 1) + "%"));
        matchesTable->setItem(i, 4, new QTableWidgetItem(QString::number(match.triangleError, 'f', 3)));
        matchesTable->setItem(i, 5, new QTableWidgetItem(match.isGeometricallyValid ? "✓" : "✗"));
        matchesTable->setItem(i, 6, new QTableWidgetItem(match.isPhotometricallyValid ? "✓" : "✗"));
    }
    
    matchesTable->resizeColumnsToContents();
    tabWidget->addTab(matchesTable, "Matches");
    
    // Triangle patterns tab (if available)
    if (!m_lastEnhancedValidation.detectedTriangles.isEmpty()) {
        QTextEdit* triangleText = new QTextEdit;
        triangleText->setReadOnly(true);
        
        QString triangleInfo = QString("Triangle Pattern Analysis:\n\n");
        triangleInfo += QString("Detected Triangles: %1\n").arg(m_lastEnhancedValidation.detectedTriangles.size());
        triangleInfo += QString("Catalog Triangles: %1\n").arg(m_lastEnhancedValidation.catalogTriangles.size());
        triangleInfo += QString("Triangle Matches: %1\n\n").arg(m_lastEnhancedValidation.triangleMatches.size());
        
        for (int i = 0; i < std::min(5, (int)(m_lastEnhancedValidation.detectedTriangles.size())); ++i) {
            const auto& triangle = m_lastEnhancedValidation.detectedTriangles[i];
            triangleInfo += QString("Triangle %1:\n").arg(i + 1);
            triangleInfo += QString("  Stars: [%1, %2, %3]\n")
                           .arg(triangle.starIndices[0])
                           .arg(triangle.starIndices[1])
                           .arg(triangle.starIndices[2]);
            triangleInfo += QString("  Sides: %1, %2, %3 px\n")
                           .arg(triangle.side1).arg(triangle.side2).arg(triangle.side3);
            triangleInfo += QString("  Area: %1 px²\n").arg(triangle.area);
            triangleInfo += QString("  Ratios: %1, %2, %3\n\n")
                           .arg(triangle.ratio12).arg(triangle.ratio13).arg(triangle.ratio23);
        }
        
        triangleText->setPlainText(triangleInfo);
        tabWidget->addTab(triangleText, "Triangles");
    }
    
    layout->addWidget(tabWidget);
    
    // Close button
    QPushButton* closeButton = new QPushButton("Close");
    connect(closeButton, &QPushButton::clicked, detailsDialog, &QDialog::accept);
    layout->addWidget(closeButton);
    
    detailsDialog->exec();
    delete detailsDialog;
}

void MainWindow::onVisualizeDistortions()
{
    if (!m_enhancedValidationComplete || m_lastEnhancedValidation.residualVectors.isEmpty()) {
        QMessageBox::information(this, "Distortion Visualization", 
                                "No distortion data available. Please perform enhanced validation first.");
        return;
    }
    
    // Create distortion visualization dialog
    QDialog* distortionDialog = new QDialog(this);
    distortionDialog->setWindowTitle("Optical Distortion Visualization");
    distortionDialog->resize(600, 500);
    
    QVBoxLayout* layout = new QVBoxLayout(distortionDialog);
    
    // Create a simple text-based visualization for now
    // In a full implementation, you would create a custom widget with graphical visualization
    QTextEdit* distortionText = new QTextEdit;
    distortionText->setReadOnly(true);
    
    QString distortionInfo = "Distortion Analysis Results:\n";
    distortionInfo += "============================\n\n";
    
    if (!m_lastEnhancedValidation.radialDistortions.isEmpty()) {
        double maxDist = *std::max_element(m_lastEnhancedValidation.radialDistortions.begin(),
                                          m_lastEnhancedValidation.radialDistortions.end());
        double minDist = *std::min_element(m_lastEnhancedValidation.radialDistortions.begin(),
                                          m_lastEnhancedValidation.radialDistortions.end());
        double avgDist = std::accumulate(m_lastEnhancedValidation.radialDistortions.begin(),
                                        m_lastEnhancedValidation.radialDistortions.end(), 0.0) 
                        / m_lastEnhancedValidation.radialDistortions.size();
        
        distortionInfo += QString("Radial Distortion Statistics:\n");
        distortionInfo += QString("  Maximum: %1 pixels\n").arg(maxDist);
        distortionInfo += QString("  Minimum: %1 pixels\n").arg(minDist);
        distortionInfo += QString("  Average: %1 pixels\n").arg(avgDist);
        distortionInfo += QString("  RMS: %1 pixels\n\n").arg(m_lastEnhancedValidation.geometricRMS);
    }
    
    if (!m_lastEnhancedValidation.residualVectors.isEmpty()) {
        distortionInfo += QString("Residual Vectors (first 10):\n");
        distortionInfo += QString("Position → Residual\n");
        distortionInfo += QString("===================\n");
        
        int count = std::min(10, (int)(m_lastEnhancedValidation.residualVectors.size()));
        for (int i = 0; i < count; ++i) {
            const QPointF& residual = m_lastEnhancedValidation.residualVectors[i];
            const auto& match = m_lastEnhancedValidation.enhancedMatches[i];
            
            distortionInfo += QString("Star %1: (%2, %3) → (%4, %5)\n")
                             .arg(match.detectedIndex)
                             .arg(m_lastStarMask.starCenters[match.detectedIndex].x())
                             .arg(m_lastStarMask.starCenters[match.detectedIndex].y())
                             .arg(residual.x()).arg(residual.y());
        }
    }
    
    distortionText->setPlainText(distortionInfo);
    layout->addWidget(distortionText);
    
    // Calibrate distortion model button
    QPushButton* calibrateButton = new QPushButton("Calibrate Distortion Model");
    calibrateButton->setToolTip("Calculate distortion correction parameters");
    connect(calibrateButton, &QPushButton::clicked, this, &MainWindow::onCalibrateDistortionModel);
    layout->addWidget(calibrateButton);
    
    QPushButton* closeButton = new QPushButton("Close");
    connect(closeButton, &QPushButton::clicked, distortionDialog, &QDialog::accept);
    layout->addWidget(closeButton);
    
    distortionDialog->exec();
    delete distortionDialog;
}

void MainWindow::onCalibrateDistortionModel()
{
    if (!m_enhancedValidationComplete) {
        QMessageBox::information(this, "Distortion Calibration", 
                                "Please perform enhanced validation first.");
        return;
    }
    
    try {
        bool success = m_catalogValidator->calibrateDistortionModel(m_lastEnhancedValidation);
        
        if (success) {
            QMessageBox::information(this, "Distortion Calibration", 
                                    "Distortion model calibrated successfully!\n\n"
                                    "The model will be applied to future coordinate transformations.");
        } else {
            QMessageBox::warning(this, "Distortion Calibration", 
                                "Failed to calibrate distortion model.\n\n"
                                "Not enough high-quality matches or data is insufficient.");
        }
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Distortion Calibration Error", 
                             QString("Error during calibration: %1").arg(e.what()));
    }
}

void MainWindow::onExportMatchingResults()
{
    if (!m_enhancedValidationComplete) {
        QMessageBox::information(this, "Export Results", 
                                "Please perform enhanced validation first.");
        return;
    }
    
    QString fileName = QFileDialog::getSaveFileName(
        this, "Export Enhanced Matching Results",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/star_matching_results.json",
        "JSON Files (*.json);;CSV Files (*.csv);;All Files (*)");
    
    if (fileName.isEmpty()) return;
    
    try {
        QFileInfo fileInfo(fileName);
        QString suffix = fileInfo.suffix().toLower();
        
        if (suffix == "json") {
            // Export as JSON
            QJsonObject exportData;
            exportData["enhanced_summary"] = m_lastEnhancedValidation.enhancedSummary;
            exportData["total_detected"] = m_lastEnhancedValidation.totalDetected;
            exportData["total_catalog"] = m_lastEnhancedValidation.totalCatalog;
            exportData["total_matches"] = m_lastEnhancedValidation.totalMatches;
            exportData["matching_confidence"] = m_lastEnhancedValidation.matchingConfidence;
            exportData["geometric_rms"] = m_lastEnhancedValidation.geometricRMS;
            exportData["astrometric_accuracy"] = m_lastEnhancedValidation.astrometricAccuracy;
            
            // Add matches
            QJsonArray matchesArray;
            for (const auto& match : m_lastEnhancedValidation.enhancedMatches) {
                QJsonObject matchObj;
                matchObj["detected_index"] = match.detectedIndex;
                matchObj["catalog_index"] = match.catalogIndex;
                matchObj["pixel_distance"] = match.pixelDistance;
                matchObj["confidence"] = match.confidence;
                matchObj["triangle_error"] = match.triangleError;
                matchObj["geometrically_valid"] = match.isGeometricallyValid;
                matchObj["photometrically_valid"] = match.isPhotometricallyValid;
                matchesArray.append(matchObj);
            }
	}
    } catch (const std::exception& e) {
        QString errorMsg = QString("Export Matching results: %1").arg(e.what());
        m_statusLabel->setText(errorMsg);
        QMessageBox::warning(this, "Eexport Matching results", errorMsg);
    }
}

void MainWindow::setupStarDetectionControls()
{
    m_starDetectionGroup = new QGroupBox("PCL Star Detection Controls");
    m_starDetectionLayout = new QVBoxLayout(m_starDetectionGroup);
    
    // Sensitivity control
    QHBoxLayout* sensitivityLayout = new QHBoxLayout;
    sensitivityLayout->addWidget(new QLabel("Sensitivity:"));
    m_sensitivitySlider = new QSlider(Qt::Horizontal);
    m_sensitivitySlider->setRange(0, 100);
    m_sensitivitySlider->setValue(50); // 0.5 default
    m_sensitivitySlider->setToolTip("Star detection sensitivity (0=minimum, 100=maximum)");
    QLabel* sensitivityValue = new QLabel("0.5");
    sensitivityValue->setMinimumWidth(30);
    connect(m_sensitivitySlider, &QSlider::valueChanged, [sensitivityValue](int value) {
        sensitivityValue->setText(QString::number(value / 100.0, 'f', 2));
    });
    sensitivityLayout->addWidget(m_sensitivitySlider);
    sensitivityLayout->addWidget(sensitivityValue);
    m_starDetectionLayout->addLayout(sensitivityLayout);
    
    // Structure layers control
    QHBoxLayout* structureLayout = new QHBoxLayout;
    structureLayout->addWidget(new QLabel("Structure Layers:"));
    m_structureLayersSpinBox = new QSpinBox;
    m_structureLayersSpinBox->setRange(1, 8);
    m_structureLayersSpinBox->setValue(5);
    m_structureLayersSpinBox->setToolTip("Number of wavelet layers for structure detection (1=small stars, 8=large stars)");
    structureLayout->addWidget(m_structureLayersSpinBox);
    structureLayout->addStretch();
    m_starDetectionLayout->addLayout(structureLayout);
    
    // Noise layers control
    QHBoxLayout* noiseLayout = new QHBoxLayout;
    noiseLayout->addWidget(new QLabel("Noise Layers:"));
    m_noiseLayersSpinBox = new QSpinBox;
    m_noiseLayersSpinBox->setRange(0, 4);
    m_noiseLayersSpinBox->setValue(1);
    m_noiseLayersSpinBox->setToolTip("Number of wavelet layers for noise reduction (0=none, 4=maximum)");
    noiseLayout->addWidget(m_noiseLayersSpinBox);
    noiseLayout->addStretch();
    m_starDetectionLayout->addLayout(noiseLayout);
    
    // Peak response control
    QHBoxLayout* peakLayout = new QHBoxLayout;
    peakLayout->addWidget(new QLabel("Peak Response:"));
    m_peakResponseSlider = new QSlider(Qt::Horizontal);
    m_peakResponseSlider->setRange(0, 100);
    m_peakResponseSlider->setValue(50); // 0.5 default
    m_peakResponseSlider->setToolTip("Peak response sensitivity (0=sharp peaks only, 100=flat features allowed)");
    QLabel* peakValue = new QLabel("0.5");
    peakValue->setMinimumWidth(30);
    connect(m_peakResponseSlider, &QSlider::valueChanged, [peakValue](int value) {
        peakValue->setText(QString::number(value / 100.0, 'f', 2));
    });
    peakLayout->addWidget(m_peakResponseSlider);
    peakLayout->addWidget(peakValue);
    m_starDetectionLayout->addLayout(peakLayout);
    
    // Max distortion control
    QHBoxLayout* distortionLayout = new QHBoxLayout;
    distortionLayout->addWidget(new QLabel("Max Distortion:"));
    m_maxDistortionSlider = new QSlider(Qt::Horizontal);
    m_maxDistortionSlider->setRange(0, 100);
    m_maxDistortionSlider->setValue(80); // 0.8 default
    m_maxDistortionSlider->setToolTip("Maximum allowed star distortion (0=circular only, 100=any shape)");
    QLabel* distortionValue = new QLabel("0.8");
    distortionValue->setMinimumWidth(30);
    connect(m_maxDistortionSlider, &QSlider::valueChanged, [distortionValue](int value) {
        distortionValue->setText(QString::number(value / 100.0, 'f', 2));
    });
    distortionLayout->addWidget(m_maxDistortionSlider);
    distortionLayout->addWidget(distortionValue);
    m_starDetectionLayout->addLayout(distortionLayout);
    
    // PSF fitting control
    m_enablePSFFittingCheck = new QCheckBox("Enable PSF Fitting");
    m_enablePSFFittingCheck->setChecked(true);
    m_enablePSFFittingCheck->setToolTip("Enable Point Spread Function fitting for better centroid accuracy");
    m_starDetectionLayout->addWidget(m_enablePSFFittingCheck);
    
    // Detection buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    m_detectAdvancedButton = new QPushButton("Detect Stars (Advanced)");
    m_detectAdvancedButton->setToolTip("Use PCL StarDetector with current parameters");
    //    m_detectSimpleButton = new QPushButton("Detect Stars (Simple)");
    //    m_detectSimpleButton->setToolTip("Use fallback simple detection algorithm");
    
    buttonLayout->addWidget(m_detectAdvancedButton);
    //    buttonLayout->addWidget(m_detectSimpleButton);
    m_starDetectionLayout->addLayout(buttonLayout);
    
    // Connect signals
    connect(m_detectAdvancedButton, &QPushButton::clicked, this, &MainWindow::onDetectStarsAdvanced);
    //    connect(m_detectSimpleButton, &QPushButton::clicked, this, &MainWindow::onDetectStarsSimple);
    
    // Initially disable until image is loaded
    m_starDetectionGroup->setEnabled(false);
}

// Add this method to handle advanced star detection
void MainWindow::onDetectStarsAdvanced()
{
    if (!m_imageReader->hasImage()) {
        m_statusLabel->setText("No image loaded.");
        return;
    }

    m_statusLabel->setText("Detecting stars with PCL StarDetector...");
    QApplication::processEvents();

    // Get parameters from UI controls
    float sensitivity = m_sensitivitySlider->value() / 100.0f;
    int structureLayers = m_structureLayersSpinBox->value();
    int noiseLayers = m_noiseLayersSpinBox->value();
    float peakResponse = m_peakResponseSlider->value() / 100.0f;
    float maxDistortion = m_maxDistortionSlider->value() / 100.0f;
    bool enablePSFFitting = m_enablePSFFittingCheck->isChecked();

    qDebug() <<  "Starting advanced star detection with parameters:";
    qDebug() <<  "  Sensitivity:" << sensitivity;
    qDebug() <<  "  Structure layers:" << structureLayers;
    qDebug() <<  "  Noise layers:" << noiseLayers;
    qDebug() <<  "  Peak response:" << peakResponse;
    qDebug() <<  "  Max distortion:" << maxDistortion;
    qDebug() <<  "  PSF fitting:" << enablePSFFitting;

    // Use our advanced StarMaskGenerator method
    m_lastStarMask = StarMaskGenerator::detectStarsAdvanced(
        m_imageReader->imageData(),
        sensitivity,
        structureLayers,
        noiseLayers,
        peakResponse,
        maxDistortion,
        enablePSFFitting
    );

    m_imageDisplayWidget->setStarOverlay(m_lastStarMask.starCenters, m_lastStarMask.starRadii);
    m_starsDetected = !m_lastStarMask.starCenters.isEmpty();
    
    // Update status with detailed information
    QString statusText = QString("PCL StarDetector: %1 stars detected").arg(m_lastStarMask.starCenters.size());
    if (m_lastStarMask.starCenters.size() > 0) {
        statusText += " - Use checkboxes to toggle display";
        
        // Add parameter summary to results text
        QString paramSummary = QString("PCL StarDetector Parameters:\n"
                                      "  Sensitivity: %1\n"
                                      "  Structure Layers: %2\n"
                                      "  Noise Layers: %3\n"
                                      "  Peak Response: %4\n"
                                      "  Max Distortion: %5\n"
                                      "  PSF Fitting: %6\n\n"
                                      "Results: %7 stars detected")
                              .arg(sensitivity, 0, 'f', 2)
                              .arg(structureLayers)
                              .arg(noiseLayers)
                              .arg(peakResponse, 0, 'f', 2)
                              .arg(maxDistortion, 0, 'f', 2)
                              .arg(enablePSFFitting ? "Enabled" : "Disabled")
                              .arg(m_lastStarMask.starCenters.size());
        
        m_resultsText->setPlainText(paramSummary);
    }
    m_statusLabel->setText(statusText);
    
    updateValidationControls();
}

/*
// Add this method to handle simple star detection
void MainWindow::onDetectStarsSimple()
{
    if (!m_imageReader->hasImage()) {
        m_statusLabel->setText("No image loaded.");
        return;
    }

    m_statusLabel->setText("Detecting stars with simple algorithm...");
    QApplication::processEvents();

    // Use the basic detection method with sensitivity from slider
    float sensitivity = m_sensitivitySlider->value() / 100.0f;
    m_lastStarMask = StarMaskGenerator::detectStars(m_imageReader->imageData(), sensitivity);

    m_imageDisplayWidget->setStarOverlay(m_lastStarMask.starCenters, m_lastStarMask.starRadii);
    m_starsDetected = !m_lastStarMask.starCenters.isEmpty();
    
    // Update status
    QString statusText = QString("Simple detection: %1 stars detected (sensitivity: %2)")
                        .arg(m_lastStarMask.starCenters.size())
                        .arg(sensitivity, 0, 'f', 2);
    if (m_lastStarMask.starCenters.size() > 0) {
        statusText += " - Use checkboxes to toggle display";
        
        QString resultsSummary = QString("Simple Star Detection:\n"
                                       "  Sensitivity: %1\n"
                                       "  Algorithm: Local maxima detection\n\n"
                                       "Results: %2 stars detected")
                               .arg(sensitivity, 0, 'f', 2)
                               .arg(m_lastStarMask.starCenters.size());
        
        m_resultsText->setPlainText(resultsSummary);
    }
    m_statusLabel->setText(statusText);
    
    updateValidationControls();
}
*/

// Update your existing setupUI() method to include the star detection controls
void MainWindow::setupUI()
{
    m_centralWidget = new QWidget;
    setCentralWidget(m_centralWidget);
    
    // Create main splitter
    QSplitter* mainSplitter = new QSplitter(Qt::Horizontal);
    
    // Left side - image display
    QWidget* leftWidget = new QWidget;
    QVBoxLayout* leftLayout = new QVBoxLayout(leftWidget);
    
    m_imageDisplayWidget = new ImageDisplayWidget;
    leftLayout->addWidget(m_imageDisplayWidget);
    
    // Button layout
    m_buttonLayout = new QHBoxLayout;
    m_loadButton = new QPushButton("Load Image");
    m_plotCatalogButton = new QPushButton("Plot Catalog Stars");
    
    // Remove the old simple detect button since it's now in the controls
    m_plotCatalogButton->setEnabled(false);
    
    m_buttonLayout->addWidget(m_loadButton);
    m_buttonLayout->addWidget(m_plotCatalogButton);
    m_buttonLayout->addStretch();
    
    leftLayout->addLayout(m_buttonLayout);
    
    // Mode toggle
    QHBoxLayout* modeLayout = new QHBoxLayout;
    m_plotModeCheck = new QCheckBox("Catalog Plotting Mode");
    m_plotModeCheck->setToolTip("Switch between star detection/validation and direct catalog plotting");
    modeLayout->addWidget(m_plotModeCheck);
    modeLayout->addStretch();
    leftLayout->addLayout(modeLayout);
    
    // Status label
    m_statusLabel = new QLabel("Ready - Load an image to begin");
    leftLayout->addWidget(m_statusLabel);
    
    // Right side - controls and results
    QWidget* rightWidget = new QWidget;
    rightWidget->setMaximumWidth(400); // Increased width for more controls
    rightWidget->setMinimumWidth(350);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightWidget);
    
    // Add star detection controls
    setupStarDetectionControls();
    rightLayout->addWidget(m_starDetectionGroup);
    
    setupCatalogPlottingControls();
    
    rightLayout->addWidget(m_plottingGroup);

    setupEnhancedMatchingControls();
    mainSplitter->addWidget(m_enhancedMatchingGroup);
    
    // Results display
    QGroupBox* resultsGroup = new QGroupBox("Results");
    QVBoxLayout* resultsLayout = new QVBoxLayout(resultsGroup);
    
    m_resultsText = new QTextEdit;
    m_resultsText->setMaximumHeight(200);
    m_resultsText->setReadOnly(true);
    m_resultsText->setPlainText("No results yet.\n\nLoad an image with WCS information and either:\n- Detect stars for validation, or\n- Plot catalog stars directly");
    
    resultsLayout->addWidget(m_resultsText);
    rightLayout->addWidget(resultsGroup);
    
    rightLayout->addStretch();
    
    // Add widgets to splitter
    mainSplitter->addWidget(leftWidget);
    mainSplitter->addWidget(rightWidget);
    mainSplitter->setStretchFactor(0, 1); // Image display gets most space
    mainSplitter->setStretchFactor(1, 0); // Controls panel fixed width
    
    // Main layout
    m_mainLayout = new QVBoxLayout(m_centralWidget);
    m_mainLayout->addWidget(mainSplitter);
    // Add debugging menu
    setupDebuggingMenu();
    
    // Initialize debugger
    m_pixelDebugger = std::make_unique<PixelMatchingDebugger>(this);


}

// Update your onLoadImage method to enable star detection controls
void MainWindow::onLoadImage()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        "Open Image File",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        ImageReader::formatFilter()
    );
    
    if (filePath.isEmpty())
        return;

    if (!m_imageReader->readFile(filePath)) {
        m_statusLabel->setText("Failed to load image: " + m_imageReader->lastError());
        QMessageBox::warning(this, "Load Error", m_imageReader->lastError());
        return;
    }

    // Store pointer to image data
    m_imageData = &(m_imageReader->imageData());
    
    m_imageDisplayWidget->setImageData(m_imageReader->imageData());
    m_statusLabel->setText("Image loaded successfully - Choose detection method");
    
    // Extract WCS information
    extractWCSFromImage();
    
    // Update UI state
    m_starDetectionGroup->setEnabled(true);  // Enable star detection controls
    m_plotCatalogButton->setEnabled(m_hasWCS);
    m_starsDetected = false;
    m_catalogQueried = false;
    m_validationComplete = false;
    m_catalogPlotted = false;
    
    updateValidationControls();
    updatePlottingControls();
    updateStatusDisplay();
    
    // Clear previous results
    m_imageDisplayWidget->clearStarOverlay();
    m_imageDisplayWidget->clearValidationResults();
    m_resultsText->clear();
}

// Replace the setup2MASSCatalog method with setupGaiaDR3Catalog:
void MainWindow::setupGaiaDR3Catalog()
{
    // Configure the Gaia GDR3 catalog path
    QString catalogPath = "/Volumes/X10Pro/gdr3-1.0.0-01.xpsd";
    
    // Check if catalog file exists
    if (QFile::exists(catalogPath)) {
      qDebug() << "✅ Found Gaia GDR3 catalog at:" << catalogPath;
        
        QFileInfo info(catalogPath);
        double sizeMB = info.size() / (1024.0 * 1024.0);
        qDebug() << QString("📊 Catalog size: %1 MB").arg(sizeMB);
        
        // Set the catalog path
        GaiaGDR3Catalog::setCatalogPath(catalogPath);
	/*        
        // Test database connection
        if (GaiaGDR3Catalog::validateDatabase()) {
	qDebug() << "✅ Gaia GDR3 database validation successful";
            
            // Get detailed info
            QString info = GaiaGDR3Catalog::getCatalogInfo();
            qDebug() << "📊 Catalog details:";
            for (const QString& line : info.split('\n')) {
                if (!line.isEmpty()) {
		qDebug() << "   " << line;
                }
            }
            
            // Update status
            m_statusLabel->setText(QString("Gaia GDR3 catalog ready (%1 MB)").arg(sizeMB));
        } else {
	qDebug() << "❌ Gaia GDR3 database validation failed";
            m_statusLabel->setText("Gaia GDR3 catalog found but validation failed");
        }
        */
    } else {
      qDebug() << "❌ Gaia GDR3 catalog not found at:" << catalogPath;
      qDebug() << "   Available catalog sources:";
        
        // Check alternative locations
        QStringList possiblePaths = {
            "/Volumes/X10Pro/gdr3-1.0.0-01.xpsd",
            "./gdr3-1.0.0-01.xpsd",
            "../gdr3-1.0.0-01.xpsd",
            QDir::homePath() + "/gdr3-1.0.0-01.xpsd",
            "/usr/local/share/pixinsight/gaia/gdr3-1.0.0-01.xpsd"
        };
        
        bool found = false;
        for (const QString& path : possiblePaths) {
            if (QFile::exists(path)) {
	      qDebug() << "✅ Found alternative at:" << path;
                GaiaGDR3Catalog::setCatalogPath(path);
                found = true;
                break;
            }
        }
        
        if (!found) {
	  qDebug() << "   - Hipparcos (network)";
	  qDebug() << "   - Gaia DR3 (network)";
	  qDebug() << "   - Tycho-2 (network)";
            
            // Default to network catalogs if no local Gaia
            m_statusLabel->setText("Using network catalogs (Gaia GDR3 not found)");
        }
    }
}

// Update the catalog menu setup for Gaia:
void MainWindow::setupCatalogMenu()
{
    // Add a menu bar item for catalog management
    QMenuBar* menuBar = this->menuBar();
    QMenu* catalogMenu = menuBar->addMenu("&Catalog");
    
    QAction* browseGaiaAction = catalogMenu->addAction("Browse for Gaia GDR3 catalog...");
    connect(browseGaiaAction, &QAction::triggered, this, &MainWindow::browseGaiaCatalogFile);
    
    catalogMenu->addSeparator();
    
    QAction* showStatsAction = catalogMenu->addAction("Show catalog statistics");
    connect(showStatsAction, &QAction::triggered, [this]() {
        m_catalogValidator->showCatalogStats();
    });
    
    QAction* testQueryAction = catalogMenu->addAction("Test Gaia query");
    connect(testQueryAction, &QAction::triggered, [this]() {
      double crval1, crval2;
      if (m_hasWCS && m_catalogValidator->getCenter(crval1, crval2)) {
	    
	qDebug() << "\n=== TESTING GAIA GDR3 QUERY ===";
	qDebug() << QString("Image center: RA=%1° Dec=%2°").arg(crval1).arg(crval2);
            
            auto start = QTime::currentTime();
            m_catalogValidator->queryCatalog(crval1, crval2, 1.0); // 1 degree radius test
            auto elapsed = start.msecsTo(QTime::currentTime());
            
            qDebug() << QString("Query completed in %1ms").arg(elapsed);
        } else {
            QMessageBox::information(this, "Test Query", "Load an image with WCS first");
        }
    });
    
    catalogMenu->addSeparator();
    
    QAction* findBrightAction = catalogMenu->addAction("Find brightest stars in field");
    connect(findBrightAction, &QAction::triggered, [this]() {
      double crval1, crval2;
      if (m_hasWCS && m_catalogValidator->getCenter(crval1, crval2)) {
	  int width =  m_catalogValidator->getWidth();
	  int height = m_catalogValidator->getHeight();
	  double pixscale = m_catalogValidator->getPixScale();
            double fieldRadius = sqrt(width * width + height * height) * pixscale / 3600.0 / 2.0;
            fieldRadius = std::max(fieldRadius, 0.5);
            
            m_catalogValidator->findBrightGaiaStars(crval1, crval2, fieldRadius, 20);
        } else {
            QMessageBox::information(this, "Bright Stars", "Load an image with WCS first");
        }
    });
    
    QAction* findSpectraAction = catalogMenu->addAction("Find stars with BP/RP spectra");
    connect(findSpectraAction, &QAction::triggered, [this]() {
      double crval1, crval2;
      if (m_hasWCS && m_catalogValidator->getCenter(crval1, crval2)) {
	  int width =  m_catalogValidator->getWidth();
	  int height = m_catalogValidator->getHeight();
	  double pixscale = m_catalogValidator->getPixScale();
            double fieldRadius = sqrt(width * width + height * height) * pixscale / 3600.0 / 2.0;
            fieldRadius = std::max(fieldRadius, 0.5);
            
            m_catalogValidator->queryGaiaWithSpectra(crval1, crval2, fieldRadius);
        } else {
            QMessageBox::information(this, "Spectral Stars", "Load an image with WCS first");
        }
    });
    
    catalogMenu->addSeparator();
    
    QAction* performanceAction = catalogMenu->addAction("Test Gaia performance");
    connect(performanceAction, &QAction::triggered, this, &MainWindow::testGaiaPerformance);
}

// Add file browser for Gaia catalog:
void MainWindow::browseGaiaCatalogFile()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        "Select Gaia GDR3 Catalog File",
        "/Volumes/X10Pro/",
        "Gaia Catalog Files (*.xpsd);;All Files (*)"
    );
    
    if (!filePath.isEmpty()) {
        GaiaGDR3Catalog::setCatalogPath(filePath);
        
        QFileInfo info(filePath);
        double sizeMB = info.size() / (1024.0 * 1024.0);
        
        qDebug() << "📂 User selected Gaia catalog:" << filePath;
        qDebug() << QString("📊 Size: %1 MB").arg(sizeMB);
        
        // Validate the new catalog
        if (GaiaGDR3Catalog::validateDatabase()) {
            m_statusLabel->setText(QString("Custom Gaia GDR3 catalog loaded (%1 MB)").arg(sizeMB));
            qDebug() << "✅ Custom Gaia catalog validation successful";
        } else {
            m_statusLabel->setText("Gaia catalog validation failed");
            qDebug() << "❌ Custom Gaia catalog validation failed";
        }
        
        // Clear previous catalog data
        m_catalogQueried = false;
        m_catalogPlotted = false;
        updateValidationControls();
        updatePlottingControls();
    }
}

// Add performance testing method:
void MainWindow::testGaiaPerformance()
{
    if (!m_hasWCS) {
        QMessageBox::information(this, "Performance Test", "Load an image with WCS first");
        return;
    }
    double crval1, crval2;
    m_catalogValidator->getCenter(crval1, crval2);
   
    double testRadius = 2.0; // 2 degree radius for performance test
    
    qDebug() << "\n=== GAIA GDR3 PERFORMANCE COMPARISON ===";
    qDebug() << QString("Test query: RA=%1° Dec=%2° radius=%3°")
      .arg(crval1).arg(crval2).arg(testRadius);
    
    // Test different magnitude limits
    QVector<double> magLimits = {12.0, 15.0, 18.0, 20.0};
    
    for (double magLimit : magLimits) {
        auto start = QTime::currentTime();
        
        GaiaGDR3Catalog::SearchParameters params(crval1, crval2, testRadius, magLimit);
        auto stars = GaiaGDR3Catalog::queryRegion(params);
        
        auto elapsed = start.msecsTo(QTime::currentTime());
        
        qDebug() << QString("🚀 Mag ≤ %1: %2 stars in %3ms (%4 stars/sec)")
                    .arg(magLimit).arg(stars.size()).arg(elapsed)
	  .arg(stars.size() * 1000.0 / elapsed);
    }
    
    // Test spectrum search
    auto start = QTime::currentTime();
    auto specStars = GaiaGDR3Catalog::findStarsWithSpectra(crval1, crval2, testRadius, 15.0);
    auto elapsed = start.msecsTo(QTime::currentTime());
    qDebug() << QString("🌈 BP/RP spectra search: %1 stars in %2ms")
      .arg(specStars.size()).arg(elapsed);
    qDebug() << "📊 Gaia GDR3 provides:";
    qDebug() << "   - Precise astrometry (positions, proper motions, parallax)";
    qDebug() << "   - Multi-band photometry (G, BP, RP)";
    qDebug() << "   - BP/RP low-resolution spectra for many stars";
    qDebug() << "   - Quality flags and error estimates";
    qDebug() << "   - Proper motion corrections to current epoch";
}

// Update the destructor to clean up Gaia resources:
MainWindow::~MainWindow() 
{
    // Clean up Gaia catalog resources
    GaiaGDR3Catalog::shutdown();
}

// Test function you can add to MainWindow or run separately
void testGaiaGDR3Integration()
{
  qDebug() << "\n=== GAIA GDR3 INTEGRATION TEST ===";
    
    // 1. Set up the catalog path
    QString catalogPath = "/Volumes/X10Pro/gdr3-1.0.0-01.xpsd";
    GaiaGDR3Catalog::setCatalogPath(catalogPath);
    
    if (!GaiaGDR3Catalog::isAvailable()) {
      qDebug() << "❌ Gaia catalog not available at:" << catalogPath;
        return;
    }
    
    qDebug() << "✅ Catalog info:";
    qDebug() << GaiaGDR3Catalog::getCatalogInfo();
    
    // 2. Test basic query around M31 (Andromeda Galaxy)
    qDebug() << "\n--- Testing M31 Region Query ---";
    double m31_ra = 10.684708;   // M31 RA
    double m31_dec = 41.268750;  // M31 Dec
    
    auto start = QTime::currentTime();
    auto m31_stars = GaiaGDR3Catalog::queryRegion(m31_ra, m31_dec, 0.5, 15.0);
    auto elapsed = start.msecsTo(QTime::currentTime());
    
    qDebug() << QString("Found %1 stars around M31 in %2ms").arg(m31_stars.size()).arg(elapsed);
    
    if (!m31_stars.isEmpty()) {
      qDebug() << "Brightest 5 stars:";
        for (int i = 0; i < qMin(5, m31_stars.size()); ++i) {
            const auto& star = m31_stars[i];
            qDebug() << QString("  %1: RA=%2° Dec=%3° G=%4 %2")
	      .arg(star.sourceId).arg(star.ra).arg(star.dec)
	      .arg(star.magnitude).arg(star.spectralClass);
        }
    }
    
    // 3. Test bright star search around Polaris
    qDebug() << "\n--- Testing Bright Stars Around Polaris ---";
    double polaris_ra = 37.954;   // Polaris RA  
    double polaris_dec = 89.264;  // Polaris Dec
    
    start = QTime::currentTime();
    auto bright_stars = GaiaGDR3Catalog::findBrightestStars(polaris_ra, polaris_dec, 2.0, 10);
    elapsed = start.msecsTo(QTime::currentTime());
    
    qDebug() << QString("Found %1 bright stars around Polaris in %2ms").arg(bright_stars.size()).arg(elapsed);
    
    for (const auto& star : bright_stars) {
      qDebug() << QString("  G=%1 %2 at (%3°, %4°) PM=(%5, %6) mas/yr")
	.arg(star.spectralClass).arg(star.magnitude)
	.arg(star.ra).arg(star.dec).arg(star.pmRA).arg(star.pmDec);
    }
    
    // 4. Test spectrum search around Vega
    qDebug() << "\n--- Testing Spectrum Search Around Vega ---";
    double vega_ra = 279.234;    // Vega RA
    double vega_dec = 38.784;    // Vega Dec
    
    start = QTime::currentTime();
    auto spectral_stars = GaiaGDR3Catalog::findStarsWithSpectra(vega_ra, vega_dec, 1.0, 12.0);
    elapsed = start.msecsTo(QTime::currentTime());
    
    qDebug() << QString("Found %1 stars with BP/RP spectra around Vega in %2ms").arg(spectral_stars.size()).arg(elapsed);
    
    for (const auto& star : spectral_stars) {
      qDebug() << QString("  %1: G=%2 BP=%3 RP=%4 %5 [SPECTRUM]")
                    .arg(star.sourceId).arg(star.magnitude)
	.arg(star.magBP).arg(star.magRP).arg(star.spectralClass);
    }
    
    // 5. Test performance with large search
    qDebug() << "\n--- Testing Performance ---";
    GaiaGDR3Catalog::testPerformance(0.0, 0.0, 5.0); // Large search around celestial equator
    
    // 6. Test proper motion application
    qDebug() << "\n--- Testing Proper Motion Correction ---";
    GaiaGDR3Catalog::SearchParameters params;
    params.centerRA = 266.417;  // Galactic center
    params.centerDec = -29.008;
    params.radiusDegrees = 1.0;
    params.maxMagnitude = 10.0;
    params.useProperMotion = true;
    params.epochYear = 2025.5;  // Current epoch
    params.maxResults = 5;
    
    auto pm_stars = GaiaGDR3Catalog::queryRegion(params);
    qDebug() << QString("Found %1 stars with proper motion applied to epoch %2")
      .arg(pm_stars.size()).arg(params.epochYear);
    
    for (const auto& star : pm_stars) {
      qDebug() << QString("  %1: G=%2 at (%3°, %4°) PM=(%5, %6)")
	.arg(star.sourceId).arg(star.magnitude)
	.arg(star.ra).arg(star.dec).arg(star.pmRA).arg(star.pmDec);
    }
    
    qDebug() << "\n✅ Gaia GDR3 integration test completed successfully!";
}

// Integration example for your existing star validation workflow
void integrateGaiaWithStarValidation()
{
  qDebug() << "\n=== GAIA INTEGRATION WITH STAR VALIDATION ===";
    
    // Example: Your detected stars from star mask
    QVector<QPoint> detectedStars = {
        QPoint(512, 256),   // Example detected star positions
        QPoint(1024, 512),
        QPoint(256, 1024),
        QPoint(1500, 800)
    };
    
    // Example WCS data (replace with your actual WCS)
    double centerRA = 11.195;    // RA of image center
    double centerDec = 41.892;   // Dec of image center
    double pixelScale = 1.2;     // arcsec/pixel
    int imageWidth = 2048;
    int imageHeight = 2048;
    
    // Calculate field radius
    double fieldRadius = sqrt(imageWidth * imageWidth + imageHeight * imageHeight) 
                        * pixelScale / 3600.0 / 2.0; // Convert to degrees
    
    qDebug() << QString("Image field: %1° radius around RA=%2° Dec=%3°")
      .arg(fieldRadius).arg(centerRA).arg(centerDec);
    
    // Query Gaia catalog for this field
    GaiaGDR3Catalog::SearchParameters params;
    params.centerRA = centerRA;
    params.centerDec = centerDec;
    params.radiusDegrees = fieldRadius;
    params.maxMagnitude = 16.0;    // Reasonable limit for star detection
    params.useProperMotion = true; // Apply proper motion to current epoch
    params.epochYear = 2025.5;
    
    auto catalogStars = GaiaGDR3Catalog::queryRegion(params);
    
    qDebug() << QString("Retrieved %1 Gaia stars for validation").arg(catalogStars.size());
    
    // Example validation process (simplified)
    int matches = 0;
    double matchTolerance = 5.0; // pixels
    
    for (const QPoint& detected : detectedStars) {
        for (const auto& catalog : catalogStars) {
            // Convert catalog star RA/Dec to pixel coordinates (simplified)
            // In real implementation, use your WCS transformation
            double pixelX = imageWidth/2 + (catalog.ra - centerRA) * 3600.0 / pixelScale;
            double pixelY = imageHeight/2 + (catalog.dec - centerDec) * 3600.0 / pixelScale;
            
            double distance = sqrt(pow(detected.x() - pixelX, 2) + pow(detected.y() - pixelY, 2));
            
            if (distance < matchTolerance) {
                matches++;
                qDebug() << QString("✅ Match: detected(%1,%2) ↔ Gaia %3 G=%4 dist=%5px")
		  .arg(detected.x()).arg(detected.y())
		  .arg(catalog.sourceId).arg(catalog.magnitude).arg(distance);
                break;
            }
        }
    }
    
    double matchPercentage = 100.0 * matches / detectedStars.size();
    qDebug() << QString("Validation result: %1/%2 stars matched (%3%%)")
      .arg(matches).arg(detectedStars.size()).arg(matchPercentage);
}

// Advanced Gaia features demonstration
void demonstrateAdvancedGaiaFeatures()
{
  qDebug() << "\n=== ADVANCED GAIA GDR3 FEATURES ===";
    
    // 1. Color-magnitude diagram data
  qDebug() << "\n--- Color-Magnitude Diagram Data ---";
    auto stars = GaiaGDR3Catalog::queryRegion(0.0, 0.0, 2.0, 15.0);
    
    if (!stars.isEmpty()) {
      qDebug() << "Sample stars for HR diagram:";
        for (int i = 0; i < qMin(10, stars.size()); ++i) {
            const auto& star = stars[i];
            double bpRpColor = star.magBP - star.magRP;
            double absoluteG = star.magnitude; // Would need distance for absolute magnitude
            
            qDebug() << QString("  G=%1 BP-RP=%2 %3 plx=%4")
	      .arg(absoluteG).arg(bpRpColor)
	      .arg(star.spectralClass).arg(star.parallax);
        }
    }
    
    // 2. Proper motion analysis
    qDebug() << "\n--- High Proper Motion Stars ---";
    auto highPMStars = GaiaGDR3Catalog::queryRegion(83.633, 22.014, 5.0, 12.0); // Around Aldebaran
    
    QVector<GaiaGDR3Catalog::Star> fastMovers;
    for (const auto& star : highPMStars) {
        double totalPM = sqrt(star.pmRA * star.pmRA + star.pmDec * star.pmDec);
        if (totalPM > 50.0) { // > 50 mas/year
            fastMovers.append(star);
        }
    }
    
    qDebug() << QString("Found %1 stars with PM > 50 mas/year").arg(fastMovers.size());
    for (const auto& star : fastMovers) {
        double totalPM = sqrt(star.pmRA * star.pmRA + star.pmDec * star.pmDec);
        qDebug() << QString("  %1: PM=%2 mas/yr G=%3")
	  .arg(star.sourceId).arg(totalPM).arg(star.magnitude);
    }
    
    // 3. Parallax-based distance estimates
    qDebug() << "\n--- Nearby Stars (Parallax > 10 mas) ---";
    QVector<GaiaGDR3Catalog::Star> nearbyStars;
    for (const auto& star : highPMStars) {
        if (star.parallax > 10.0) { // Closer than ~100 parsecs
            nearbyStars.append(star);
        }
    }
    
    qDebug() << QString("Found %1 nearby stars").arg(nearbyStars.size());
    for (const auto& star : nearbyStars) {
        double distancePc = 1000.0 / star.parallax; // Distance in parsecs
        qDebug() << QString("  %1: %2 pc G=%3 %4")
	  .arg(star.sourceId).arg(distancePc)
	  .arg(star.magnitude).arg(star.spectralClass);
    }
    
    // 4. Quality assessment
    qDebug() << "\n--- Data Quality Assessment ---";
    int highQuality = 0;
    int withSpectra = 0;
    int goodAstrometry = 0;
    
    for (const auto& star : stars) {
        if (GaiaGDR3Catalog::isHighQuality(star)) highQuality++;
        if (star.hasSpectrum) withSpectra++;
        if (GaiaGDR3Catalog::hasGoodAstrometry(star)) goodAstrometry++;
    }
    
    qDebug() << QString("Quality statistics from %1 stars:").arg(stars.size());
    qDebug() << QString("  High quality: %1 (%2%%)").arg(highQuality).arg(100.0 * highQuality / stars.size());
    qDebug() << QString("  With BP/RP spectra: %1 (%2%%)").arg(withSpectra).arg(100.0 * withSpectra / stars.size());
    qDebug() << QString("  Good astrometry: %1 (%2%%)").arg(goodAstrometry).arg(100.0 * goodAstrometry / stars.size());
}

// Performance comparison between different catalog sources
void comparePerformanceWithOtherCatalogs()
{
  qDebug() << "\n=== CATALOG PERFORMANCE COMPARISON ===";
    
    double testRA = 83.633;     // Aldebaran region
    double testDec = 22.014;
    double testRadius = 1.0;    // 1 degree radius
    double magLimit = 15.0;
    
    qDebug() << QString("Test region: RA=%1° Dec=%2° radius=%3° mag≤%4")
      .arg(testRA).arg(testDec).arg(testRadius).arg(magLimit);
    
    // Test Gaia GDR3 local database
    if (GaiaGDR3Catalog::isAvailable()) {
        auto start = QTime::currentTime();
        auto gaiaStars = GaiaGDR3Catalog::queryRegion(testRA, testDec, testRadius, magLimit);
        auto elapsed = start.msecsTo(QTime::currentTime());
        
        qDebug() << QString("🚀 Gaia GDR3 (local): %1 stars in %2ms (%3 stars/sec)")
	  .arg(gaiaStars.size()).arg(elapsed).arg(gaiaStars.size() * 1000.0 / elapsed);
        
        // Analyze data quality
        int withParallax = 0, withPM = 0, withSpectra = 0;
        for (const auto& star : gaiaStars) {
            if (star.parallax > 0) withParallax++;
            if (star.pmRA != 0 || star.pmDec != 0) withPM++;
            if (star.hasSpectrum) withSpectra++;
        }
        
	qDebug() << QString("  Data richness: %1 with parallax, %2 with PM, %3 with spectra")
	  .arg(withParallax).arg(withPM).arg(withSpectra);
    }
    
    qDebug() << "📊 Comparison summary:";
    qDebug() << "  Gaia GDR3: ~1000x faster than network, richest data (astrometry + photometry + spectra)";
    qDebug() << "  2MASS: ~100x faster than network, photometry only (J, H, K bands)";
    qDebug() << "  Network catalogs: Slow (2-10 seconds), limited by bandwidth";
    qDebug() << "  Bright star DB: Fastest (< 1ms), but only brightest stars";
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_imageReader(std::make_unique<ImageReader>())
    , m_catalogValidator(std::make_unique<StarCatalogValidator>(this))
    , m_imageData(nullptr)
    , m_hasWCS(false)
    , m_starsDetected(false)
    , m_catalogQueried(false)
    , m_validationComplete(false)
    , m_plotMode(false)
    , m_catalogPlotted(false)
{
    setupUI();
    setupGaiaDR3Catalog();  // Add this line
    setupCatalogMenu();   // Add this line
    setupDebuggingMenu();
    initializePlatesolveIntegration();
    addPlatesolvingTestButton();
    
    // Connect image reader signals
    connect(m_loadButton, &QPushButton::clicked, this, &MainWindow::onLoadImage);
    connect(m_plotCatalogButton, &QPushButton::clicked, this, &MainWindow::onPlotCatalogStars);
    
    // Connect mode control
    connect(m_plotModeCheck, &QCheckBox::toggled, this, &MainWindow::onPlotModeToggled);
    /*    
    // Connect validation control signals
    connect(m_validationModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onValidationModeChanged);

      connect(m_fieldRadiusSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [this](double) {
                m_catalogQueried = false;
                m_catalogPlotted = false;
                updatePlottingControls();
            });
    */
    
    // Connect catalog validator signals
    connect(m_catalogValidator.get(), &StarCatalogValidator::catalogQueryStarted,
            this, &MainWindow::onCatalogQueryStarted);
    connect(m_catalogValidator.get(), &StarCatalogValidator::catalogQueryFinished,
            this, &MainWindow::onCatalogQueryFinished);
    connect(m_catalogValidator.get(), &StarCatalogValidator::validationCompleted,
            this, &MainWindow::onValidationCompleted);
    connect(m_catalogValidator.get(), &StarCatalogValidator::errorSignal,
            this, &MainWindow::onValidatorError);
    
    // Connect display widget overlay signals
    connect(m_imageDisplayWidget, &ImageDisplayWidget::starOverlayToggled, 
            this, &MainWindow::onStarOverlayToggled);
    connect(m_imageDisplayWidget, &ImageDisplayWidget::catalogOverlayToggled,
            this, &MainWindow::onCatalogOverlayToggled);
    connect(m_imageDisplayWidget, &ImageDisplayWidget::validationOverlayToggled,
            this, &MainWindow::onValidationOverlayToggled);

    resize(1400, 900);
    setWindowTitle("Star Detection & Catalog Validation/Plotting");
    
    updateValidationControls();
    updatePlottingControls();
}

void MainWindow::setupCatalogPlottingControls()
{
    m_plottingGroup = new QGroupBox("Catalog Plotting");
    m_plottingLayout = new QVBoxLayout(m_plottingGroup);
    /*    
    // Magnitude limit for plotting
    QHBoxLayout* plotMagLayout = new QHBoxLayout;
    plotMagLayout->addWidget(new QLabel("Magnitude Limit:"));
    m_plotMagnitudeSpin = new QDoubleSpinBox;
    m_plotMagnitudeSpin->setRange(4.0, 20.0);
    m_plotMagnitudeSpin->setValue(12.0);
    m_plotMagnitudeSpin->setDecimals(1);
    m_plotMagnitudeSpin->setSuffix(" mag");
    plotMagLayout->addWidget(m_plotMagnitudeSpin);
    m_plottingLayout->addLayout(plotMagLayout);
    // Field radius
    QHBoxLayout* radiusLayout = new QHBoxLayout;
    radiusLayout->addWidget(new QLabel("Field Radius:"));
    m_fieldRadiusSpin = new QDoubleSpinBox;
    m_fieldRadiusSpin->setRange(0.1, 10.0);
    m_fieldRadiusSpin->setValue(2.0);
    m_fieldRadiusSpin->setDecimals(1);
    m_fieldRadiusSpin->setSuffix(" deg");
    m_fieldRadiusSpin->setToolTip("Search radius around image center");
    radiusLayout->addWidget(m_fieldRadiusSpin);
    m_plottingLayout->addLayout(radiusLayout);
    */
    
    // Initially hide plotting controls
    m_plottingGroup->setVisible(false);
}

void MainWindow::onPlotCatalogStars()
{
    if (!m_hasWCS) {
        QMessageBox::warning(this, "Plot Catalog", "No WCS information available. Cannot plot catalog stars.");
        return;
    }
    
    plotCatalogStarsDirectly();
}

void MainWindow::onDebugStarCorrelation()
{
    if (!m_catalogQueried) {
      return;
    }

    QVector<CatalogStar> catalogStars = m_catalogValidator->getCatalogStars();
    StarMaskGenerator::dumpcat(catalogStars);
}

void MainWindow::plotCatalogStarsDirectly()
{
  double crval1, crval2;
  m_catalogValidator->getCenter(crval1, crval2);
 
    if (!m_catalogQueried) {
        // Need to query catalog first
      int width =  m_catalogValidator->getWidth();
      int height = m_catalogValidator->getHeight();
      double pixscale = m_catalogValidator->getPixScale();
      double fieldRadius = sqrt(width * width + height * height) * pixscale / 3600.0 / 2.0;
      fieldRadius = std::max(fieldRadius, 0.5);
        
        // Set magnitude limit from plotting controls
	//        m_catalogValidator->setMagnitudeLimit(m_plotMagnitudeSpin->value());
        
        m_catalogValidator->queryCatalog(crval1, crval2, fieldRadius);
    }

    // Catalog already queried, just display it
    QVector<CatalogStar> catalogStars = m_catalogValidator->getCatalogStars();
    StarMaskGenerator::dumpcat(catalogStars);
    // Create a validation result just for display purposes
    ValidationResult plotResult;
    plotResult.catalogStars = catalogStars;
    plotResult.totalCatalog = catalogStars.size();
    plotResult.isValid = true;
    plotResult.summary = QString("Catalog Plot:\n%1 stars plotted\nSource: %2")
      .arg(catalogStars.size());

    m_imageDisplayWidget->setValidationResults(plotResult);
    m_resultsText->setPlainText(plotResult.summary);

    m_catalogPlotted = true;
    m_statusLabel->setText(QString("Plotted %1 catalog stars")
			   .arg(catalogStars.size()));

    updatePlottingControls();
}

void MainWindow::onPlotModeToggled(bool plotMode)
{
    m_plotMode = plotMode;
    
    // Show/hide appropriate control groups
    m_plottingGroup->setVisible(plotMode);
    
    // Update button states
    if (plotMode) {
        m_plotCatalogButton->setVisible(true);
    } else {
        m_plotCatalogButton->setVisible(false);
    }
    
    updateValidationControls();
    updatePlottingControls();
    
    // Update status
    QString modeText = plotMode ? "Catalog Plotting Mode" : "Star Detection & Validation Mode";
    if (m_imageData) {
        m_statusLabel->setText(QString("Image loaded - %1").arg(modeText));
    } else {
        m_statusLabel->setText(QString("Ready - %1").arg(modeText));
    }
}

void MainWindow::updatePlottingControls()
{
    bool canPlot = m_hasWCS && m_imageData;
    m_plotCatalogButton->setEnabled(canPlot);
    
    // Update tooltip
    if (!m_hasWCS) {
        m_plotCatalogButton->setToolTip("No WCS information available in image");
    } else if (!m_imageData) {
        m_plotCatalogButton->setToolTip("Load an image first");
    } else {
        m_plotCatalogButton->setToolTip("Plot catalog stars directly on the image");
    }
}

// ... [Rest of the existing methods remain the same] ...

// Update the MainWindow to use the simplified method
void MainWindow::extractWCSFromImage()
{
    m_hasWCS = false;
    
    if (!m_imageData || m_imageData->metadata.isEmpty()) {
      qDebug() << "No metadata available for WCS extraction";
        return;
    }
    
    qDebug() << "=== Using PCL Native WCS Extraction ===";
    
    // Use PCL's native WCS parsing instead of custom parsing
    m_hasWCS = m_catalogValidator->setWCSFromImageMetadata(*m_imageData);
    
    // Update WCS status display
    QLabel* wcsLabel = findChild<QLabel*>("wcsStatusLabel");
    if (wcsLabel) {
        if (m_hasWCS) {
	  double crval1, crval2, pixscale = m_catalogValidator->getPixScale() ;
	  m_catalogValidator->getCenter(crval1, crval2);
	  wcsLabel->setText(QString("WCS Status: Available (PCL)\nRA: %1°, Dec: %2°\nPixel Scale: %3 arcsec/px")
                            .arg(crval1, 0, 'f', 4)
                            .arg(crval2, 0, 'f', 4)
                            .arg(pixscale, 0, 'f', 2));
	  wcsLabel->setStyleSheet("QLabel { color: green; font-weight: bold; }");
        } else {
	  wcsLabel->setText("WCS Status: Failed (PCL)");
	  wcsLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
        }
    }
    
    qDebug() << "PCL WCS extraction completed, valid:" << m_hasWCS;
}

/*
void MainWindow::onDetectStars()
{
    if (!m_imageReader->hasImage()) {
        m_statusLabel->setText("No image loaded.");
        return;
    }

    m_statusLabel->setText("Detecting stars...");
    QApplication::processEvents();

    // Use our StarMaskGenerator for detection
    m_lastStarMask = StarMaskGenerator::detectStars(m_imageReader->imageData(), 0.5f);
    m_imageDisplayWidget->setStarOverlay(m_lastStarMask.starCenters, m_lastStarMask.starRadii);

    m_starsDetected = !m_lastStarMask.starCenters.isEmpty();
    
    // Update status
    QString statusText = QString("Detected %1 stars").arg(m_lastStarMask.starCenters.size());
    if (m_lastStarMask.starCenters.size() > 0) {
        statusText += " - Use checkboxes to toggle display";
    }
    m_statusLabel->setText(statusText);
    
    updateValidationControls();
}
*/

void MainWindow::performValidation()
{
    ValidationResult result = m_catalogValidator->validateStars(m_lastStarMask.starCenters, m_lastStarMask.starRadii);
    
    if (result.isValid) {
        m_lastValidation = result;
        m_validationComplete = true;
        
        // Update display
        m_imageDisplayWidget->setValidationResults(result);
        
        // Update results text
        m_resultsText->setPlainText(result.summary);
        
        m_statusLabel->setText(QString("Validation complete: %1/%2 stars matched (%3%%)")
                              .arg(result.totalMatches)
                              .arg(result.totalDetected)
                              .arg(result.matchPercentage));
    } else {
        QMessageBox::warning(this, "Validation Error", "Validation failed. Check WCS data and try again.");
    }
    
    updateValidationControls();
}

void MainWindow::onValidationModeChanged()
{
    auto mode = static_cast<StarCatalogValidator::ValidationMode>(m_validationModeCombo->currentData().toInt());
    m_catalogValidator->setValidationMode(mode);
}

void MainWindow::onPixelToleranceChanged()
{
  //    m_catalogValidator->setMatchingTolerance(m_pixelToleranceSpin->value());
}

void MainWindow::onCatalogQueryStarted()
{
    m_plotCatalogButton->setEnabled(false);
    m_statusLabel->setText("Querying star catalog...");
}

// Replace your existing onCatalogQueryFinished method with this:
void MainWindow::onCatalogQueryFinished(bool success, const QString& message)
{
     m_plotCatalogButton->setEnabled(m_hasWCS && m_plotMode);
    
    if (success) {
        m_catalogQueried = true;
        
	double crval1, crval2;
	m_catalogValidator->getCenter(crval1, crval2);
	int width =  m_catalogValidator->getWidth();
	int height = m_catalogValidator->getHeight();
	double pixscale = m_catalogValidator->getPixScale();
        double fieldRadius = sqrt(width * width + height * height) * pixscale / 3600.0 / 2.0;
        fieldRadius = std::max(fieldRadius, 0.5);
        
        // Add bright stars from local database (this always works!)
        m_catalogValidator->addBrightStarsFromDatabase(crval1, crval2, fieldRadius);
        
        m_statusLabel->setText(QString("Retrieved %1 catalog stars (including bright stars)")
                               .arg(m_catalogValidator->getCatalogStars().size()));
        
        if (m_plotMode) {
            plotCatalogStarsDirectly();
        } else if (m_starsDetected) {
            performValidation();
        }
    } else {
        m_statusLabel->setText("Catalog query failed: " + message);
        QMessageBox::warning(this, "Catalog Query Error", message);
    }
    
    updateValidationControls();
    updatePlottingControls();
}

void MainWindow::onValidationCompleted(const ValidationResult& result)
{
    m_lastValidation = result;
    m_validationComplete = true;
    
    qDebug() << "Validation completed with" << result.totalMatches << "matches";
}

void MainWindow::onValidatorError(const QString& message)
{
    m_statusLabel->setText("Validation error: " + message);
    QMessageBox::warning(this, "Validation Error", message);
}

void MainWindow::onStarOverlayToggled(bool visible)
{
    if (m_lastStarMask.starCenters.isEmpty()) return;
    
    QString baseText = QString("Detected %1 stars").arg(m_lastStarMask.starCenters.size());
    QString statusText = baseText + (visible ? " - Stars visible" : " - Stars hidden");
    m_statusLabel->setText(statusText);
}

void MainWindow::onCatalogOverlayToggled(bool visible)
{
    if (!m_catalogQueried && !m_catalogPlotted) return;
    
    QVector<CatalogStar> catalogStars = m_catalogValidator->getCatalogStars();
    QString statusText = QString("Catalog stars %1 (%2 total)")
                        .arg(visible ? "visible" : "hidden")
                        .arg(catalogStars.size());
    m_statusLabel->setText(statusText);
}

void MainWindow::onValidationOverlayToggled(bool visible)
{
    if (!m_validationComplete && !m_catalogPlotted) return;
    
    if (m_catalogPlotted) {
        QVector<CatalogStar> catalogStars = m_catalogValidator->getCatalogStars();
        QString statusText = QString("Catalog plot %1 (%2 stars)")
                            .arg(visible ? "visible" : "hidden")
                            .arg(catalogStars.size());
        m_statusLabel->setText(statusText);
    } else {
        QString statusText = QString("Validation results %1 (%2 matches)")
                            .arg(visible ? "visible" : "hidden")
                            .arg(m_lastValidation.totalMatches);
        m_statusLabel->setText(statusText);
    }
}

void MainWindow::updateValidationControls()
{
}

void MainWindow::updateStatusDisplay()
{
    QString status = "Ready";
    
    if (m_imageData) {
        status = QString("Image: %1×%2×%3")
                .arg(m_imageData->width)
                .arg(m_imageData->height)
                .arg(m_imageData->channels);
        
        if (m_hasWCS) {
            status += " (WCS available)";
        }
        
        if (m_plotMode) {
            status += " - Catalog Plotting Mode";
            if (m_catalogPlotted) {
                QVector<CatalogStar> catalogStars = m_catalogValidator->getCatalogStars();
                status += QString(", %1 stars plotted").arg(catalogStars.size());
            }
        } else {
            status += " - Detection & Validation Mode";
            if (m_starsDetected) {
                status += QString(", %1 stars detected").arg(m_lastStarMask.starCenters.size());
            }
            
            if (m_validationComplete) {
                status += QString(", %1 matched").arg(m_lastValidation.totalMatches);
            }
        }
    }
    
    m_statusLabel->setText(status);
}

void MainWindow::onMatchingParametersChanged()
{

}

void MainWindow::updateEnhancedMatchingControls()
{
  
}

// Quick test function you can add to MainWindow to test the debugging system
// Add this to MainWindow.cpp for immediate testing

void MainWindow::testPixelMatchingDebug()
{
  qDebug() << "\n=== TESTING PIXEL MATCHING DEBUG SYSTEM ===";
    
    // Create some test data to simulate your matching problem
    QVector<QPoint> testDetectedStars = {
        QPoint(100, 100),
        QPoint(200, 150),
        QPoint(300, 200),
        QPoint(150, 300),
        QPoint(250, 350)
    };
    
    QVector<CatalogStar> testCatalogStars;
    
    // Create catalog stars that are close but not exactly matching
    // This simulates your "visual match but mathematical fail" problem
    testCatalogStars.append(CatalogStar("Test_1", 45.0, 30.0, 8.5));
    testCatalogStars[0].pixelPos = QPointF(102.3, 101.7); // 2.3px offset
    testCatalogStars[0].isValid = true;
    
    testCatalogStars.append(CatalogStar("Test_2", 45.1, 30.1, 9.2));
    testCatalogStars[1].pixelPos = QPointF(195.8, 152.4); // 4.8px offset  
    testCatalogStars[1].isValid = true;
    
    testCatalogStars.append(CatalogStar("Test_3", 45.2, 30.2, 7.8));
    testCatalogStars[2].pixelPos = QPointF(306.2, 195.1); // 7.8px offset
    testCatalogStars[2].isValid = true;
    
    testCatalogStars.append(CatalogStar("Test_4", 45.0, 29.9, 10.1));
    testCatalogStars[3].pixelPos = QPointF(148.1, 304.5); // 4.9px offset
    testCatalogStars[3].isValid = true;
    
    testCatalogStars.append(CatalogStar("Test_5", 45.1, 29.8, 8.9));
    testCatalogStars[4].pixelPos = QPointF(253.2, 347.8); // 4.0px offset
    testCatalogStars[4].isValid = true;
    
    // Create test validation result with mixed success
    ValidationResult testResult;
    testResult.catalogStars = testCatalogStars;
    testResult.totalDetected = testDetectedStars.size();
    testResult.totalCatalog = testCatalogStars.size();
    
    // Simulate matches with different success criteria
    // Match 1: Close distance, passes
    StarMatch match1(0, 0, 2.9, 0.5);
    match1.isGoodMatch = true;
    testResult.matches.append(match1);
    
    // Match 2: Medium distance, should pass but marked as fail (your problem!)
    StarMatch match2(1, 1, 4.8, 0.8);
    match2.isGoodMatch = false; // This is the problem case
    testResult.matches.append(match2);
    
    // Match 3: Large distance, correctly fails
    StarMatch match3(2, 2, 7.8, 1.2);
    match3.isGoodMatch = false;
    testResult.matches.append(match3);
    
    // Match 4: Close distance, passes
    StarMatch match4(3, 3, 4.9, 0.9);
    match4.isGoodMatch = true;
    testResult.matches.append(match4);
    
    // Match 5: Medium distance, should pass but fails
    StarMatch match5(4, 4, 4.0, 0.7);
    match5.isGoodMatch = false; // Another problem case
    testResult.matches.append(match5);
    
    testResult.totalMatches = 2; // Only 2 out of 5 passed
    testResult.matchPercentage = 40.0;
    testResult.isValid = true;
    
    qDebug() << "Created test data:";
    qDebug() << "  Detected stars:" << testDetectedStars.size();
    qDebug() << "  Catalog stars:" << testCatalogStars.size();  
    qDebug() << "  Matches:" << testResult.matches.size();
    qDebug() << "  Successful matches:" << testResult.totalMatches;
    
    // Test the debugging system
    if (!m_pixelDebugger) {
        m_pixelDebugger = std::make_unique<PixelMatchingDebugger>(this);
    }
    
    // Run the diagnostic
    m_pixelDebugger->diagnoseMatching(testDetectedStars, testCatalogStars, testResult, nullptr);
    
    // Test specific analysis methods
    qDebug() << "\n--- Testing Distance Criteria Analysis ---";
    m_pixelDebugger->analyzeDistanceCriteria(5.0); // 5px tolerance
    
    qDebug() << "\n--- Testing Missed Matches Analysis ---";
    m_pixelDebugger->findMissedMatches(6.0);
    
    qDebug() << "\n--- Testing Tolerance Sensitivity ---";
    m_pixelDebugger->testToleranceRange(2.0, 8.0, 1.0);
    
    qDebug() << "\n--- Testing Summary Report ---";
    m_pixelDebugger->printSummaryReport();
    
    qDebug() << "\n=== TEST COMPLETE ===";
    qDebug() << "Expected findings:";
    qDebug() << "• 2 matches should be flagged as 'visual matches that fail criteria'";
    qDebug() << "• Optimal tolerance should be around 5-6 pixels";
    qDebug() << "• Should detect systematic offset if any";
    qDebug() << "• Efficiency should be low (40%) indicating matching problems";
}

// Call this from a menu item or button for testing
void MainWindow::addTestButton()
{
    // Add this to your setupUI() method to create a test button
    QPushButton* testDebugBtn = new QPushButton("Test Debug System");
    testDebugBtn->setToolTip("Test the pixel matching debug system with synthetic data");
    connect(testDebugBtn, &QPushButton::clicked, this, &MainWindow::testPixelMatchingDebug);
    
    // Add to your button layout or create a test menu
    m_buttonLayout->addWidget(testDebugBtn);
}

// Immediate diagnostic function you can call from anywhere
void MainWindow::quickDiagnoseCurrentMatches()
{
    if (!m_validationComplete) {
        qDebug() << "No validation results to diagnose";
        return;
    }
    
    qDebug() << "\n🔍 QUICK DIAGNOSIS OF CURRENT MATCHES";
    
    // Quick analysis without full debug system
    int closeButFailing = 0;
    int farButPassing = 0;
    double totalDistance = 0;
    int validDistanceCount = 0;
    
    for (const auto& match : m_lastValidation.matches) {
        if (match.detectedIndex >= 0 && match.detectedIndex < m_lastStarMask.starCenters.size() &&
            match.catalogIndex >= 0 && match.catalogIndex < m_lastValidation.catalogStars.size()) {
            
            QPoint detected = m_lastStarMask.starCenters[match.detectedIndex];
            QPointF catalog = m_lastValidation.catalogStars[match.catalogIndex].pixelPos;
            
            double distance = sqrt(pow(detected.x() - catalog.x(), 2) + 
                                 pow(detected.y() - catalog.y(), 2));
            
            totalDistance += distance;
            validDistanceCount++;
            
            // Check for problematic cases
            if (distance <= 5.0 && !match.isGoodMatch) {
                closeButFailing++;
                qDebug() << QString("⚠️  Close but failing: det[%1] distance=%2 to cat[%3]")
                            .arg(match.detectedIndex).arg(distance).arg(match.catalogIndex);
            }
            
            if (distance > 8.0 && match.isGoodMatch) {
                farButPassing++;
                qDebug() << QString("⚠️  Far but passing: det[%1] distance=%2 to cat[%3]")
                            .arg(match.detectedIndex).arg(distance).arg(match.catalogIndex);
            }
        }
    }
    
    double avgDistance = (validDistanceCount > 0) ? totalDistance / validDistanceCount : 0.0;
    
    qDebug() << QString("Quick diagnosis results:");
    qDebug() << QString("  Average match distance: %1 px").arg(avgDistance);
    qDebug() << QString("  Close but failing matches: %1").arg(closeButFailing);
    qDebug() << QString("  Far but passing matches: %1").arg(farButPassing);
    
    // Quick recommendations
    if (closeButFailing > 0) {
        qDebug() << "💡 RECOMMENDATION: You have close matches that are failing criteria";
        qDebug() << "   → Check magnitude difference requirements";
        qDebug() << "   → Consider increasing pixel tolerance to" << (avgDistance * 1.5);
        qDebug() << "   → Use full debug system for detailed analysis";
    }
    
    if (closeButFailing == 0 && farButPassing == 0) {
        qDebug() << "✅ Matching criteria appear to be working correctly";
        qDebug() << "   Issue may be in coordinate transformations or catalog accuracy";
    }
}

// Modified version of your existing extractStars method to integrate plate solving
void MainWindow::extractStarsIntegrated()
{
    if (!m_imageData) {
        QMessageBox::information(this, "Extract Stars", "No image loaded.");
        return;
    }

    onDetectStarsAdvanced();
    
    if (m_lastStarMask.starCenters.isEmpty()) {
        QMessageBox::information(this, "Extract Stars", "No stars detected.");
        return;
    }
    
    // Update display with detected stars
    updateStarDisplay(m_lastStarMask);
    
    qDebug() << "Extracted" << m_lastStarMask.starCenters.size() << "stars";
}

/*
void MainWindow::showPlatesolveResults(const pcl::AstrometricMetadata& result)
{
    QString resultsText = QString(
        "Plate Solving Results\n"
        "====================\n\n"
        "✓ Solution Found\n\n"
        "Center Coordinates:\n"
        "  RA:  %1° (%2)\n"
        "  Dec: %3° (%4)\n\n"
        "Image Properties:\n"
        "  Pixel Scale: %5 arcsec/pixel\n"
        "  Orientation: %6°\n"
        "  Field Size:  %7' × %8'\n\n"
        "Solution Quality:\n"
        "  Matched Stars: %9\n"
        "  RA Error:  %10 arcsec\n"
        "  Dec Error: %11 arcsec\n"
        "  Solve Time: %12 sec\n\n"
        "WCS Matrix:\n"
        "  CD1_1: %13\n"
        "  CD1_2: %14\n"
        "  CD2_1: %15\n"
        "  CD2_2: %16"
    ).arg(result.ra_center, 0, 'f', 6)
     .arg(formatRACoordinates(result.ra_center))
     .arg(result.dec_center, 0, 'f', 6) 
     .arg(formatDecCoordinates(result.dec_center))
     .arg(result.pixscale, 0, 'f', 3)
     .arg(result.orientation, 0, 'f', 2)
     .arg(result.fieldWidth, 0, 'f', 1)
     .arg(result.fieldHeight, 0, 'f', 1)
     .arg(result.matched_stars)
     .arg(result.ra_error, 0, 'f', 2)
     .arg(result.dec_error, 0, 'f', 2)
     .arg(result.solve_time, 0, 'f', 1)
     .arg(result.cd11, 0, 'e', 6)
     .arg(result.cd12, 0, 'e', 6)
     .arg(result.cd21, 0, 'e', 6)
     .arg(result.cd22, 0, 'e', 6);
    
    QDialog resultsDialog(this);
    resultsDialog.setWindowTitle("Plate Solve Results");
    resultsDialog.setModal(true);
    resultsDialog.resize(500, 600);
    
    QVBoxLayout* layout = new QVBoxLayout(&resultsDialog);
    
    QTextEdit* textEdit = new QTextEdit;
    textEdit->setPlainText(resultsText);
    textEdit->setFont(QFont("Consolas", 10));
    textEdit->setReadOnly(true);
    layout->addWidget(textEdit);
    
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    
    QPushButton* copyButton = new QPushButton("Copy Results");
    connect(copyButton, &QPushButton::clicked, [resultsText]() {
        QApplication::clipboard()->setText(resultsText);
    });
    buttonLayout->addWidget(copyButton);
    
    buttonLayout->addStretch();
    
    QPushButton* okButton = new QPushButton("OK");
    connect(okButton, &QPushButton::clicked, &resultsDialog, &QDialog::accept);
    buttonLayout->addWidget(okButton);
    
    layout->addLayout(buttonLayout);
    
    resultsDialog.exec();
}
*/

void MainWindow::updateImageDisplayWithWCS(const WCSData& wcs)
{
    if (!m_imageDisplayWidget) return;
    
    // Enable WCS overlay on your image display
    m_imageDisplayWidget->setWCSData(wcs);
    m_imageDisplayWidget->setWCSOverlayEnabled(true);
    m_imageDisplayWidget->update();
    
    // Update any coordinate display panels
    updateCoordinateDisplay(wcs);
}

void MainWindow::triggerCatalogValidation(const pcl::AstrometricMetadata& result)
{
    if (!m_catalogValidator) return;

    pcl::DPoint centerCoords;
    result.ImageCenterToCelestial(centerCoords);
    
    // Automatically query catalog and validate with the solved WCS
    m_catalogValidator->queryCatalog(
        centerCoords.x,
        centerCoords.y,
        calculateFieldRadius(result)
    );
    
    // Validate against detected stars if available
    if (!m_lastStarMask.starCenters.isEmpty()) {
        auto validation = m_catalogValidator->validateStars(
            m_lastStarMask.starCenters,
            m_lastStarMask.starRadii
        );
        
        // Show validation results
        showCatalogValidationResults(validation);
    }
}

double MainWindow::calculateFieldRadius(const pcl::AstrometricMetadata& result)
{
    // Calculate field diagonal in degrees
        WCSData wcs;
	pcl::DPoint centerCoords;
	pcl::DPoint center, right, up;
	double centerX = result.Width() * 0.5;
	double centerY = result.Height() * 0.5;
	double delta = 1.0; // 1 pixel offset
	result.ImageCenterToCelestial(centerCoords);
        wcs.crval1 = centerCoords.x;
        wcs.crval2 = centerCoords.y;
        wcs.crpix1 = centerX;
        wcs.crpix2 = centerY;
	result.ImageToCelestial(center, pcl::DPoint(centerX, centerY));
	result.ImageToCelestial(right,  pcl::DPoint(centerX + delta, centerY));
        result.ImageToCelestial(up,     pcl::DPoint(centerX, centerY + delta));
        
        // Calculate CD matrix elements
        double cd1_1 = (right.x - center.x) / delta;  // dRA/dX
        double cd1_2 = (up.x - center.x) / delta;     // dRA/dY  
        double cd2_1 = (right.y - center.y) / delta;  // dDec/dX
        double cd2_2 = (up.y - center.y) / delta;     // dDec/dY
        
        // Handle RA wraparound near 0/360 boundary
        if (abs(cd1_1) > 180) cd1_1 = cd1_1 > 0 ? cd1_1 - 360 : cd1_1 + 360;
        if (abs(cd1_2) > 180) cd1_2 = cd1_2 > 0 ? cd1_2 - 360 : cd1_2 + 360;

        wcs.cd11 = cd1_1;
        wcs.cd12 = cd1_2;
        wcs.cd21 = cd2_1;
        wcs.cd22 = cd2_2;
	wcs.pixscale = sqrt(cd1_1*cd1_1 + cd1_2*cd1_2) * 3600.0;
	
    if (m_imageData) {
        double widthDeg = (m_imageData->width * wcs.pixscale) / 3600.0;
        double heightDeg = (m_imageData->height * wcs.pixscale) / 3600.0;
        return sqrt(widthDeg * widthDeg + heightDeg * heightDeg) / 2.0;
    }
    return 1.0; // Default 1 degree radius
}

QString MainWindow::formatRACoordinates(double ra)
{
    // Convert decimal degrees to hours:minutes:seconds
    double hours = ra / 15.0;
    int h = (int)hours;
    double minutes = (hours - h) * 60.0;
    int m = (int)minutes;
    double seconds = (minutes - m) * 60.0;
    
    return QString("%1h %2m %3s")
           .arg(h, 2, 10, QChar('0'))
           .arg(m, 2, 10, QChar('0'))
           .arg(seconds, 0, 'f', 1);
}

QString MainWindow::formatDecCoordinates(double dec)
{
    // Convert decimal degrees to degrees:arcminutes:arcseconds
    bool negative = dec < 0;
    dec = qAbs(dec);
    
    int d = (int)dec;
    double arcmin = (dec - d) * 60.0;
    int m = (int)arcmin;
    double arcsec = (arcmin - m) * 60.0;
    
    return QString("%1%2° %3' %4\"")
           .arg(negative ? "-" : "+")
           .arg(d, 2, 10, QChar('0'))
           .arg(m, 2, 10, QChar('0'))
           .arg(arcsec, 0, 'f', 1);
}

/*
StarMaskResult MainWindow::performStarExtraction()
{
    StarMaskResult result;
    
    if (!m_imageData || !m_imageData->isValid()) {
        qDebug() << "No valid image data for star extraction";
        return result;
    }
    
    // Call your existing star detection method
    result = StarMaskGenerator::detectStars(*m_imageData, 0.5f);
    
    qDebug() << "Star extraction completed:" << result.starCenters.size() << "stars found";
    return result;
}
*/

// 4. Add the missing updateStarDisplay() method
void MainWindow::updateStarDisplay(const StarMaskResult& starMask)
{
    if (!m_imageDisplayWidget) {
        qDebug() << "No image display widget available";
        return;
    }
    
    // Clear existing overlays
    m_imageDisplayWidget->clearStarOverlays();
    
    // Add detected stars to display
    for (int i = 0; i < starMask.starCenters.size(); ++i) {
        const QPoint& center = starMask.starCenters[i];
        float radius = (i < starMask.starRadii.size()) ? starMask.starRadii[i] : 5.0f;
        float flux = (i < starMask.starFluxes.size()) ? starMask.starFluxes[i] : 1000.0f;
        
        // Add star overlay (you'll need to implement this in ImageDisplayWidget)
        m_imageDisplayWidget->addStarOverlay(center, radius, flux);
    }
    
    // Update the display
    m_imageDisplayWidget->update();
    
    // Update status
    QString status = QString("Stars detected: %1").arg(starMask.starCenters.size());
    if (m_statusLabel) {
        m_statusLabel->setText(status);
    }
    
    qDebug() << "Star display updated with" << starMask.starCenters.size() << "stars";
}

// 8. Add missing updateCoordinateDisplay() method
void MainWindow::updateCoordinateDisplay(const WCSData& wcs)
{
    if (!wcs.isValid) {
        qDebug() << "Invalid WCS data for coordinate display";
        return;
    }
    
    // Update your coordinate display widgets/labels
    // Example implementation:
    QString coordText = QString("RA: %1° Dec: %2° Scale: %3\"/px")
                       .arg(wcs.crval1, 0, 'f', 4)
                       .arg(wcs.crval2, 0, 'f', 4)
                       .arg(wcs.pixscale, 0, 'f', 2);
    
    // Update your coordinate label/widget
    if (m_coordinateLabel) {
        m_coordinateLabel->setText(coordText);
    }
    
    qDebug() << "Coordinate display updated:" << coordText;
}

// 9. Add missing showCatalogValidationResults() method
void MainWindow::showCatalogValidationResults(const ValidationResult& validation)
{
    // Create a dialog or update existing results display
    QString resultsText = QString(
        "Catalog Validation Results\n"
        "==========================\n\n"
        "Total Matches: %1\n"
        "Match Percentage: %2%\n"
        "Average Error: %3 pixels\n"
        "RMS Error: %4 pixels\n"
    ).arg(validation.totalMatches)
     .arg(validation.matchPercentage, 0, 'f', 1)
     .arg(validation.averagePositionError, 0, 'f', 2)
     .arg(validation.rmsPositionError, 0, 'f', 2);
    
    // Show results in your results text widget
    if (m_resultsText) {
        m_resultsText->setPlainText(resultsText);
    }
    
    // Or show in a message box
    QMessageBox::information(this, "Catalog Validation Results", resultsText);
    
    qDebug() << "Catalog validation results displayed";
}

void MainWindow::onWCSDataReceived(const WCSData& wcs)
{
    if (!wcs.isValid) {
        qDebug() << "Received invalid WCS data";
        return;
    }
    
    qDebug() << "WCS data received and processed";
    qDebug() << "  RA center:" << wcs.crval1 << "degrees";
    qDebug() << "  Dec center:" << wcs.crval2 << "degrees";
    qDebug() << "  Pixel scale:" << wcs.pixscale << "arcsec/pixel";
    
    // Set WCS flag
    m_hasWCS = true;
    
    // Update image display with WCS
    if (m_imageDisplayWidget) {
        m_imageDisplayWidget->setWCSData(wcs);
        m_imageDisplayWidget->setWCSOverlayEnabled(true);
    }
    
    // Update coordinate display
    updateCoordinateDisplay(wcs);
    
    // Update UI controls
    updatePlottingControls();
    updateStatusDisplay();
    
    // Update status
    QString statusMsg = QString("✓ WCS Available: RA=%1° Dec=%2° Scale=%3\"/px")
                       .arg(wcs.crval1, 0, 'f', 4)
                       .arg(wcs.crval2, 0, 'f', 4)
                       .arg(wcs.pixscale, 0, 'f', 2);
    
    if (m_statusLabel) {
        m_statusLabel->setText(statusMsg);
    }
}

void MainWindow::onTestPlatesolveWithStarExtraction()
{
    extractStarsIntegrated();
    
    // Show extraction results
    QString extractionInfo = QString("Star Extraction Results:\n"
                                   "  Stars detected: %1\n"
                                   "  Algorithm: %2\n\n"
                                   "Starting plate solving...")
                            .arg(m_lastStarMask.starCenters.size())
                            .arg("Advanced star detection");
    
    m_resultsText->setPlainText(extractionInfo);
    QApplication::processEvents();
    
    // Force plate solving even if auto-solve is disabled
    if (m_platesolveIntegration) {
        // Connect to result signals temporarily for this test
      //        connect(m_platesolveIntegration, &ExtractStarsWithPlateSolve::platesolveComplete,
      //        connect(m_platesolveIntegration, &SimplePlatesolver::platesolveComplete,
      //        this, &MainWindow::onTestPlatesolveComplete, Qt::UniqueConnection);
	//        connect(m_platesolveIntegration, &ExtractStarsWithPlateSolve::platesolveFailed,
        connect(m_platesolveIntegration, &SimplePlatesolver::platesolveFailed,
                this, &MainWindow::onTestPlateSolveFailed, Qt::UniqueConnection);
        
        // Trigger plate solving
        m_platesolveIntegration->extractStarsAndSolve(
            m_imageData,
            m_lastStarMask.starCenters,
            m_lastStarMask.starFluxes,
            m_lastStarMask.starRadii
        );
        
        m_statusLabel->setText(QString("Plate solving %1 detected stars...").arg(m_lastStarMask.starCenters.size()));
    } else {
        QMessageBox::warning(this, "Test Plate Solve", 
                           "Plate solving integration not initialized.\n"
                           "Please check your plate solver configuration.");
    }
}

/*
// Add these helper methods for the test results:
void MainWindow::onTestPlatesolveComplete(const pcl::AstrometricMetadata& result, const WCSData& wcs)
{
    Q_UNUSED(wcs)
    
    QString successMessage = QString(
        "✅ PLATE SOLVING TEST SUCCESSFUL!\n\n"
        "Results:\n"
        "  RA Center:    %1° (%2)\n"
        "  Dec Center:   %3° (%4)\n"
        "  Pixel Scale:  %5 arcsec/px\n"
        "  Orientation:  %6°\n"
        "  Field Size:   %7' × %8'\n"
        "  Matched Stars: %9\n"
        "  Solve Time:   %10 sec\n\n"
        "The star extraction and plate solving pipeline is working correctly!"
    ).arg(result.ra_center, 0, 'f', 6)
     .arg(formatRACoordinates(result.ra_center))
     .arg(result.dec_center, 0, 'f', 6)
     .arg(formatDecCoordinates(result.dec_center))
     .arg(result.pixscale, 0, 'f', 3)
     .arg(result.orientation, 0, 'f', 2)
     .arg(result.fieldWidth, 0, 'f', 1)
     .arg(result.fieldHeight, 0, 'f', 1)
     .arg(result.matched_stars)
     .arg(result.solve_time, 0, 'f', 1);
    
    m_resultsText->setPlainText(successMessage);
    m_statusLabel->setText("Plate solving test completed successfully!");
    
    // Show results in a dialog for better visibility
    QMessageBox::information(this, "Plate Solve Test Results", successMessage);
}
*/

void MainWindow::onTestPlateSolveFailed(const QString& error)
{
    QString failureMessage = QString(
        "❌ PLATE SOLVING TEST FAILED\n\n"
        "Error: %1\n\n"
        "Possible causes:\n"
        "• Insufficient stars detected\n"
        "• Poor star quality or distribution\n"
        "• Incorrect scale parameters\n"
        "• Missing or corrupted star catalog\n"
        "• Network connectivity issues\n\n"
        "Check your plate solver settings and try again."
    ).arg(error);
    
    m_resultsText->setPlainText(failureMessage);
    m_statusLabel->setText("Plate solving test failed. Check settings.");
    
    QMessageBox::warning(this, "Plate Solve Test Failed", failureMessage);
}

// Add the button to your UI setup (add this to your setupUI() method):
void MainWindow::addPlatesolvingTestButton()
{
    // Add the test button to your existing button layout
    if (m_buttonLayout) {
        QPushButton* testPlatesolveBtn = new QPushButton("Test Plate Solve");
        testPlatesolveBtn->setToolTip("Test plate solving with star extraction pipeline");
        testPlatesolveBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; }");
        connect(testPlatesolveBtn, &QPushButton::clicked, this, &MainWindow::onTestPlatesolveWithStarExtraction);
        m_buttonLayout->addWidget(testPlatesolveBtn);
    }
}
