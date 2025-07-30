#include "MainWindow.h"
#include "SimplifiedXISFWriter.h"
#include "ImageReader.h"
#include "ImageDisplayWidget.h"
#include "ImageStatistics.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QProgressBar>
#include <QTextEdit>
#include <QPixmap>
#include <QPainter>
#include <QTimer>
#include <QThread>
#include <QApplication>
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>
#include <QMenuBar>
#include <QStatusBar>
#include <QTabWidget>
#include <QSplitter>
#include <QFileInfo>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_tabWidget(nullptr)
    , m_previewTimer(new QTimer(this))
    , m_imageReader(std::make_unique<ImageReader>())
    , m_imageStats(std::make_unique<ImageStatistics>())
{
    setupUI();
    setupMenuBar();
    
    // Set up preview timer (debounced updates)
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(300); // 300ms delay
    connect(m_previewTimer, &QTimer::timeout, this, &MainWindow::updatePreview);
    
    // Initial preview
    updatePreview();
    
    logMessage("XISF Test Creator ready!");
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUI()
{
    setWindowTitle("XISF Test Creator - Enhanced");
    setMinimumSize(800, 600);
    resize(1200, 800);
    
    // Create main tab widget
    m_tabWidget = new QTabWidget;
    setCentralWidget(m_tabWidget);
    
    setupCreateTab();
    setupViewTab();
    
    // Status bar
    statusBar()->showMessage("Ready");
}

void MainWindow::setupMenuBar()
{
    // File menu
    QMenu* fileMenu = menuBar()->addMenu("&File");
    
    QAction* openAction = fileMenu->addAction("&Open Image...");
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::onOpenFileClicked);
    
    QAction* saveAsAction = fileMenu->addAction("Save &As...");
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAction, &QAction::triggered, this, &MainWindow::onSaveAsClicked);
    
    fileMenu->addSeparator();
    
    QAction* exitAction = fileMenu->addAction("E&xit");
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
    
    // View menu
    QMenu* viewMenu = menuBar()->addMenu("&View");
    viewMenu->addAction("&Create Tab", [this]() { m_tabWidget->setCurrentIndex(0); });
    viewMenu->addAction("&View Tab", [this]() { m_tabWidget->setCurrentIndex(1); });
}

void MainWindow::setupCreateTab()
{
    m_createTab = new QWidget;
    m_tabWidget->addTab(m_createTab, "Create XISF");
    
    m_createCentralWidget = m_createTab;
    auto* mainLayout = new QVBoxLayout(m_createCentralWidget);
    
    // Image Settings Group
    m_imageGroup = new QGroupBox("Image Settings");
    auto* imageLayout = new QGridLayout(m_imageGroup);
    
    // Width setting
    imageLayout->addWidget(new QLabel("Width:"), 0, 0);
    m_widthSpinBox = new QSpinBox;
    m_widthSpinBox->setRange(64, 2048);
    m_widthSpinBox->setValue(256);
    m_widthSpinBox->setSuffix(" px");
    connect(m_widthSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), 
            this, &MainWindow::onImageSizeChanged);
    imageLayout->addWidget(m_widthSpinBox, 0, 1);
    
    // Height setting
    imageLayout->addWidget(new QLabel("Height:"), 1, 0);
    m_heightSpinBox = new QSpinBox;
    m_heightSpinBox->setRange(64, 2048);
    m_heightSpinBox->setValue(256);
    m_heightSpinBox->setSuffix(" px");
    connect(m_heightSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), 
            this, &MainWindow::onImageSizeChanged);
    imageLayout->addWidget(m_heightSpinBox, 1, 1);
    
    // Compression setting
    imageLayout->addWidget(new QLabel("Compression:"), 2, 0);
    m_compressionCombo = new QComboBox;
    m_compressionCombo->addItems({"None (fastest)", "ZLib (balanced)", "LZ4 (fast)", "ZSTD (best)"});
    m_compressionCombo->setCurrentIndex(1); // ZLib default
    imageLayout->addWidget(m_compressionCombo, 2, 1);
    
    // Preview
    imageLayout->addWidget(new QLabel("Preview:"), 3, 0);
    m_previewLabel = new QLabel;
    m_previewLabel->setFixedSize(128, 128);
    m_previewLabel->setStyleSheet("border: 1px solid gray;");
    m_previewLabel->setAlignment(Qt::AlignCenter);
    imageLayout->addWidget(m_previewLabel, 3, 1);
    
    mainLayout->addWidget(m_imageGroup);
    
    // Action Group
    m_actionGroup = new QGroupBox("Actions");
    auto* actionLayout = new QVBoxLayout(m_actionGroup);
    
    m_createButton = new QPushButton("Create XISF File...");
    m_createButton->setMinimumHeight(40);
    connect(m_createButton, &QPushButton::clicked, this, &MainWindow::onCreateXISFClicked);
    actionLayout->addWidget(m_createButton);
    
    m_progressBar = new QProgressBar;
    m_progressBar->setVisible(false);
    actionLayout->addWidget(m_progressBar);
    
    mainLayout->addWidget(m_actionGroup);
    
    // Log Group
    m_logGroup = new QGroupBox("Log");
    auto* logLayout = new QVBoxLayout(m_logGroup);
    
    m_logTextEdit = new QTextEdit;
    m_logTextEdit->setMaximumHeight(150);
    m_logTextEdit->setReadOnly(true);
    logLayout->addWidget(m_logTextEdit);
    
    mainLayout->addWidget(m_logGroup);
    
    // Set stretch factors
    mainLayout->setStretchFactor(m_imageGroup, 1);
    mainLayout->setStretchFactor(m_actionGroup, 0);
    mainLayout->setStretchFactor(m_logGroup, 0);
}

