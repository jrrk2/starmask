#include "ImageDisplayWidget.h"
#include "ImageReader.h"
#include "ImageStatistics.h"

#include <QPixmap>
#include <QImage>
#include <QPainter>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QDebug>
#include <QGroupBox>
#include <QCheckBox>
#include <algorithm>
#include <cmath>

ImageDisplayWidget::ImageDisplayWidget(QWidget *parent)
    : QWidget(parent)
    , m_imageData(nullptr)
    , m_zoomFactor(1.0)
    , m_autoStretchEnabled(true)
    , m_stretchMin(0.0)
    , m_stretchMax(1.0)
    , m_imageMin(0.0)
    , m_imageMax(1.0)
    , m_imageMean(0.5)
    , m_imageStdDev(0.0)
{
    setupUI();
    updateZoomControls();
}

ImageDisplayWidget::~ImageDisplayWidget() = default;
// Update the setupUI() method in ImageDisplayWidget.cpp to add the star toggle:

void ImageDisplayWidget::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    
    // Control panel
    auto* controlGroup = new QGroupBox("Display Controls");
    m_controlLayout = new QHBoxLayout(controlGroup);
    
    // Zoom controls
    m_zoomInButton = new QPushButton("Zoom In");
    m_zoomOutButton = new QPushButton("Zoom Out");
    m_zoomFitButton = new QPushButton("Fit");
    m_zoom100Button = new QPushButton("100%");
    m_zoomLabel = new QLabel("100%");
    m_zoomLabel->setMinimumWidth(60);
    m_zoomLabel->setAlignment(Qt::AlignCenter);
    
    connect(m_zoomInButton, &QPushButton::clicked, this, &ImageDisplayWidget::onZoomInClicked);
    connect(m_zoomOutButton, &QPushButton::clicked, this, &ImageDisplayWidget::onZoomOutClicked);
    connect(m_zoomFitButton, &QPushButton::clicked, this, &ImageDisplayWidget::onZoomFitClicked);
    connect(m_zoom100Button, &QPushButton::clicked, this, &ImageDisplayWidget::onZoom100Clicked);
    
    m_controlLayout->addWidget(m_zoomOutButton);
    m_controlLayout->addWidget(m_zoomInButton);
    m_controlLayout->addWidget(m_zoomFitButton);
    m_controlLayout->addWidget(m_zoom100Button);
    m_controlLayout->addWidget(m_zoomLabel);
    
    // Add separator
    auto* separator1 = new QFrame();
    separator1->setFrameShape(QFrame::VLine);
    separator1->setFrameShadow(QFrame::Sunken);
    m_controlLayout->addWidget(separator1);
    
    // NEW: Star overlay toggle
    m_showStarsCheck = new QCheckBox("Show Stars");
    m_showStarsCheck->setChecked(m_showStars);  // Use current state
    m_showStarsCheck->setToolTip("Toggle star detection overlay");
    connect(m_showStarsCheck, &QCheckBox::toggled, this, &ImageDisplayWidget::onShowStarsToggled);
    m_controlLayout->addWidget(m_showStarsCheck);
    
    // Add another separator
    auto* separator2 = new QFrame();
    separator2->setFrameShape(QFrame::VLine);
    separator2->setFrameShadow(QFrame::Sunken);
    m_controlLayout->addWidget(separator2);
    
    m_controlLayout->addStretch();
    
    // Stretch controls
    m_autoStretchButton = new QPushButton("Auto Stretch");
    m_autoStretchButton->setCheckable(true);
    m_autoStretchButton->setChecked(m_autoStretchEnabled);
    connect(m_autoStretchButton, &QPushButton::toggled, this, &ImageDisplayWidget::onAutoStretchToggled);
    
    m_minLabel = new QLabel("Min:");
    m_minSlider = new QSlider(Qt::Horizontal);
    m_minSlider->setRange(0, 1000);
    m_minSlider->setValue(0);
    m_minSpinBox = new QSpinBox;
    m_minSpinBox->setRange(0, 100);
    m_minSpinBox->setValue(0);
    m_minSpinBox->setSuffix("%");
    
    m_maxLabel = new QLabel("Max:");
    m_maxSlider = new QSlider(Qt::Horizontal);
    m_maxSlider->setRange(0, 1000);
    m_maxSlider->setValue(1000);
    m_maxSpinBox = new QSpinBox;
    m_maxSpinBox->setRange(0, 100);
    m_maxSpinBox->setValue(100);
    m_maxSpinBox->setSuffix("%");
    
    connect(m_minSlider, &QSlider::valueChanged, this, &ImageDisplayWidget::onStretchChanged);
    connect(m_maxSlider, &QSlider::valueChanged, this, &ImageDisplayWidget::onStretchChanged);
    connect(m_minSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &ImageDisplayWidget::onStretchChanged);
    connect(m_maxSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &ImageDisplayWidget::onStretchChanged);
    
    m_controlLayout->addWidget(m_autoStretchButton);
    m_controlLayout->addWidget(m_minLabel);
    m_controlLayout->addWidget(m_minSlider);
    m_controlLayout->addWidget(m_minSpinBox);
    m_controlLayout->addWidget(m_maxLabel);
    m_controlLayout->addWidget(m_maxSlider);
    m_controlLayout->addWidget(m_maxSpinBox);
    
    m_mainLayout->addWidget(controlGroup);
    
    // Image display area
    m_scrollArea = new QScrollArea;
    m_scrollArea->setBackgroundRole(QPalette::Dark);
    m_scrollArea->setAlignment(Qt::AlignCenter);
    
    m_imageLabel = new QLabel;
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setMinimumSize(200, 200);
    m_imageLabel->setText("No image loaded");
    m_imageLabel->setStyleSheet("border: 1px solid gray; background-color: #2b2b2b; color: white;");
    
    m_scrollArea->setWidget(m_imageLabel);
    m_mainLayout->addWidget(m_scrollArea, 1);
    
    // Initial state
    onAutoStretchToggled(m_autoStretchEnabled);
}

