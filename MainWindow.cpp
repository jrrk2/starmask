#include "MainWindow.h"
#include "SimplifiedXISFWriter.h"

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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_centralWidget(nullptr)
    , m_previewTimer(new QTimer(this))
{
    setupUI();
    
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
    setWindowTitle("XISF Test Creator");
    setMinimumSize(600, 500);
    resize(800, 600);
    
    m_centralWidget = new QWidget;
    setCentralWidget(m_centralWidget);
    
    auto* mainLayout = new QVBoxLayout(m_centralWidget);
    
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
        writer.setCreatorApplication("XISF Test Creator");
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

#include "MainWindow.moc"