void MainWindow::setupViewTab()
{
    m_viewTab = new QWidget;
    m_tabWidget->addTab(m_viewTab, "View Images");
    
    auto* viewLayout = new QVBoxLayout(m_viewTab);
    
    // File operations toolbar
    auto* fileToolbar = new QWidget;
    auto* fileLayout = new QHBoxLayout(fileToolbar);
    fileLayout->setContentsMargins(5,5,5,5);
    
    m_openButton = new QPushButton("Open Image...");
    m_openButton->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    connect(m_openButton, &QPushButton::clicked, this, &MainWindow::onOpenFileClicked);
    fileLayout->addWidget(m_openButton);
    
    m_saveAsButton = new QPushButton("Save As...");
    m_saveAsButton->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    m_saveAsButton->setEnabled(false);
    connect(m_saveAsButton, &QPushButton::clicked, this, &MainWindow::onSaveAsClicked);
    fileLayout->addWidget(m_saveAsButton);
    
    fileLayout->addStretch();
    
    m_statusLabel = new QLabel("No image loaded");
    fileLayout->addWidget(m_statusLabel);
    
    viewLayout->addWidget(fileToolbar);
    
    // Main splitter
    m_viewSplitter = new QSplitter(Qt::Horizontal);
    viewLayout->addWidget(m_viewSplitter, 1);
    
    // Image display
    m_imageDisplay = new ImageDisplayWidget;
    connect(m_imageDisplay, &ImageDisplayWidget::imageClicked, 
            this, &MainWindow::onImageClicked);
    connect(m_imageDisplay, &ImageDisplayWidget::zoomChanged,
            this, &MainWindow::onZoomChanged);
    m_viewSplitter->addWidget(m_imageDisplay);
    
    // Info panel
    setupInfoPanel();
    
    // Set splitter proportions
    m_viewSplitter->setStretchFactor(0, 3); // Image display gets more space
    m_viewSplitter->setStretchFactor(1, 1); // Info panel gets less space
    m_viewSplitter->setSizes({800, 300});
}