// Add the new slot method:

void ImageDisplayWidget::onShowStarsToggled(bool show)
{
    m_showStars = show;
    qDebug() << "Star overlay toggled:" << (show ? "ON" : "OFF");
    updateDisplay();  // Refresh the display
    
    // Emit signal so other components can react if needed
    emit starOverlayToggled(show);
}

// Add the public method for programmatic control:

void ImageDisplayWidget::setStarOverlayVisible(bool visible)
{
    if (m_showStars != visible) {
        m_showStars = visible;
        m_showStarsCheck->setChecked(visible);  // Update the checkbox
        updateDisplay();
    }
}

// Update the setStarOverlay method to enable the checkbox:

void ImageDisplayWidget::setStarOverlay(const QVector<QPoint>& centers, const QVector<float>& radii)
{
    m_starCenters = centers;
    m_starRadii = radii;
    
    // If we have stars, enable the checkbox and show them by default
    if (!centers.isEmpty()) {
        m_showStars = true;
        m_showStarsCheck->setChecked(true);
        m_showStarsCheck->setEnabled(true);  // Enable the checkbox
    } else {
        m_showStarsCheck->setEnabled(false);  // Disable if no stars
    }
    
    updateDisplay();
}

// Update clearStarOverlay method:

void ImageDisplayWidget::clearStarOverlay()
{
    m_starCenters.clear();
    m_starRadii.clear();
    m_showStars = false;
    m_showStarsCheck->setChecked(false);
    m_showStarsCheck->setEnabled(false);  // Disable checkbox when no stars
    updateDisplay();
}

void ImageDisplayWidget::setImageData(const ImageData& imageData)
{
    m_ownedImageData = std::make_unique<ImageData>(imageData);
    m_imageData = m_ownedImageData.get();
    
    if (!imageData.isValid()) {
        clearImage();
        return;
    }
    
    // Calculate image statistics
    ImageStatistics stats;
    stats.calculate(imageData.pixels.constData(), imageData.pixels.size());
    
    m_imageMin = stats.minimum();
    m_imageMax = stats.maximum();
    m_imageMean = stats.mean();
    m_imageStdDev = stats.standardDeviation();
    
    // Set default stretch limits
    if (m_autoStretchEnabled) {
        // Use mean ± 2.5 sigma for auto stretch
        double range = 2.5 * m_imageStdDev;
        m_stretchMin = std::max(m_imageMin, m_imageMean - range);
        m_stretchMax = std::min(m_imageMax, m_imageMean + range);
    } else {
        m_stretchMin = m_imageMin;
        m_stretchMax = m_imageMax;
    }
    
    updateDisplay();
    onZoomFitClicked(); // Fit image to window
}

