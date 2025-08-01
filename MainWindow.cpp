#include "MainWindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_imageReader(std::make_unique<ImageReader>())
{
    setupUI();
    setupMenuBar();
    setupStatusBar();
    
    // Initialize with no file loaded state
    m_statusLabel->setText("Ready - No image loaded");
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUI()
{
    m_centralWidget = new QWidget;
    setCentralWidget(m_centralWidget);
    
    m_mainLayout = new QVBoxLayout(m_centralWidget);
    
    // Create tab widget
    m_tabWidget = new QTabWidget;
    m_mainLayout->addWidget(m_tabWidget);
    
    setupViewTab();
    setupBackgroundTab();
}

void MainWindow::setupMenuBar()
{
    // File menu
    QMenu* fileMenu = menuBar()->addMenu("&File");
    
    QAction* openAction = fileMenu->addAction("&Open...");
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::onOpenFile);
    
    m_saveAsAction = fileMenu->addAction("Save &As...");
    m_saveAsAction->setShortcut(QKeySequence::SaveAs);
    m_saveAsAction->setEnabled(false);
    connect(m_saveAsAction, &QAction::triggered, this, &MainWindow::onSaveAs);
    
    fileMenu->addSeparator();
    
    QAction* exitAction = fileMenu->addAction("E&xit");
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
/*        
    // Background menu
    QMenu* backgroundMenu = menuBar()->addMenu("&Background");
    
    m_extractBackgroundAction = backgroundMenu->addAction("&Extract Background...");
    m_extractBackgroundAction->setShortcut(QKeySequence("Ctrl+B"));
    m_extractBackgroundAction->setToolTip("Extract background model from the current image");
    m_extractBackgroundAction->setEnabled(false);
    connect(m_extractBackgroundAction, &QAction::triggered, this, &MainWindow::onExtractBackground);
    
    backgroundMenu->addSeparator();

    m_showBackgroundAction = backgroundMenu->addAction("Show Background &Model");
    m_showBackgroundAction->setShortcut(QKeySequence("Ctrl+M"));
    m_showBackgroundAction->setEnabled(false);
    connect(m_showBackgroundAction, &QAction::triggered, this, &MainWindow::onShowBackgroundModel);

    m_applyBackgroundAction = backgroundMenu->addAction("&Apply Background Correction");
    m_applyBackgroundAction->setShortcut(QKeySequence("Ctrl+Shift+B"));
    m_applyBackgroundAction->setEnabled(false);
    connect(m_applyBackgroundAction, &QAction::triggered, this, &MainWindow::onApplyBackgroundCorrection);
*/    
    // Help menu
    QMenu* helpMenu = menuBar()->addMenu("&Help");
    
    QAction* aboutAction = helpMenu->addAction("&About");
    connect(aboutAction, &QAction::triggered, this, &MainWindow::onAbout);
}

void MainWindow::setupStatusBar()
{
    m_statusLabel = new QLabel("Ready");
    statusBar()->addWidget(m_statusLabel, 1);
    
    m_statusProgress = new QProgressBar;
    m_statusProgress->setVisible(false);
    m_statusProgress->setMaximumWidth(200);
    statusBar()->addPermanentWidget(m_statusProgress);
}

