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
#include <QClipboard>
#include <QDesktopServices>
#include <QSplitter>
#include <QMessageBox>
#include <QJsonDocument>

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

MainWindow::~MainWindow() = default;

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
    m_validationGroup = new QGroupBox("Star Detection & Validation");
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
    QLabel* wcsStatusLabel = new QLabel("WCS Status: Not available");
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