void ImageDisplayWidget::clearImage()
{
    m_ownedImageData.reset();
    m_imageData = nullptr;
    m_currentPixmap = QPixmap();
    m_imageLabel->setPixmap(QPixmap());
    m_imageLabel->setText("No image loaded");
    
    m_imageMin = 0.0;
    m_imageMax = 1.0;
    m_imageMean = 0.5;
    m_imageStdDev = 0.0;
}

void ImageDisplayWidget::setZoomFactor(double factor)
{
    factor = std::max(0.1, std::min(10.0, factor));
    if (std::abs(factor - m_zoomFactor) > 0.001) {
        m_zoomFactor = factor;
        updateDisplay();
        updateZoomControls();
        emit zoomChanged(factor);
    }
}

double ImageDisplayWidget::zoomFactor() const
{
    return m_zoomFactor;
}

void ImageDisplayWidget::setAutoStretch(bool enabled)
{
    if (enabled != m_autoStretchEnabled) {
        m_autoStretchEnabled = enabled;
        m_autoStretchButton->setChecked(enabled);
        onAutoStretchToggled(enabled);
        if (m_imageData) {
            updateDisplay();
        }
    }
}

bool ImageDisplayWidget::autoStretch() const
{
    return m_autoStretchEnabled;
}

void ImageDisplayWidget::setStretchLimits(double minValue, double maxValue)
{
    m_stretchMin = minValue;
    m_stretchMax = maxValue;
    
    // Update UI controls
    double minPercent = (minValue - m_imageMin) / (m_imageMax - m_imageMin) * 100.0;
    double maxPercent = (maxValue - m_imageMin) / (m_imageMax - m_imageMin) * 100.0;
    
    m_minSpinBox->setValue(static_cast<int>(minPercent));
    m_maxSpinBox->setValue(static_cast<int>(maxPercent));
    m_minSlider->setValue(static_cast<int>(minPercent * 10));
    m_maxSlider->setValue(static_cast<int>(maxPercent * 10));
    
    if (m_imageData) {
        updateDisplay();
    }
}

void ImageDisplayWidget::getStretchLimits(double& minValue, double& maxValue) const
{
    minValue = m_stretchMin;
    maxValue = m_stretchMax;
}

void ImageDisplayWidget::wheelEvent(QWheelEvent* event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        // Zoom with Ctrl+Wheel
        double scaleFactor = event->angleDelta().y() > 0 ? 1.25 : 0.8;
        setZoomFactor(m_zoomFactor * scaleFactor);
        event->accept();
    } else {
        QWidget::wheelEvent(event);
    }
}

void ImageDisplayWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_imageData && !m_currentPixmap.isNull()) {
        // Calculate image coordinates
        QPoint labelPos = event->pos() - m_imageLabel->pos();
        QSize pixmapSize = m_currentPixmap.size();
        QSize labelSize = m_imageLabel->size();
        
        // Account for centering
        int offsetX = (labelSize.width() - pixmapSize.width()) / 2;
        int offsetY = (labelSize.height() - pixmapSize.height()) / 2;
        
        int pixmapX = labelPos.x() - offsetX;
        int pixmapY = labelPos.y() - offsetY;
        
        // Convert to original image coordinates
        int imageX = static_cast<int>(pixmapX / m_zoomFactor);
        int imageY = static_cast<int>(pixmapY / m_zoomFactor);
        
        if (imageX >= 0 && imageX < m_imageData->width && 
            imageY >= 0 && imageY < m_imageData->height) {
            
            int pixelIndex = imageY * m_imageData->width + imageX;
            float pixelValue = m_imageData->pixels[pixelIndex];
            
            emit imageClicked(imageX, imageY, pixelValue);
        }
    }
    
    QWidget::mousePressEvent(event);
}

