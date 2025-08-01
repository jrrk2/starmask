#include "MainWindow.h"
#include "ImageReader.h"

#include "PCLMockAPI.h"

// PCL includes
#include <pcl/Image.h>
#include <pcl/XISF.h>
#include <pcl/String.h>
#include <pcl/Property.h>
#include <pcl/api/APIInterface.h>

// NOTE: StarDetector might not be available in mock API
// We'll use our own StarMaskGenerator instead
// #include <pcl/StarDetector.h>

#include <QApplication>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStandardPaths>
#include <QDebug>

MainWindow::~MainWindow() {}

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
        return;
    }

    // Store pointer to image data for star detection
    m_imageData = &m_imageReader->imageData();
    
    m_imageDisplayWidget->setImageData(m_imageReader->imageData());
    m_statusLabel->setText("Image loaded.");
}
// Add this to MainWindow.cpp - update the onDetectStars method:

void MainWindow::onDetectStars()
{
    if (!m_imageReader->hasImage()) {
        m_statusLabel->setText("No image loaded.");
        return;
    }

    m_statusLabel->setText("Detecting stars...");
    QApplication::processEvents();  // Update UI

    // Use our StarMaskGenerator instead of PCL StarDetector
    m_lastStarMask = StarMaskGenerator::detectStars(m_imageReader->imageData(), 0.5f);
    m_imageDisplayWidget->setImageData(m_imageReader->imageData());
    m_imageDisplayWidget->setStarOverlay(m_lastStarMask.starCenters, m_lastStarMask.starRadii);

    // Update status with star count and toggle info
    QString statusText = QString("Detected %1 stars").arg(m_lastStarMask.starCenters.size());
    if (m_lastStarMask.starCenters.size() > 0) {
        statusText += " - Use checkbox to toggle display";
    }
    m_statusLabel->setText(statusText);
}

// Optional: Add this to MainWindow constructor to connect to the star overlay signal:

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_imageReader(std::make_unique<ImageReader>())
    , m_imageData(nullptr)
{
    QWidget* central = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(central);

    m_imageDisplayWidget = new ImageDisplayWidget;
    m_loadButton = new QPushButton("Load Image");
    m_detectButton = new QPushButton("Detect Stars");
    m_statusLabel = new QLabel("Ready");

    layout->addWidget(m_imageDisplayWidget);
    
    // Button layout
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(m_loadButton);
    buttonLayout->addWidget(m_detectButton);
    buttonLayout->addStretch();  // Push buttons to left
    
    layout->addLayout(buttonLayout);
    layout->addWidget(m_statusLabel);

    connect(m_loadButton, &QPushButton::clicked, this, &MainWindow::onLoadImage);
    connect(m_detectButton, &QPushButton::clicked, this, &MainWindow::onDetectStars);
    
    // Connect to star overlay toggle signal for status updates
    connect(m_imageDisplayWidget, &ImageDisplayWidget::starOverlayToggled, 
            this, &MainWindow::onStarOverlayToggled);

    setCentralWidget(central);
    resize(1000, 700);  // Make window a bit larger
    setWindowTitle("Star Mask Demo - Enhanced");
}

// Add this new slot to MainWindow:

void MainWindow::onStarOverlayToggled(bool visible)
{
    if (m_lastStarMask.starCenters.isEmpty()) {
        return;  // No stars to show/hide
    }
    
    QString baseText = QString("Detected %1 stars").arg(m_lastStarMask.starCenters.size());
    QString statusText = baseText + (visible ? " - Stars visible" : " - Stars hidden");
    m_statusLabel->setText(statusText);
}

void MainWindow::runStarDetection()
{
    if (!m_imageData)
        return;

    try {
        // Initialize PCL Mock API
        pcl_mock::InitializeMockAPI();
        if (!API) {
            API = new pcl::APIInterface(pcl_mock::GetMockFunctionResolver());
        }

        // Create a proper PCL Image (not ImageVariant)
        pcl::Image pclImage(m_imageData->width, m_imageData->height, ColorSpace::RGB);
        
        // Copy pixel data properly - PCL Image uses different pixel access
        for (int y = 0; y < m_imageData->height; ++y) {
            for (int x = 0; x < m_imageData->width; ++x) {
                int index = y * m_imageData->width + x;
                if (index < m_imageData->pixels.size()) {
                    // Use the correct PCL Image pixel access method
                    pclImage.Pixel(x, y) = m_imageData->pixels[index];
                }
            }
        }

        // Try to use PCL StarDetector if available
        // NOTE: This may not work with the mock API
        #ifdef HAS_PCL_STAR_DETECTOR
        pcl::StarDetector detector;
        detector.SetStructureLayers(5);
        detector.SetHotPixelFilterRadius(1);
        detector.SetSensitivity(0.5f);
        detector.SetMinStructureSize(4);
        detector.SetNoiseLayers(1);

        // Convert back to ImageVariant for StarDetector
        pcl::ImageVariant imageVar;
        imageVar = pclImage;  // This should work for assignment
        
        auto stars = detector.DetectStars(imageVar);

        QVector<QPoint> starCenters;
        QVector<float> starRadii;
        for (const auto& s : stars) {
            starCenters.append(QPoint(static_cast<int>(std::round(s.pos.x)),
                                      static_cast<int>(std::round(s.pos.y))));
            starRadii.append(std::sqrt(s.area / 3.1416f));
        }

        m_imageDisplayWidget->setStarOverlay(starCenters, starRadii);
        #else
        // Fallback: Use our own star detection
        StarMaskResult result = StarMaskGenerator::detectStars(*m_imageData, 0.5f);
        m_imageDisplayWidget->setStarOverlay(result.starCenters, result.starRadii);
        #endif
        
    } catch (const std::exception& e) {
        qDebug() << "Star detection failed:" << e.what();
        m_statusLabel->setText("Star detection failed");
    }
}
