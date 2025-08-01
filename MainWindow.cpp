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
        const ImageData& imageData = m_imageReader->imageData();
        
        if (saveImageData(imageData, fileName, "Original")) {
            logMessage("✓ Successfully saved image to: " + fileName);
            QMessageBox::information(this, "Save Successful", 
                "Image saved successfully!");
        } else {
            logMessage("✗ Failed to save image");
            QMessageBox::critical(this, "Save Failed", 
                "Failed to save image. Check the log for details.");
        }
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
    if (!m_imageReader->hasImage()) {
        return;
    }
    
    // Get original metadata to preserve what we can
    const ImageData& original = m_imageReader->imageData();
    
    // Create new ImageData with corrected pixels but original metadata
    ImageData correctedImage;
    correctedImage.width = width;
    correctedImage.height = height;
    correctedImage.channels = channels;
    correctedImage.pixels = correctedData;
    correctedImage.format = "Background Corrected";
    correctedImage.colorSpace = original.colorSpace;
    
    // Preserve original metadata and add correction info
    correctedImage.metadata = original.metadata;
    correctedImage.metadata.prepend("Processing: Background correction applied");
    correctedImage.metadata.prepend(QString("Correction applied: %1")
                                   .arg(QDateTime::currentDateTime().toString()));
    
    // Clear the reader and create new image data
    m_imageReader->setImageData(correctedImage);
    
    // Update display
    m_imageDisplay->setImageData(correctedImage);
    
    logMessage("✓ Background correction applied - image replaced");
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
        if (saveImageData(correctedImageData, fileName, "Background Corrected")) {
            logMessage("✓ Successfully saved corrected image to: " + fileName);
            QMessageBox::information(this, "Save Successful", 
                "Background corrected image saved successfully!");
        } else {
            logMessage("✗ Failed to save corrected image");
            QMessageBox::critical(this, "Save Failed", 
                "Failed to save corrected image. Check the log for details.");
        }
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

/*
bool MainWindow::saveCorrectedImage(const ImageData& imageData, const QString& fileName)
{
    try {
        SimplifiedXISFWriter writer(fileName, CompressionType::ZLib);
        writer.setCreatorApplication("XISF Test Creator - Background Corrected");
        
        // Add metadata
        writer.addProperty("ProcessingType", "String", "Background Correction", "Type of processing applied");
        writer.addProperty("OriginalFile", "String", QFileInfo(m_currentFilePath).fileName(), "Original image file");
        writer.addProperty("ProcessingDate", "String", QDateTime::currentDateTime().toString(Qt::ISODate), "Processing date");
        
        // Add background extraction statistics if available
        if (m_backgroundWidget && m_backgroundWidget->hasResult()) {
            const BackgroundExtractionResult& result = m_backgroundWidget->result();
            writer.addImageProperty("BackgroundModel", "String", "Polynomial", "Background model used");
            writer.addImageProperty("SamplesUsed", "Int32", result.samplesUsed, "Number of samples used");
            writer.addImageProperty("RMSError", "Float64", result.rmsError, "RMS fitting error");
            writer.addImageProperty("ProcessingTime", "Float64", result.processingTimeSeconds, "Processing time in seconds");
        }
        
        // Add the image
        if (!writer.addImage("corrected_image", imageData.pixels.constData(),
                           imageData.width, imageData.height, imageData.channels)) {
            return false;
        }
        
        return writer.write();
        
    } catch (const std::exception& e) {
        logMessage(QString("Exception saving corrected image: %1").arg(e.what()));
        return false;
    }
}
*/

bool MainWindow::saveImageData(const ImageData& imageData, const QString& fileName, const QString& imageType)
{
    try {
        SimplifiedXISFWriter writer(fileName, CompressionType::ZLib);
        writer.setCreatorApplication("XISF Test Creator");
        
        // Add global properties
        writer.addProperty("SavedBy", "String", "XISF Test Creator", "Application that saved this file");
        writer.addProperty("SaveDate", "String", QDateTime::currentDateTime().toString(Qt::ISODate), "Date saved");
        writer.addProperty("ImageType", "String", imageType, "Type of image");
        
        if (!m_currentFilePath.isEmpty()) {
            writer.addProperty("OriginalFile", "String", QFileInfo(m_currentFilePath).fileName(), "Original source file");
        }
        
        // Add image-specific properties
        writer.addImageProperty("Dimensions", "String", 
            QString("%1x%2x%3").arg(imageData.width).arg(imageData.height).arg(imageData.channels),
            "Image dimensions");
        writer.addImageProperty("Format", "String", imageData.format, "Original format");
        writer.addImageProperty("ColorSpace", "String", imageData.colorSpace, "Color space");
        
        // If this is a corrected image, add background extraction info
        if (imageType == "Background Corrected" && m_backgroundWidget && m_backgroundWidget->hasResult()) {
            const BackgroundExtractionResult& result = m_backgroundWidget->result();
            writer.addImageProperty("BackgroundModel", "String", "Polynomial", "Background model used");
            writer.addImageProperty("SamplesUsed", "Int32", result.samplesUsed, "Number of samples used");
            writer.addImageProperty("RMSError", "Float64", result.rmsError, "RMS fitting error");
            writer.addImageProperty("ProcessingTime", "Float64", result.processingTimeSeconds, "Processing time in seconds");
            
            // Add per-channel results if available
            if (result.usedChannelMode == ChannelMode::PerChannel && !result.channelResults.isEmpty()) {
                writer.addImageProperty("ProcessingMode", "String", "Per-Channel", "Background processing mode");
                writer.addImageProperty("ChannelCount", "Int32", result.channelResults.size(), "Number of channels processed");
                
                for (int i = 0; i < result.channelResults.size(); ++i) {
                    const ChannelResult& ch = result.channelResults[i];
                    writer.addImageProperty(QString("Channel%1_Samples").arg(i), "Int32", ch.samplesUsed, QString("Samples used for channel %1").arg(i));
                    writer.addImageProperty(QString("Channel%1_RMS").arg(i), "Float64", ch.rmsError, QString("RMS error for channel %1").arg(i));
                }
            }
        }
        
        // Copy original metadata if available
        for (const QString& meta : imageData.metadata) {
            // Simple way to preserve some original metadata
            if (meta.contains("WCS:") || meta.contains("Stellina:")) {
                QStringList parts = meta.split(": ");
                if (parts.size() >= 2) {
                    writer.addImageProperty(parts[0], "String", parts[1], "Preserved from original");
                }
            }
        }
        
        // Determine image ID based on type
        QString imageId = (imageType == "Background Corrected") ? "corrected_image" : "main_image";
        
        // Add the image data
        if (!writer.addImage(imageId, imageData.pixels.constData(),
                           imageData.width, imageData.height, imageData.channels)) {
            logMessage("✗ Failed to add image data to XISF writer");
            return false;
        }
        
        // Write the file
        if (!writer.write()) {
            logMessage("✗ XISF write failed: " + writer.lastError());
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logMessage(QString("Exception saving image: %1").arg(e.what()));
        return false;
    }
}