void MainWindow::setupInfoPanel()
{
    m_infoPanel = new QWidget;
    m_infoPanel->setMinimumWidth(250);
    m_infoPanel->setMaximumWidth(400);
    
    auto* infoPanelLayout = new QVBoxLayout(m_infoPanel);
    
    // File info
    m_fileInfoGroup = new QGroupBox("File Information");
    auto* fileInfoLayout = new QVBoxLayout(m_fileInfoGroup);
    m_fileInfoText = new QTextEdit;
    m_fileInfoText->setMaximumHeight(80);
    m_fileInfoText->setReadOnly(true);
    fileInfoLayout->addWidget(m_fileInfoText);
    infoPanelLayout->addWidget(m_fileInfoGroup);
    
    // Image info
    m_imageInfoGroup = new QGroupBox("Image Information");
    auto* imageInfoLayout = new QVBoxLayout(m_imageInfoGroup);
    m_imageInfoText = new QTextEdit;
    m_imageInfoText->setMaximumHeight(100);
    m_imageInfoText->setReadOnly(true);
    imageInfoLayout->addWidget(m_imageInfoText);
    infoPanelLayout->addWidget(m_imageInfoGroup);
    
    // Statistics
    m_statisticsGroup = new QGroupBox("Image Statistics");
    auto* statisticsLayout = new QVBoxLayout(m_statisticsGroup);
    m_statisticsText = new QTextEdit;
    m_statisticsText->setMaximumHeight(120);
    m_statisticsText->setReadOnly(true);
    statisticsLayout->addWidget(m_statisticsText);
    infoPanelLayout->addWidget(m_statisticsGroup);
    
    // Pixel info
    m_pixelInfoGroup = new QGroupBox("Pixel Information");
    auto* pixelInfoLayout = new QVBoxLayout(m_pixelInfoGroup);
    m_pixelInfoText = new QTextEdit;
    m_pixelInfoText->setMaximumHeight(60);
    m_pixelInfoText->setReadOnly(true);
    pixelInfoLayout->addWidget(m_pixelInfoText);
    infoPanelLayout->addWidget(m_pixelInfoGroup);
    
    // Metadata
    m_metadataGroup = new QGroupBox("Metadata");
    auto* metadataLayout = new QVBoxLayout(m_metadataGroup);
    m_metadataText = new QTextEdit;
    m_metadataText->setReadOnly(true);
    metadataLayout->addWidget(m_metadataText);
    infoPanelLayout->addWidget(m_metadataGroup);
    
    // Set stretch factors so metadata gets most space
    infoPanelLayout->setStretchFactor(m_fileInfoGroup, 0);
    infoPanelLayout->setStretchFactor(m_imageInfoGroup, 0);
    infoPanelLayout->setStretchFactor(m_statisticsGroup, 0);
    infoPanelLayout->setStretchFactor(m_pixelInfoGroup, 0);
    infoPanelLayout->setStretchFactor(m_metadataGroup, 1);
    
    m_viewSplitter->addWidget(m_infoPanel);
}

void MainWindow::onOpenFileClicked()
{
    QString filter = ImageReader::formatFilter();
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Open Image File",
        m_currentFilePath.isEmpty() ? 
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) : 
            QFileInfo(m_currentFilePath).absolutePath(),
        filter
    );
    
    if (!fileName.isEmpty()) {
        loadImageFile(fileName);
        // Switch to view tab
        m_tabWidget->setCurrentIndex(1);
    }
}

void MainWindow::onSaveAsClicked()
{
    if (!m_imageReader->hasImage()) {
        QMessageBox::information(this, "No Image", "No image is currently loaded to save.");
        return;
    }
    
    // For now, just show info about the current image
    // In a full implementation, you would implement saving in various formats
    QMessageBox::information(this, "Save As", 
        "Save functionality would be implemented here.\n"
        "Current image could be saved as XISF, FITS, or other formats.");
}

void MainWindow::loadImageFile(const QString& filePath)
{
    statusBar()->showMessage("Loading image...");
    QApplication::processEvents();
    
    m_imageReader->clear();
    
    if (m_imageReader->readFile(filePath)) {
        m_currentFilePath = filePath;
        const ImageData& imageData = m_imageReader->imageData();
        
        // Update displays
        m_imageDisplay->setImageData(imageData);
        updateImageInfo();
        updateImageStatistics();
        
        m_saveAsButton->setEnabled(true);
        
        QString message = QString("Loaded: %1 (%2×%3×%4)")
                         .arg(QFileInfo(filePath).fileName())
                         .arg(imageData.width)
                         .arg(imageData.height) 
                         .arg(imageData.channels);
        statusBar()->showMessage(message);
        m_statusLabel->setText(message);
        
        logMessage(QString("✓ Successfully loaded: %1").arg(QFileInfo(filePath).fileName()));
        
    } else {
        QString error = QString("Failed to load image: %1").arg(m_imageReader->lastError());
        statusBar()->showMessage("Failed to load image");
        m_statusLabel->setText("Load failed");
        
        QMessageBox::critical(this, "Load Error", error);
        logMessage(QString("✗ %1").arg(error));
    }
}