void ImageDisplayWidget::onZoomInClicked()
{
    setZoomFactor(m_zoomFactor * 1.5);
}

void ImageDisplayWidget::onZoomOutClicked()
{
    setZoomFactor(m_zoomFactor / 1.5);
}

void ImageDisplayWidget::onZoomFitClicked()
{
    if (!m_imageData || !m_imageData->isValid()) {
        return;
    }
    
    QSize availableSize = m_scrollArea->viewport()->size();
    double scaleX = double(availableSize.width()) / m_imageData->width;
    double scaleY = double(availableSize.height()) / m_imageData->height;
    double scale = std::min(scaleX, scaleY) * 0.9; // Leave some margin
    
    setZoomFactor(scale);
}

void ImageDisplayWidget::onZoom100Clicked()
{
    setZoomFactor(1.0);
}

void ImageDisplayWidget::onAutoStretchToggled(bool enabled)
{
    m_autoStretchEnabled = enabled;
    
    // Enable/disable manual stretch controls
    m_minLabel->setEnabled(!enabled);
    m_minSlider->setEnabled(!enabled);
    m_minSpinBox->setEnabled(!enabled);
    m_maxLabel->setEnabled(!enabled);
    m_maxSlider->setEnabled(!enabled);
    m_maxSpinBox->setEnabled(!enabled);
    
    if (enabled && m_imageData) {
        // Auto-calculate stretch limits
        double range = 2.5 * m_imageStdDev;
        m_stretchMin = std::max(m_imageMin, m_imageMean - range);
        m_stretchMax = std::min(m_imageMax, m_imageMean + range);
        updateDisplay();
    }
}

void ImageDisplayWidget::onStretchChanged()
{
    if (m_autoStretchEnabled) {
        return; // Ignore manual changes when auto stretch is enabled
    }
    
    // Get values from spinboxes (they're the primary controls)
    double minPercent = m_minSpinBox->value() / 100.0;
    double maxPercent = m_maxSpinBox->value() / 100.0;
    
    // Update sliders to match
    m_minSlider->setValue(static_cast<int>(minPercent * 1000));
    m_maxSlider->setValue(static_cast<int>(maxPercent * 1000));
    
    // Calculate actual values
    m_stretchMin = m_imageMin + minPercent * (m_imageMax - m_imageMin);
    m_stretchMax = m_imageMin + maxPercent * (m_imageMax - m_imageMin);
    
    if (m_imageData) {
        updateDisplay();
    }
}

void ImageDisplayWidget::updateDisplay()
{
    if (!m_imageData || !m_imageData->isValid()) {
        m_imageLabel->clear();
        m_imageLabel->setText("No image loaded");
        return;
    }
    
    qDebug() << "Updating display - zoom factor:" << m_zoomFactor;
    
    m_currentPixmap = createPixmapFromImageData();
    
    if (m_currentPixmap.isNull()) {
        qDebug() << "Failed to create pixmap from image data";
        return;
    }
    
    qDebug() << "Original pixmap size:" << m_currentPixmap.size();
    
    // Apply zoom scaling
    QSize scaledSize = m_currentPixmap.size() * m_zoomFactor;
    qDebug() << "Scaled size:" << scaledSize;
    
    QPixmap displayPixmap;
    if (std::abs(m_zoomFactor - 1.0) > 0.001) {
        // Only scale if zoom factor is not 1.0
        displayPixmap = m_currentPixmap.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    } else {
        displayPixmap = m_currentPixmap;
    }
    
    // Handle star overlay
    if (m_showStars && !m_starCenters.isEmpty()) {
        qDebug() << "Drawing star overlay with" << m_starCenters.size() << "stars";
        
        QPixmap overlayPixmap = displayPixmap;  // Work with the scaled pixmap
        QPainter painter(&overlayPixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(Qt::green, 2));  // Make stars more visible
        
        // Scale factors for star positions
        double xScale = double(displayPixmap.width()) / m_imageData->width;
        double yScale = double(displayPixmap.height()) / m_imageData->height;
        
        qDebug() << "Star overlay scale factors:" << xScale << yScale;
        
        for (int i = 0; i < m_starCenters.size() && i < m_starRadii.size(); ++i) {
            const QPoint& pt = m_starCenters[i];
            float r = m_starRadii[i];
            
            // Scale star position and radius
            QPointF scaledCenter(pt.x() * xScale, pt.y() * yScale);
            double scaledRadius = r * std::min(xScale, yScale);
            
            QRectF ellipse(scaledCenter.x() - scaledRadius,
                          scaledCenter.y() - scaledRadius,
                          2 * scaledRadius,
                          2 * scaledRadius);
            
            painter.drawEllipse(ellipse);
            
            // Draw center point
            painter.drawPoint(scaledCenter);
        }
        
        painter.end();
        m_imageLabel->setPixmap(overlayPixmap);
    } else {
        m_imageLabel->setPixmap(displayPixmap);
    }
    
    // Update label size to match pixmap
    m_imageLabel->resize(displayPixmap.size());
    
    qDebug() << "Display updated - label size:" << m_imageLabel->size() << "pixmap size:" << displayPixmap.size();
}