void MainWindow::setupViewTab()
{
    m_viewTab = new QWidget;
    m_tabWidget->addTab(m_viewTab, "View");
    
    auto* viewLayout = new QHBoxLayout(m_viewTab);
    
    // Create splitter for image display and info panel
    m_viewSplitter = new QSplitter(Qt::Horizontal);
    viewLayout->addWidget(m_viewSplitter);
    
    // Image display widget
    m_imageDisplay = new ImageDisplayWidget;
    m_viewSplitter->addWidget(m_imageDisplay);
    
    // Info panel
    auto* infoWidget = new QWidget;
    auto* infoLayout = new QVBoxLayout(infoWidget);
    
    // Image info
    m_infoGroup = new QGroupBox("Image Information");
    auto* infoGroupLayout = new QVBoxLayout(m_infoGroup);
    
    m_infoText = new QTextEdit;
    m_infoText->setMaximumHeight(150);
    m_infoText->setReadOnly(true);
    m_infoText->setPlainText("No image loaded");
    infoGroupLayout->addWidget(m_infoText);
    
    infoLayout->addWidget(m_infoGroup);
    
    // Log panel
    auto* logGroup = new QGroupBox("Log");
    auto* logLayout = new QVBoxLayout(logGroup);
    
    m_logText = new QTextEdit;
    m_logText->setReadOnly(true);
    m_logText->setMaximumHeight(200);
    logLayout->addWidget(m_logText);
    
    infoLayout->addWidget(logGroup);
    infoLayout->addStretch();
    
    m_viewSplitter->addWidget(infoWidget);
    
    // Set splitter proportions (image display gets more space)
    m_viewSplitter->setStretchFactor(0, 3);
    m_viewSplitter->setStretchFactor(1, 1);
}

void MainWindow::setupBackgroundTab()
{
    m_backgroundTab = new QWidget;
    m_tabWidget->addTab(m_backgroundTab, "Background");
    
    auto* backgroundLayout = new QVBoxLayout(m_backgroundTab);
    
    // Create background extraction widget
    m_backgroundWidget = new BackgroundExtractionWidget;
    
    // Connect background extraction signals
    connect(m_backgroundWidget, &BackgroundExtractionWidget::backgroundExtracted,
            this, &MainWindow::onBackgroundExtracted);
    connect(m_backgroundWidget, &BackgroundExtractionWidget::backgroundModelChanged,
            this, &MainWindow::onBackgroundModelChanged);
    connect(m_backgroundWidget, &BackgroundExtractionWidget::correctedImageReady,
            this, &MainWindow::onCorrectedImageReady);
    
    // Connect image clicks for manual sampling
    connect(m_imageDisplay, &ImageDisplayWidget::imageClicked,
            m_backgroundWidget, &BackgroundExtractionWidget::onImageClicked);
    
    backgroundLayout->addWidget(m_backgroundWidget);
}

void MainWindow::onOpenFile()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Open Image File",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        ImageReader::formatFilter()
    );
    
    if (!fileName.isEmpty()) {
        loadImageFile(fileName);
    }
}

void MainWindow::onSaveAs()
{
    if (!m_imageReader->hasImage()) {
        QMessageBox::information(this, "No Image", "No image to save.");
        return;
    }
    
    QString fileName = QFileDialog::getSaveFileName(
        this,
        "Save Image As",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/output.xisf",
        "XISF Files (*.xisf);;All Files (*)"
    );
    
    if (!fileName.isEmpty()) {
        // Implementation would save the current image
        logMessage("Save functionality not yet implemented");
    }
}

void MainWindow::onAbout()
{
    QMessageBox::about(this, "About XISF Test Creator",
        "XISF Test Creator v1.0\n\n"
        "A tool for reading, processing, and writing XISF image files.\n"
        "Built with Qt and PCL integration.");
}

void MainWindow::loadImageFile(const QString& filePath)
{
    if (m_imageReader->readFile(filePath)) {
        const ImageData& imageData = m_imageReader->imageData();
        
        // Update image display
        m_imageDisplay->setImageData(imageData);
        
        // Pass image data to background extraction widget
        if (m_backgroundWidget) {
            m_backgroundWidget->setImageData(imageData);
        }
        
        // Update UI state
        m_currentFilePath = filePath;
        updateImageInfo();
        
        // Enable menu actions
        m_saveAsAction->setEnabled(true);
	//        m_extractBackgroundAction->setEnabled(true);
        
        logMessage(QString("✓ Loaded image: %1").arg(QFileInfo(filePath).fileName()));
        m_statusLabel->setText(QString("Image loaded: %1×%2×%3")
                              .arg(imageData.width)
                              .arg(imageData.height)
                              .arg(imageData.channels));
    } else {
        // Clear displays
        m_imageDisplay->clearImage();
        if (m_backgroundWidget) {
            m_backgroundWidget->clearImage();
        }
        
        // Show error
        QString error = m_imageReader->lastError();
        QMessageBox::critical(this, "Error Loading Image", 
                             QString("Failed to load image:\n%1").arg(error));
        
        logMessage(QString("✗ Failed to load image: %1").arg(error));
        
        // Disable menu actions
        m_saveAsAction->setEnabled(false);
        
        m_statusLabel->setText("Ready - No image loaded");
    }
}