void MainWindow::updateImageInfo()
{
    if (!m_imageReader->hasImage()) {
        m_fileInfoText->clear();
        m_imageInfoText->clear();
        m_metadataText->clear();
        return;
    }
    
    const ImageData& imageData = m_imageReader->imageData();
    
    // File information
    QFileInfo fileInfo(m_currentFilePath);
    QString fileInfoStr = QString(
        "Name: %1\n"
        "Size: %2 bytes\n"
        "Modified: %3"
    ).arg(fileInfo.fileName())
     .arg(fileInfo.size())
     .arg(fileInfo.lastModified().toString());
    m_fileInfoText->setPlainText(fileInfoStr);
    
    // Image information
    QString imageInfoStr = QString(
        "Dimensions: %1 × %2\n"
        "Channels: %3\n"
        "Color Space: %4\n"
        "Format: %5\n"
        "Total Pixels: %6"
    ).arg(imageData.width)
     .arg(imageData.height)
     .arg(imageData.channels)
     .arg(imageData.colorSpace)
     .arg(imageData.format)
     .arg(imageData.width * imageData.height);
    m_imageInfoText->setPlainText(imageInfoStr);
    
    // Metadata
    m_metadataText->setPlainText(imageData.metadata.join("\n"));
}

void MainWindow::updateImageStatistics()
{
    if (!m_imageReader->hasImage()) {
        m_statisticsText->clear();
        return;
    }
    
    const ImageData& imageData = m_imageReader->imageData();
    
    m_imageStats->calculate(imageData.pixels.constData(), imageData.pixels.size());
    m_statisticsText->setPlainText(m_imageStats->toDetailedString());
}

void MainWindow::onImageClicked(int x, int y, float value)
{
    if (!m_imageReader->hasImage()) {
        return;
    }
    
    const ImageData& imageData = m_imageReader->imageData();
    
    QString pixelInfo;
    if (imageData.channels == 1) {
        pixelInfo = QString("Position: (%1, %2)\nValue: %3")
                   .arg(x).arg(y).arg(value, 0, 'g', 6);
    } else {
        // For multi-channel images, show values for all channels at this pixel
        int pixelIndex = y * imageData.width + x;
        QStringList channelValues;
        
        for (int c = 0; c < imageData.channels; ++c) {
            int channelPixelIndex = pixelIndex + c * imageData.width * imageData.height;
            if (channelPixelIndex < imageData.pixels.size()) {
                float channelValue = imageData.pixels[channelPixelIndex];
                channelValues.append(QString("Ch%1: %2").arg(c).arg(channelValue, 0, 'g', 6));
            }
        }
        
        pixelInfo = QString("Position: (%1, %2)\n%3")
                   .arg(x).arg(y).arg(channelValues.join("\n"));
    }
    
    m_pixelInfoText->setPlainText(pixelInfo);
}

void MainWindow::onZoomChanged(double factor)
{
    statusBar()->showMessage(QString("Zoom: %1%").arg(static_cast<int>(factor * 100)), 2000);
}

void MainWindow::onImageSizeChanged()
{
    // Restart the timer for debounced preview updates
    m_previewTimer->start();
}

void MainWindow::updatePreview()
{
    int width = m_widthSpinBox->value();
    int height = m_heightSpinBox->value();
    
    // Create a small preview version (max 128x128)
    int previewSize = 128;
    int previewWidth = qMin(width, previewSize);
    int previewHeight = qMin(height, previewSize);
    
    QPixmap preview(previewWidth, previewHeight);
    preview.fill(Qt::black);
    
    QPainter painter(&preview);
    
    // Create the same test pattern as the original code
    for (int y = 0; y < previewHeight; ++y) {
        for (int x = 0; x < previewWidth; ++x) {
            // Scale coordinates to match the full image
            int fullX = (x * width) / previewWidth;
            int fullY = (y * height) / previewHeight;
            
            float value = float(fullX ^ fullY) / 255.0f;
            int grayValue = qBound(0, int(value * 255), 255);
            
            painter.setPen(QColor(grayValue, grayValue, grayValue));
            painter.drawPoint(x, y);
        }
    }
    
    m_previewLabel->setPixmap(preview);
    
    // Update the status
    QString sizeText = QString("%1 × %2 pixels").arg(width).arg(height);
    m_previewLabel->setToolTip(sizeText);
}