void ImageDisplayWidget::updateZoomControls()
{
    m_zoomLabel->setText(QString("%1%").arg(static_cast<int>(m_zoomFactor * 100)));
    
    m_zoomInButton->setEnabled(m_zoomFactor < 10.0);
    m_zoomOutButton->setEnabled(m_zoomFactor > 0.1);
}

QPixmap ImageDisplayWidget::createPixmapFromImageData()
{
    if (!m_imageData || !m_imageData->isValid()) {
        return QPixmap();
    }
    
    const int width = m_imageData->width;
    const int height = m_imageData->height;
    const int channels = m_imageData->channels;
    
    // Create stretched image data
    QVector<float> stretchedData(m_imageData->pixels.size());
    stretchImageData(m_imageData->pixels.constData(), stretchedData.data(), 
                    m_imageData->pixels.size(), m_stretchMin, m_stretchMax);
    
    QImage image(width, height, QImage::Format_RGB32);
    
    if (channels == 1) {
        // Grayscale image
        for (int y = 0; y < height; ++y) {
            QRgb* scanLine = reinterpret_cast<QRgb*>(image.scanLine(y));
            for (int x = 0; x < width; ++x) {
                float value = stretchedData[y * width + x];
                int gray = static_cast<int>(std::clamp(value * 255.0f, 0.0f, 255.0f));
                scanLine[x] = qRgb(gray, gray, gray);
            }
        }
    } else if (channels >= 3) {
        // RGB image (use first 3 channels)
        for (int y = 0; y < height; ++y) {
            QRgb* scanLine = reinterpret_cast<QRgb*>(image.scanLine(y));
            for (int x = 0; x < width; ++x) {
                int pixelIndex = y * width + x;
                float r = stretchedData[pixelIndex];
                float g = stretchedData[pixelIndex + width * height];
                float b = stretchedData[pixelIndex + 2 * width * height];
                
                int red = static_cast<int>(std::clamp(r * 255.0f, 0.0f, 255.0f));
                int green = static_cast<int>(std::clamp(g * 255.0f, 0.0f, 255.0f));
                int blue = static_cast<int>(std::clamp(b * 255.0f, 0.0f, 255.0f));
                
                scanLine[x] = qRgb(red, green, blue);
            }
        }
    }
    
    return QPixmap::fromImage(image);
}

void ImageDisplayWidget::stretchImageData(const float* input, float* output, size_t count, double minVal, double maxVal)
{
    if (std::abs(maxVal - minVal) < 1e-6) {
        // Avoid division by zero
        std::fill(output, output + count, 0.5f);
        return;
    }
    
    double scale = 1.0 / (maxVal - minVal);
    
    for (size_t i = 0; i < count; ++i) {
        double stretched = (input[i] - minVal) * scale;
        output[i] = static_cast<float>(std::clamp(stretched, 0.0, 1.0));
    }
}