void MainWindow::updateImageInfo()
{
    if (!m_imageReader->hasImage()) {
        m_infoText->setPlainText("No image loaded");
        return;
    }
    
    const ImageData& imageData = m_imageReader->imageData();
    
    QString info;
    info += QString("File: %1\n").arg(QFileInfo(m_currentFilePath).fileName());
    info += QString("Dimensions: %1 × %2 × %3\n").arg(imageData.width).arg(imageData.height).arg(imageData.channels);
    info += QString("Format: %1\n").arg(imageData.format);
    info += QString("Color Space: %1\n").arg(imageData.colorSpace);
    info += QString("Pixels: %1\n").arg(imageData.pixels.size());
    
    if (!imageData.metadata.isEmpty()) {
        info += "\nMetadata:\n";
        for (const QString& meta : imageData.metadata) {
            info += QString("  %1\n").arg(meta);
        }
    }
    
    m_infoText->setPlainText(info);
}

void MainWindow::logMessage(const QString& message)
{
    QString timestamp = QTime::currentTime().toString("hh:mm:ss");
    m_logText->append(QString("[%1] %2").arg(timestamp, message));
}

void MainWindow::onBackgroundExtracted(const BackgroundExtractionResult& result)
{
    if (result.success) {
        logMessage(QString("✓ Background extraction completed: %1 samples used, RMS error: %2")
                  .arg(result.samplesUsed)
                  .arg(result.rmsError, 0, 'f', 6));
        
        // Switch to results tab to show statistics
        if (m_backgroundWidget) {
            // The background widget will handle displaying results
        }
        
        // Enable background-related menu actions
        
    } else {
        logMessage(QString("✗ Background extraction failed: %1").arg(result.errorMessage));
    }
}

void MainWindow::onBackgroundModelChanged(const QVector<float>& backgroundData, int width, int height, int channels)
{
    // Store background model for potential display
    logMessage(QString("Background model ready: %1×%2×%3").arg(width).arg(height).arg(channels));
}

void MainWindow::onCorrectedImageReady(const QVector<float>& correctedData, int width, int height, int channels)
{
    // Create corrected image data structure
    ImageData correctedImageData;
    correctedImageData.width = width;
    correctedImageData.height = height;
    correctedImageData.channels = channels;
    correctedImageData.pixels = correctedData;
    correctedImageData.format = "Background Corrected";
    correctedImageData.colorSpace = (channels == 1) ? "Grayscale" : "RGB";
    
    // Update the main image display with corrected data
    m_imageDisplay->setImageData(correctedImageData);
    
    logMessage(QString("✓ Background correction applied to image display"));
}

