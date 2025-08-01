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
{
    setupUI();
    
    // Connect image reader signals
    connect(m_loadButton, &QPushButton::clicked, this, &MainWindow::onLoadImage);
    connect(m_detectButton, &QPushButton::clicked, this, &MainWindow::onDetectStars);
    connect(m_validateButton, &QPushButton::clicked, this, &MainWindow::onValidateStars);
    
    // Connect validation control signals
    connect(m_catalogSourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onCatalogSourceChanged);
    connect(m_validationModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onValidationModeChanged);
    connect(m_magnitudeLimitSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onMagnitudeLimitChanged);
    connect(m_pixelToleranceSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onPixelToleranceChanged);
    
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
    setWindowTitle("Star Detection & Catalog Validation");
    
    updateValidationControls();
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
    
    m_detectButton->setEnabled(false);
    m_validateButton->setEnabled(false);
    
    m_buttonLayout->addWidget(m_loadButton);
    m_buttonLayout->addWidget(m_detectButton);
    m_buttonLayout->addWidget(m_validateButton);
    m_buttonLayout->addStretch();
    
    leftLayout->addLayout(m_buttonLayout);
    
    // Status label
    m_statusLabel = new QLabel("Ready - Load an image to begin");
    leftLayout->addWidget(m_statusLabel);
    
    // Right side - validation controls and results
    QWidget* rightWidget = new QWidget;
    rightWidget->setMaximumWidth(350);
    rightWidget->setMinimumWidth(300);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightWidget);
    
    setupValidationControls();
    rightLayout->addWidget(m_validationGroup);
    
    // Results display
    QGroupBox* resultsGroup = new QGroupBox("Validation Results");
    QVBoxLayout* resultsLayout = new QVBoxLayout(resultsGroup);
    
    m_resultsText = new QTextEdit;
    m_resultsText->setMaximumHeight(200);
    m_resultsText->setReadOnly(true);
    m_resultsText->setPlainText("No validation results yet.\n\nLoad an image with WCS information and detect stars to begin validation.");
    
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
    m_validationGroup = new QGroupBox("Catalog Validation Settings");
    m_validationLayout = new QVBoxLayout(m_validationGroup);
    
    // Catalog source selection
    QHBoxLayout* catalogLayout = new QHBoxLayout;
    catalogLayout->addWidget(new QLabel("Catalog Source:"));
    m_catalogSourceCombo = new QComboBox;
    m_catalogSourceCombo->addItem("Hipparcos (Bright Stars)", static_cast<int>(StarCatalogValidator::Hipparcos));
    m_catalogSourceCombo->addItem("Tycho-2", static_cast<int>(StarCatalogValidator::Tycho2));
    m_catalogSourceCombo->addItem("Gaia DR3", static_cast<int>(StarCatalogValidator::Gaia));
    m_catalogSourceCombo->addItem("Custom", static_cast<int>(StarCatalogValidator::Custom));
    catalogLayout->addWidget(m_catalogSourceCombo);
    m_validationLayout->addLayout(catalogLayout);
    
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
    m_starsDetected = false;
    m_catalogQueried = false;
    m_validationComplete = false;
    
    updateValidationControls();
    updateStatusDisplay();
    
    // Clear previous results
    m_imageDisplayWidget->clearStarOverlay();
    m_imageDisplayWidget->clearValidationResults();
    m_resultsText->clear();
}

void MainWindow::extractWCSFromImage()
{
    m_hasWCS = false;
    
    if (!m_imageData || m_imageData->metadata.isEmpty()) {
        qDebug() << "No metadata available for WCS extraction";
        return;
    }
    
    // Try to extract WCS from metadata
    m_catalogValidator->setWCSFromMetadata(m_imageData->metadata);
    m_hasWCS = m_catalogValidator->hasValidWCS();
    
    // Update WCS status display
    QLabel* wcsLabel = findChild<QLabel*>("wcsStatusLabel");
    if (wcsLabel) {
        if (m_hasWCS) {
            WCSData wcs = m_catalogValidator->getWCSData();
            wcsLabel->setText(QString("WCS Status: Available\nRA: %1°, Dec: %2°\nPixel Scale: %3 arcsec/px")
                            .arg(wcs.crval1, 0, 'f', 4)
                            .arg(wcs.crval2, 0, 'f', 4)
                            .arg(wcs.pixscale, 0, 'f', 2));
            wcsLabel->setStyleSheet("QLabel { color: green; font-weight: bold; }");
        } else {
            wcsLabel->setText("WCS Status: Not available");
            wcsLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
        }
    }
    
    qDebug() << "WCS extraction completed, valid:" << m_hasWCS;
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

void MainWindow::onCatalogSourceChanged()
{
    auto source = static_cast<StarCatalogValidator::CatalogSource>(m_catalogSourceCombo->currentData().toInt());
    m_catalogValidator->setCatalogSource(source);
    m_catalogQueried = false; // Need to re-query with new source
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
    updateValidationControls();
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
    m_statusLabel->setText("Querying star catalog...");
}

void MainWindow::onCatalogQueryFinished(bool success, const QString& message)
{
    m_queryProgressBar->setVisible(false);
    m_validateButton->setEnabled(m_starsDetected && m_hasWCS);
    
    if (success) {
        m_catalogQueried = true;
        m_statusLabel->setText(message);
        
        // Automatically perform validation after successful catalog query
        performValidation();
    } else {
        m_statusLabel->setText("Catalog query failed: " + message);
        QMessageBox::warning(this, "Catalog Query Error", message);
    }
    
    updateValidationControls();
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
    if (!m_catalogQueried) return;
    
    QVector<CatalogStar> catalogStars = m_catalogValidator->getCatalogStars();
    QString statusText = QString("Catalog stars %1 (%2 total)")
                        .arg(visible ? "visible" : "hidden")
                        .arg(catalogStars.size());
    m_statusLabel->setText(statusText);
}

void MainWindow::onValidationOverlayToggled(bool visible)
{
    if (!m_validationComplete) return;
    
    QString statusText = QString("Validation results %1 (%2 matches)")
                        .arg(visible ? "visible" : "hidden")
                        .arg(m_lastValidation.totalMatches);
    m_statusLabel->setText(statusText);
}

void MainWindow::updateValidationControls()
{
    bool canValidate = m_starsDetected && m_hasWCS;
    m_validateButton->setEnabled(canValidate && !m_queryProgressBar->isVisible());
    
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
        
        if (m_starsDetected) {
            status += QString(", %1 stars detected").arg(m_lastStarMask.starCenters.size());
        }
        
        if (m_validationComplete) {
            status += QString(", %1 matched").arg(m_lastValidation.totalMatches);
        }
    }
    
    m_statusLabel->setText(status);
}

void MainWindow::runStarDetection()
{
    // This method can be kept for backwards compatibility or removed
    onDetectStars();
}
