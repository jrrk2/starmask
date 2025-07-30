#ifndef IMAGE_DISPLAY_WIDGET_H
#define IMAGE_DISPLAY_WIDGET_H

#include <QWidget>
#include <QScrollArea>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QPixmap>

struct ImageData;

class ImageDisplayWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ImageDisplayWidget(QWidget *parent = nullptr);
    ~ImageDisplayWidget();

    void setImageData(const ImageData& imageData);
    void clearImage();
    
    // Display options
    void setZoomFactor(double factor);
    double zoomFactor() const;
    
    void setAutoStretch(bool enabled);
    bool autoStretch() const;
    
    void setStretchLimits(double minValue, double maxValue);
    void getStretchLimits(double& minValue, double& maxValue) const;

signals:
    void imageClicked(int x, int y, float value);
    void zoomChanged(double factor);

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private slots:
    void onZoomInClicked();
    void onZoomOutClicked();
    void onZoomFitClicked();
    void onZoom100Clicked();
    void onAutoStretchToggled(bool enabled);
    void onStretchChanged();

private:
    void setupUI();
    void updateDisplay();
    void updateZoomControls();
    QPixmap createPixmapFromImageData();
    void stretchImageData(const float* input, float* output, size_t count, double minVal, double maxVal);
    
    // UI components
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_controlLayout;
    QScrollArea* m_scrollArea;
    QLabel* m_imageLabel;
    
    // Zoom controls
    QPushButton* m_zoomInButton;
    QPushButton* m_zoomOutButton;
    QPushButton* m_zoomFitButton;
    QPushButton* m_zoom100Button;
    QLabel* m_zoomLabel;
    
    // Stretch controls
    QPushButton* m_autoStretchButton;
    QLabel* m_minLabel;
    QSlider* m_minSlider;
    QSpinBox* m_minSpinBox;
    QLabel* m_maxLabel;
    QSlider* m_maxSlider;
    QSpinBox* m_maxSpinBox;
    
    // Data
    ImageData* m_imageData;
    QPixmap m_currentPixmap;
    double m_zoomFactor;
    bool m_autoStretchEnabled;
    double m_stretchMin;
    double m_stretchMax;
    
    // Image statistics for stretching
    double m_imageMin;
    double m_imageMax;
    double m_imageMean;
    double m_imageStdDev;
};

#endif // IMAGE_DISPLAY_WIDGET_H