void MainWindow::onExtractBackground()
{
    if (!m_imageReader->hasImage()) {
        QMessageBox::information(this, "No Image", "Please load an image first.");
        return;
    }
    
    // Switch to background extraction tab
    if (m_tabWidget && m_backgroundTab) {
        int backgroundTabIndex = m_tabWidget->indexOf(m_backgroundTab);
        if (backgroundTabIndex >= 0) {
            m_tabWidget->setCurrentIndex(backgroundTabIndex);
        }
    }
    
    logMessage("Switched to background extraction tab");
}
/*
void MainWindow::onShowBackgroundModel()
{
    if (!m_backgroundWidget || !m_backgroundWidget->hasResult()) {
        QMessageBox::information(this, "No Background Model", 
            "Please extract a background model first.");
        return;
    }
    
    const BackgroundExtractionResult& result = m_backgroundWidget->result();
    
    if (!result.backgroundData.isEmpty() && m_imageReader->hasImage()) {
        const ImageData& originalImage = m_imageReader->imageData();
        createBackgroundImageWindow(result.backgroundData, 
                                   originalImage.width, 
                                   originalImage.height, 
                                   originalImage.channels);
    }
}
*/
void MainWindow::onApplyBackgroundCorrection()
{
    if (!m_backgroundWidget || !m_backgroundWidget->hasResult()) {
        QMessageBox::information(this, "No Background Correction", 
            "Please extract a background model first.");
        return;
    }
    
    const BackgroundExtractionResult& result = m_backgroundWidget->result();
    
    if (!result.correctedData.isEmpty() && m_imageReader->hasImage()) {
        const ImageData& originalImage = m_imageReader->imageData();
        
        // Ask user if they want to replace the current image or create a new window
        QMessageBox::StandardButton reply = QMessageBox::question(this, 
            "Apply Background Correction",
            "Do you want to replace the current image with the background-corrected version?\n\n"
            "Choose 'Yes' to replace current image, 'No' to create a new window, or 'Cancel' to abort.",
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        
        if (reply == QMessageBox::Yes) {
            // Replace current image
            onCorrectedImageReady(result.correctedData, 
                                 originalImage.width, 
                                 originalImage.height, 
                                 originalImage.channels);
        } else if (reply == QMessageBox::No) {
            // Create new window
            createCorrectedImageWindow(result.correctedData,
                                     originalImage.width,
                                     originalImage.height,
                                     originalImage.channels);
        }
        // Cancel does nothing
    }
}

void MainWindow::createCorrectedImageWindow(const QVector<float>& correctedData, int width, int height, int channels)
{
    // Create a new image data structure for the corrected image
    ImageData correctedImageData;
    correctedImageData.width = width;
    correctedImageData.height = height;
    correctedImageData.channels = channels;
    correctedImageData.pixels = correctedData;
    correctedImageData.format = "Background Corrected";
    correctedImageData.colorSpace = (channels == 1) ? "Grayscale" : "RGB";
    correctedImageData.metadata.append("Type: Background Corrected Image");
    correctedImageData.metadata.append(QString("Original Image: %1")
                                      .arg(QFileInfo(m_currentFilePath).fileName()));
    
    // For now, create a simple dialog to show the corrected image
    QDialog* correctedDialog = new QDialog(this, Qt::Window);
    correctedDialog->setWindowTitle("Background Corrected Image");
    correctedDialog->resize(800, 600);
    
    auto* layout = new QVBoxLayout(correctedDialog);
    
    // Create a simplified image display widget for the dialog
    auto* correctedDisplay = new ImageDisplayWidget;
    correctedDisplay->setImageData(correctedImageData);
    layout->addWidget(correctedDisplay);
    
    auto* buttonLayout = new QHBoxLayout;
    auto* saveButton = new QPushButton("Save As...");
    auto* closeButton = new QPushButton("Close");
    
    connect(saveButton, &QPushButton::clicked, [this, correctedImageData]() {
        // Implement save functionality for corrected image
        QString fileName = QFileDialog::getSaveFileName(
            this,
            "Save Background Corrected Image",
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/corrected_image.xisf",
            "XISF Files (*.xisf);;All Files (*)"
        );
        
        if (!fileName.isEmpty()) {
            // Here you would use SimplifiedXISFWriter to save the corrected image
            logMessage("Corrected image would be saved to: " + fileName);
        }
    });
    
    connect(closeButton, &QPushButton::clicked, correctedDialog, &QDialog::accept);
    
    buttonLayout->addWidget(saveButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton);
    
    layout->addLayout(buttonLayout);
    
    correctedDialog->show();
    
    logMessage("✓ Created background corrected image window");
}