void MainWindow::onCreateXISFClicked()
{
    // Get save location
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString fileName = QFileDialog::getSaveFileName(
        this,
        "Save XISF File",
        QDir(defaultPath).filePath("test_pattern.xisf"),
        "XISF Files (*.xisf);;All Files (*)"
    );
    
    if (fileName.isEmpty()) {
        return; // User cancelled
    }
    
    // Disable UI during creation
    m_createButton->setEnabled(false);
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 0); // Indeterminate
    
    logMessage(QString("Creating XISF file: %1").arg(QFileInfo(fileName).fileName()));
    
    QApplication::processEvents(); // Update UI
    
    try {
        int width = m_widthSpinBox->value();
        int height = m_heightSpinBox->value();
        
        logMessage(QString("Generating %1×%2 test pattern...").arg(width).arg(height));
        
        // Create test image data
        QVector<float> pixels(width * height);
        createTestImage(pixels.data(), width, height);
        
        logMessage("Creating XISF writer...");
        
        // Convert compression combo selection to enum
        CompressionType compression;
        switch (m_compressionCombo->currentIndex()) {
            case 0: compression = CompressionType::None; break;
            case 1: compression = CompressionType::ZLib; break;
            case 2: compression = CompressionType::LZ4; break;
            case 3: compression = CompressionType::ZSTD; break;
            default: compression = CompressionType::ZLib; break;
        }
        
        SimplifiedXISFWriter writer(fileName, compression);
        writer.setCreatorApplication("XISF Test Creator - Enhanced");
        writer.setVerbosity(0); // Quiet mode for GUI
        
        // Add some metadata
        writer.addProperty("TestPattern", "String", "XOR Pattern", "Type of test pattern");
        writer.addProperty("ImageDimensions", "String", QString("%1×%2").arg(width).arg(height), "Image size");
        writer.addProperty("Compression", "String", getCompressionName(m_compressionCombo->currentIndex()), "Compression method");
        writer.addProperty("CreationTime", "String", QDateTime::currentDateTimeUtc().toString(Qt::ISODate), "File creation time");
        
        logMessage("Adding image data...");
        
        if (!writer.addImage("test_pattern", pixels.data(), width, height, 1)) {
            throw std::runtime_error(writer.lastError().toStdString());
        }
        
        logMessage("Writing XISF file...");
        
        if (!writer.write()) {
            throw std::runtime_error(writer.lastError().toStdString());
        }
        
        logMessage("✓ XISF file created successfully!");
        
        QMessageBox::information(this, "Success", 
            QString("XISF file created successfully!\n\nFile: %1\nSize: %2×%3 pixels\nCompression: %4")
            .arg(QFileInfo(fileName).fileName())
            .arg(width).arg(height)
            .arg(getCompressionName(m_compressionCombo->currentIndex())));
        
    } catch (const std::exception& e) {
        QString errorMsg = QString("Error creating XISF file: %1").arg(e.what());
        logMessage("✗ " + errorMsg);
        QMessageBox::critical(this, "Error", errorMsg);
    }
    
    // Re-enable UI
    m_createButton->setEnabled(true);
    m_progressBar->setVisible(false);
}

void MainWindow::createTestImage(float* pixels, int width, int height)
{
    // Create the same XOR pattern as the original code
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            pixels[y * width + x] = float(x ^ y) / 255.0f;
        }
    }
}

QString MainWindow::getCompressionName(int index)
{
    switch (index) {
        case 0: return "None";
        case 1: return "ZLib";
        case 2: return "LZ4";
        case 3: return "ZSTD";
        default: return "Unknown";
    }
}

void MainWindow::logMessage(const QString& message)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    m_logTextEdit->append(QString("[%1] %2").arg(timestamp, message));
    
    // Auto-scroll to bottom
    QTextCursor cursor = m_logTextEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_logTextEdit->setTextCursor(cursor);
    
    QApplication::processEvents(); // Update UI immediately
}
