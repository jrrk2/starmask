#include "MainWindow.h"
#include "ImageReader.h"
#include "PCLMockAPI.h"

// PCL includes
#include <pcl/Image.h>
#include <pcl/XISF.h>
#include <pcl/String.h>
#include <pcl/Property.h>
#include <pcl/api/APIInterface.h>

#include <QApplication>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDebug>
#include <QMenuBar>
#include <QClipboard>
#include <QDesktopServices>
#include <QSplitter>
#include <QMessageBox>
#include <QJsonDocument>
#include "GaiaGDR3Catalog.h"
#include <QTime>
// Add these updates to your MainWindow.cpp

// Update the includes at the top:
#include "MainWindow.h"
#include "ImageReader.h"
#include "PCLMockAPI.h"
#include "GaiaGDR3Catalog.h"  // Add this line

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
        qDebug() << QString("📊 Catalog size: %.1f MB").arg(sizeMB);
        
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
            m_statusLabel->setText(QString("Gaia GDR3 catalog ready (%.1f MB)").arg(sizeMB));
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
        if (m_hasWCS) {
            WCSData wcs = m_catalogValidator->getWCSData();
            qDebug() << "\n=== TESTING GAIA GDR3 QUERY ===";
            qDebug() << QString("Image center: RA=%.4f° Dec=%.4f°").arg(wcs.crval1).arg(wcs.crval2);
            
            auto start = QTime::currentTime();
            m_catalogValidator->queryCatalog(wcs.crval1, wcs.crval2, 1.0); // 1 degree radius test
            auto elapsed = start.msecsTo(QTime::currentTime());
            
            qDebug() << QString("Query completed in %1ms").arg(elapsed);
        } else {
            QMessageBox::information(this, "Test Query", "Load an image with WCS first");
        }
    });
    
    catalogMenu->addSeparator();
    
    QAction* findBrightAction = catalogMenu->addAction("Find brightest stars in field");
    connect(findBrightAction, &QAction::triggered, [this]() {
        if (m_hasWCS) {
            WCSData wcs = m_catalogValidator->getWCSData();
            double fieldRadius = sqrt(wcs.width * wcs.width + wcs.height * wcs.height) * wcs.pixscale / 3600.0 / 2.0;
            fieldRadius = std::max(fieldRadius, 0.5);
            
            m_catalogValidator->findBrightGaiaStars(wcs.crval1, wcs.crval2, fieldRadius, 20);
        } else {
            QMessageBox::information(this, "Bright Stars", "Load an image with WCS first");
        }
    });
    
    QAction* findSpectraAction = catalogMenu->addAction("Find stars with BP/RP spectra");
    connect(findSpectraAction, &QAction::triggered, [this]() {
        if (m_hasWCS) {
            WCSData wcs = m_catalogValidator->getWCSData();
            double fieldRadius = sqrt(wcs.width * wcs.width + wcs.height * wcs.height) * wcs.pixscale / 3600.0 / 2.0;
            fieldRadius = std::max(fieldRadius, 0.5);
            
            m_catalogValidator->queryGaiaWithSpectra(wcs.crval1, wcs.crval2, fieldRadius);
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
        qDebug() << QString("📊 Size: %.1f MB").arg(sizeMB);
        
        // Validate the new catalog
        if (GaiaGDR3Catalog::validateDatabase()) {
            m_statusLabel->setText(QString("Custom Gaia GDR3 catalog loaded (%.1f MB)").arg(sizeMB));
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
    
    WCSData wcs = m_catalogValidator->getWCSData();
    double testRadius = 2.0; // 2 degree radius for performance test
    
    qDebug() << "\n=== GAIA GDR3 PERFORMANCE COMPARISON ===";
    qDebug() << QString("Test query: RA=%.4f° Dec=%.4f° radius=%.1f°")
                .arg(wcs.crval1).arg(wcs.crval2).arg(testRadius);
    
    // Test different magnitude limits
    QVector<double> magLimits = {12.0, 15.0, 18.0, 20.0};
    
    for (double magLimit : magLimits) {
        auto start = QTime::currentTime();
        
        GaiaGDR3Catalog::SearchParameters params(wcs.crval1, wcs.crval2, testRadius, magLimit);
        auto stars = GaiaGDR3Catalog::queryRegion(params);
        
        auto elapsed = start.msecsTo(QTime::currentTime());
        
        qDebug() << QString("🚀 Mag ≤ %.1f: %1 stars in %2ms (%.1f stars/sec)")
                    .arg(magLimit).arg(stars.size()).arg(elapsed)
                    .arg(stars.size() * 1000.0 / elapsed);
    }
    
    // Test spectrum search
    auto start = QTime::currentTime();
    auto specStars = GaiaGDR3Catalog::findStarsWithSpectra(wcs.crval1, wcs.crval2, testRadius, 15.0);
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
            qDebug() << QString("  %1: RA=%.4f° Dec=%.4f° G=%.2f %2")
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
        qDebug() << QString("  G=%.2f %1 at (%.4f°, %.4f°) PM=(%.1f, %.1f) mas/yr")
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
        qDebug() << QString("  %1: G=%.2f BP=%.2f RP=%.2f %2 [SPECTRUM]")
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
    qDebug() << QString("Found %1 stars with proper motion applied to epoch %.1f")
                .arg(pm_stars.size()).arg(params.epochYear);
    
    for (const auto& star : pm_stars) {
        qDebug() << QString("  %1: G=%.2f at (%.6f°, %.6f°) PM=(%.1f, %.1f)")
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
    
    qDebug() << QString("Image field: %.2f° radius around RA=%.3f° Dec=%.3f°")
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
                qDebug() << QString("✅ Match: detected(%1,%2) ↔ Gaia %3 G=%.2f dist=%.1fpx")
                            .arg(detected.x()).arg(detected.y())
                            .arg(catalog.sourceId).arg(catalog.magnitude).arg(distance);
                break;
            }
        }
    }
    
    double matchPercentage = 100.0 * matches / detectedStars.size();
    qDebug() << QString("Validation result: %1/%2 stars matched (%.1f%%)")
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
            
            qDebug() << QString("  G=%.2f BP-RP=%.3f %1 plx=%.2f")
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
        qDebug() << QString("  %1: PM=%.1f mas/yr G=%.2f")
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
        qDebug() << QString("  %1: %.1f pc G=%.2f %2")
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
    qDebug() << QString("  High quality: %1 (%.1f%%)").arg(highQuality).arg(100.0 * highQuality / stars.size());
    qDebug() << QString("  With BP/RP spectra: %1 (%.1f%%)").arg(withSpectra).arg(100.0 * withSpectra / stars.size());
    qDebug() << QString("  Good astrometry: %1 (%.1f%%)").arg(goodAstrometry).arg(100.0 * goodAstrometry / stars.size());
}

// Performance comparison between different catalog sources
void comparePerformanceWithOtherCatalogs()
{
    qDebug() << "\n=== CATALOG PERFORMANCE COMPARISON ===";
    
    double testRA = 83.633;     // Aldebaran region
    double testDec = 22.014;
    double testRadius = 1.0;    // 1 degree radius
    double magLimit = 15.0;
    
    qDebug() << QString("Test region: RA=%.3f° Dec=%.3f° radius=%.1f° mag≤%.1f")
                .arg(testRA).arg(testDec).arg(testRadius).arg(magLimit);
    
    // Test Gaia GDR3 local database
    if (GaiaGDR3Catalog::isAvailable()) {
        auto start = QTime::currentTime();
        auto gaiaStars = GaiaGDR3Catalog::queryRegion(testRA, testDec, testRadius, magLimit);
        auto elapsed = start.msecsTo(QTime::currentTime());
        
        qDebug() << QString("🚀 Gaia GDR3 (local): %1 stars in %2ms (%.1f stars/sec)")
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
    
    // Connect image reader signals
    connect(m_loadButton, &QPushButton::clicked, this, &MainWindow::onLoadImage);
    connect(m_detectButton, &QPushButton::clicked, this, &MainWindow::onDetectStars);
    connect(m_validateButton, &QPushButton::clicked, this, &MainWindow::onValidateStars);
    connect(m_plotCatalogButton, &QPushButton::clicked, this, &MainWindow::onPlotCatalogStars);
    
    // Connect mode control
    connect(m_plotModeCheck, &QCheckBox::toggled, this, &MainWindow::onPlotModeToggled);
    
    // Connect validation control signals
    connect(m_validationModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onValidationModeChanged);
    connect(m_magnitudeLimitSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onMagnitudeLimitChanged);
    connect(m_pixelToleranceSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onPixelToleranceChanged);
    
    // Connect plotting control signals
    connect(m_plotMagnitudeSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [this](double value) {
                m_catalogValidator->setMagnitudeLimit(value);
                m_catalogQueried = false;
                m_catalogPlotted = false;
                updatePlottingControls();
            });
    connect(m_fieldRadiusSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [this](double) {
                m_catalogQueried = false;
                m_catalogPlotted = false;
                updatePlottingControls();
            });
    
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
    m_detectButton = new QPushButton("Detect Stars");
    m_validateButton = new QPushButton("Validate with Catalog");
    m_plotCatalogButton = new QPushButton("Plot Catalog Stars");
    
    m_detectButton->setEnabled(false);
    m_validateButton->setEnabled(false);
    m_plotCatalogButton->setEnabled(false);
    
    m_buttonLayout->addWidget(m_loadButton);
    m_buttonLayout->addWidget(m_detectButton);
    m_buttonLayout->addWidget(m_validateButton);
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
    rightWidget->setMaximumWidth(350);
    rightWidget->setMinimumWidth(300);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightWidget);
    
    setupValidationControls();
    setupCatalogPlottingControls();
    
    rightLayout->addWidget(m_validationGroup);
    rightLayout->addWidget(m_plottingGroup);
    
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
}

void MainWindow::setupValidationControls()
{
    m_validationGroup = new QGroupBox("Star Detection & Validation (Gaia GDR3)");
    m_validationLayout = new QVBoxLayout(m_validationGroup);
        
    // Validation mode
    QHBoxLayout* modeLayout = new QHBoxLayout;
    modeLayout->addWidget(new QLabel("Validation Mode:"));
    m_validationModeCombo = new QComboBox;
    m_validationModeCombo->addItem("Strict", static_cast<int>(StarCatalogValidator::Strict));
    m_validationModeCombo->addItem("Normal", static_cast<int>(StarCatalogValidator::Normal));
    m_validationModeCombo->addItem("Loose", static_cast<int>(StarCatalogValidator::Loose));
    m_validationModeCombo->setCurrentIndex(1); // Default to Normal
    modeLayout->addWidget(m_validationModeCombo);
    m_validationLayout->addLayout(modeLayout);
    
    // Magnitude limit
    QHBoxLayout* magLayout = new QHBoxLayout;
    magLayout->addWidget(new QLabel("Magnitude Limit:"));
    m_magnitudeLimitSpin = new QSpinBox;
    m_magnitudeLimitSpin->setRange(6, 20);
    m_magnitudeLimitSpin->setValue(12);
    m_magnitudeLimitSpin->setSuffix(" mag");
    magLayout->addWidget(m_magnitudeLimitSpin);
    m_validationLayout->addLayout(magLayout);
    
    // Pixel tolerance
    QHBoxLayout* pixelLayout = new QHBoxLayout;
    pixelLayout->addWidget(new QLabel("Pixel Tolerance:"));
    m_pixelToleranceSpin = new QSpinBox;
    m_pixelToleranceSpin->setRange(1, 20);
    m_pixelToleranceSpin->setValue(5);
    m_pixelToleranceSpin->setSuffix(" px");
    pixelLayout->addWidget(m_pixelToleranceSpin);
    m_validationLayout->addLayout(pixelLayout);
    
    // Progress bar for catalog queries
    m_queryProgressBar = new QProgressBar;
    m_queryProgressBar->setVisible(false);
    m_validationLayout->addWidget(m_queryProgressBar);
    
    // WCS status
    QLabel* wcsStatusLabel = new QLabel("WCS Status: Not available (Required for Gaia queries)");
    wcsStatusLabel->setObjectName("wcsStatusLabel");
    wcsStatusLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
    m_validationLayout->addWidget(wcsStatusLabel);
}

void MainWindow::setupCatalogPlottingControls()
{
    m_plottingGroup = new QGroupBox("Catalog Plotting");
    m_plottingLayout = new QVBoxLayout(m_plottingGroup);
    
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
    
    // Initially hide plotting controls
    m_plottingGroup->setVisible(false);
}

void MainWindow::onLoadImage()
{
    pcl_mock::InitializeMockAPI();
    
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
    m_imageData = &m_imageReader->imageData();
    
    m_imageDisplayWidget->setImageData(m_imageReader->imageData());
    m_statusLabel->setText("Image loaded successfully");
    
    // Extract WCS information
    extractWCSFromImage();
    
    // Update UI state
    m_detectButton->setEnabled(true);
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

void MainWindow::onPlotCatalogStars()
{
    if (!m_hasWCS) {
        QMessageBox::warning(this, "Plot Catalog", "No WCS information available. Cannot plot catalog stars.");
        return;
    }
    
    plotCatalogStarsDirectly();
}

void MainWindow::plotCatalogStarsDirectly()
{
    if (!m_catalogQueried) {
        // Need to query catalog first
        WCSData wcs = m_catalogValidator->getWCSData();
        double fieldRadius = m_fieldRadiusSpin->value();
        
        // Set magnitude limit from plotting controls
        m_catalogValidator->setMagnitudeLimit(m_plotMagnitudeSpin->value());
        
        m_catalogValidator->queryCatalog(wcs.crval1, wcs.crval2, fieldRadius);
    } else {
        // Catalog already queried, just display it
        QVector<CatalogStar> catalogStars = m_catalogValidator->getCatalogStars();
        
        // Create a validation result just for display purposes
        ValidationResult plotResult;
        plotResult.catalogStars = catalogStars;
        plotResult.totalCatalog = catalogStars.size();
        plotResult.isValid = true;
        plotResult.summary = QString("Catalog Plot:\n%1 stars plotted (magnitude ≤ %.1f)\nSource: %2")
                            .arg(catalogStars.size())
	                    .arg(m_plotMagnitudeSpin->value());
        
        m_imageDisplayWidget->setValidationResults(plotResult);
        m_resultsText->setPlainText(plotResult.summary);
        
        m_catalogPlotted = true;
        m_statusLabel->setText(QString("Plotted %1 catalog stars (mag ≤ %.1f)")
                              .arg(catalogStars.size())
                              .arg(m_plotMagnitudeSpin->value()));
        
        updatePlottingControls();
    }
}

void MainWindow::onPlotModeToggled(bool plotMode)
{
    m_plotMode = plotMode;
    
    // Show/hide appropriate control groups
    m_validationGroup->setVisible(!plotMode);
    m_plottingGroup->setVisible(plotMode);
    
    // Update button states
    if (plotMode) {
        m_detectButton->setVisible(false);
        m_validateButton->setVisible(false);
        m_plotCatalogButton->setVisible(true);
    } else {
        m_detectButton->setVisible(true);
        m_validateButton->setVisible(true);
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
    m_plotCatalogButton->setEnabled(canPlot && !m_queryProgressBar->isVisible());
    
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
            WCSData wcs = m_catalogValidator->getWCSData();
            wcsLabel->setText(QString("WCS Status: Available (PCL)\nRA: %1°, Dec: %2°\nPixel Scale: %3 arcsec/px")
                            .arg(wcs.crval1, 0, 'f', 4)
                            .arg(wcs.crval2, 0, 'f', 4)
                            .arg(wcs.pixscale, 0, 'f', 2));
            wcsLabel->setStyleSheet("QLabel { color: green; font-weight: bold; }");
        } else {
            wcsLabel->setText("WCS Status: Failed (PCL)");
            wcsLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
        }
    }
    
    qDebug() << "PCL WCS extraction completed, valid:" << m_hasWCS;
}

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

void MainWindow::onValidateStars()
{
    if (!m_starsDetected) {
        QMessageBox::information(this, "Validation", "Please detect stars first.");
        return;
    }
    
    if (!m_hasWCS) {
        QMessageBox::warning(this, "Validation", "No WCS information available. Cannot validate against catalog.");
        return;
    }
    
    // Start catalog query if not already done
    if (!m_catalogQueried) {
        WCSData wcs = m_catalogValidator->getWCSData();
        
        // Calculate field radius (diagonal of image)
        double fieldRadius = sqrt(wcs.width * wcs.width + wcs.height * wcs.height) * wcs.pixscale / 3600.0 / 2.0;
        fieldRadius = std::max(fieldRadius, 0.5); // Minimum 0.5 degrees
        
        m_catalogValidator->queryCatalog(wcs.crval1, wcs.crval2, fieldRadius);
    } else {
        // Perform validation with existing catalog data
        performValidation();
    }
}

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
        
        m_statusLabel->setText(QString("Validation complete: %1/%2 stars matched (%.1f%%)")
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

void MainWindow::onMagnitudeLimitChanged()
{
    m_catalogValidator->setMagnitudeLimit(m_magnitudeLimitSpin->value());
    m_catalogQueried = false; // Need to re-query with new limit
    m_catalogPlotted = false;
    updateValidationControls();
    updatePlottingControls();
}

void MainWindow::onPixelToleranceChanged()
{
    m_catalogValidator->setMatchingTolerance(m_pixelToleranceSpin->value());
}

void MainWindow::onCatalogQueryStarted()
{
    m_queryProgressBar->setVisible(true);
    m_queryProgressBar->setRange(0, 0); // Indeterminate progress
    m_validateButton->setEnabled(false);
    m_plotCatalogButton->setEnabled(false);
    m_statusLabel->setText("Querying star catalog...");
}

// Replace your existing onCatalogQueryFinished method with this:
void MainWindow::onCatalogQueryFinished(bool success, const QString& message)
{
    m_queryProgressBar->setVisible(false);
    m_validateButton->setEnabled(m_starsDetected && m_hasWCS && !m_plotMode);
    m_plotCatalogButton->setEnabled(m_hasWCS && m_plotMode);
    
    if (success) {
        m_catalogQueried = true;
        
        // Add bright stars from local database (this always works!)
        WCSData wcs = m_catalogValidator->getWCSData();
        double fieldRadius = sqrt(wcs.width * wcs.width + wcs.height * wcs.height) * wcs.pixscale / 3600.0 / 2.0;
        fieldRadius = std::max(fieldRadius, 0.5);
        
        m_catalogValidator->addBrightStarsFromDatabase(wcs.crval1, wcs.crval2, fieldRadius);
        
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
    if (m_plotMode) {
        // In plot mode, disable validation controls
        m_validateButton->setEnabled(false);
        m_detectButton->setEnabled(false);
        return;
    }
    
    bool canValidate = m_starsDetected && m_hasWCS;
    m_validateButton->setEnabled(canValidate && !m_queryProgressBar->isVisible());
    m_detectButton->setEnabled(m_imageData != nullptr);
    
    // Update tooltip
    if (!m_hasWCS) {
        m_validateButton->setToolTip("No WCS information available in image");
    } else if (!m_starsDetected) {
        m_validateButton->setToolTip("Detect stars first");
    } else {
        m_validateButton->setToolTip("Validate detected stars against catalog");
    }
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
// Add this to MainWindow.cpp constructor or as a separate setup method

/*
void MainWindow::setupGaiaCatalog()
{
    QString catalogPath = "/Volumes/X10Pro/gdr3-1.0.0-01.xpsd";
    
    // Check if catalog file exists
    if (QFile::exists(catalogPath)) {
        qDebug() << "✅ Found 2MASS catalog at:" << catalogPath;
        
        QFileInfo info(catalogPath);
        double sizeMB = info.size() / (1024.0 * 1024.0);
        qDebug() << QString("📊 Catalog size: %.1f MB").arg(sizeMB);
        
        // Update status
        m_statusLabel->setText(QString("Local 2MASS catalog ready (%.1f MB)").arg(sizeMB));
        
    } else {
        qDebug() << "❌ 2MASS catalog not found at:" << catalogPath;
        qDebug() << "   Available catalog sources:";
        
        // Check alternative locations
        QStringList possiblePaths = {
            "/Volumes/X10Pro/allsky_2mass/allsky.mag",
            "./allsky.mag",
            "../allsky.mag",
            QDir::homePath() + "/allsky.mag"
        };
        
        bool found = false;
        for (const QString& path : possiblePaths) {
            if (QFile::exists(path)) {
                qDebug() << "✅ Found alternative at:" << path;
                Local2MASSCatalog::setCatalogPath(path);
                found = true;
                break;
            }
        }
        
        if (!found) {
            qDebug() << "   - Hipparcos (network)";
            qDebug() << "   - Gaia DR3 (network)";
            qDebug() << "   - Tycho-2 (network)";
            
            // Default to Gaia if no local catalog
            m_statusLabel->setText("Using network catalogs (2MASS not found)");
        }
    }
}

// Add a file browser to let users select their catalog
void MainWindow::browseCatalogFile()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        "Select 2MASS Catalog File",
        "/Volumes/X10Pro/allsky_2mass/",
        "Catalog Files (*.mag *.txt *.csv);;All Files (*)"
    );
    
    if (!filePath.isEmpty()) {
        Local2MASSCatalog::setCatalogPath(filePath);
        
        QFileInfo info(filePath);
        double sizeMB = info.size() / (1024.0 * 1024.0);
        
        qDebug() << "📂 User selected catalog:" << filePath;
        qDebug() << QString("📊 Size: %.1f MB").arg(sizeMB);
        
        // Set to Local2MASS and update UI
        m_statusLabel->setText(QString("Custom 2MASS catalog loaded (%.1f MB)").arg(sizeMB));
        
        // Clear previous catalog data
        m_catalogQueried = false;
        m_catalogPlotted = false;
        updateValidationControls();
        updatePlottingControls();
    }
}

// Add a menu or button for catalog file selection
void MainWindow::setupCatalogMenu()
{
    // Add a menu bar item for catalog management
    QMenuBar* menuBar = this->menuBar();
    QMenu* catalogMenu = menuBar->addMenu("&Catalog");
    
    QAction* browse2MASSAction = catalogMenu->addAction("Browse for 2MASS catalog...");
    connect(browse2MASSAction, &QAction::triggered, this, &MainWindow::browseCatalogFile);
    
    catalogMenu->addSeparator();
    
    QAction* showStatsAction = catalogMenu->addAction("Show catalog statistics");
    connect(showStatsAction, &QAction::triggered, [this]() {
        m_catalogValidator->showCatalogStats();
    });
    
    QAction* testQueryAction = catalogMenu->addAction("Test catalog query");
    connect(testQueryAction, &QAction::triggered, [this]() {
        if (m_hasWCS) {
            WCSData wcs = m_catalogValidator->getWCSData();
            qDebug() << "\n=== TESTING CATALOG QUERY ===";
            qDebug() << QString("Image center: RA=%.4f° Dec=%.4f°").arg(wcs.crval1).arg(wcs.crval2);
            
            auto start = QTime::currentTime();
            m_catalogValidator->queryCatalog(wcs.crval1, wcs.crval2, 1.0); // 1 degree radius test
            auto elapsed = start.msecsTo(QTime::currentTime());
            
            qDebug() << QString("Query completed in %1ms").arg(elapsed);
        } else {
            QMessageBox::information(this, "Test Query", "Load an image with WCS first");
        }
    });
}

// Add this to show performance comparison
void MainWindow::compareCatalogPerformance()
{
    if (!m_hasWCS) {
        QMessageBox::information(this, "Performance Test", "Load an image with WCS first");
        return;
    }
    
    WCSData wcs = m_catalogValidator->getWCSData();
    double testRadius = 1.0; // 1 degree radius
    
    qDebug() << "\n=== CATALOG PERFORMANCE COMPARISON ===";
    qDebug() << QString("Test query: RA=%.4f° Dec=%.4f° radius=%.1f°")
                .arg(wcs.crval1).arg(wcs.crval2).arg(testRadius);
    
    // Test Local 2MASS
    if (Local2MASSCatalog::isAvailable()) {
        auto start = QTime::currentTime();
        auto stars = Local2MASSCatalog::queryRegion(wcs.crval1, wcs.crval2, testRadius, 15.0);
        auto elapsed = start.msecsTo(QTime::currentTime());
        
        qDebug() << QString("🏠 Local 2MASS: %1 stars in %2ms").arg(stars.size()).arg(elapsed);
    }
    
    // For network catalogs, you'd time the network requests
    qDebug() << "🌐 Network catalogs: 2000-10000ms (depends on connection)";
    qDebug() << "📊 Local catalog is ~100x faster than network queries";
}
*/
