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
#include <QCheckBox>
#include <QFrame>
#include <QPixmap>
#include "StarCatalogValidator.h"

struct ImageData;
struct ValidationResult;
struct CatalogStar;

// Additional StarOverlay structure for ImageDisplayWidget
struct StarOverlay {
    QPoint center;
    float radius;
    float flux;
    QColor color = Qt::green;
    bool visible = true;
};

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
    
    // Star overlay controls
    void setStarOverlay(const QVector<QPoint>& centers, const QVector<float>& radii);
    void clearStarOverlay();
    void setStarOverlayVisible(bool visible);
    bool isStarOverlayVisible() const { return m_showStars; }
    
    // Catalog validation display
    void setValidationResults(const ValidationResult& results);
    void clearValidationResults();
    void setCatalogOverlayVisible(bool visible);
    void setValidationOverlayVisible(bool visible);
    bool isCatalogOverlayVisible() const { return m_showCatalog; }
    bool isValidationOverlayVisible() const { return m_showValidation; }
    // Add these method declarations
    void setWCSData(const WCSData& wcs);
    void setWCSOverlayEnabled(bool enabled);
    void clearStarOverlays();
    void addStarOverlay(const QPoint& center, float radius, float flux);

signals:
    void imageClicked(int x, int y, float value);
    void zoomChanged(double factor);
    void starOverlayToggled(bool visible);
    void catalogOverlayToggled(bool visible);
    void validationOverlayToggled(bool visible);

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private slots:
    void onZoomInClicked();
    void onZoomOutClicked();
    void onZoomFitClicked();
    void onZoom100Clicked();
  //    void onAutoStretchToggled(bool enabled);
  //    void onStretchChanged();
    void onShowStarsToggled(bool show);
    void onShowCatalogToggled(bool show);
    void onShowValidationToggled(bool show);

private:
    void setupUI();
    void updateDisplay();
    void updateZoomControls();
    QPixmap createPixmapFromImageData();
    void stretchImageData(const float* input, float* output, size_t count, double minVal, double maxVal);
    void drawOverlays(QPixmap& pixmap);
    void drawStarOverlay(QPainter& painter, double xScale, double yScale);
    void drawCatalogOverlay(QPainter& painter, double xScale, double yScale);
    void drawValidationOverlay(QPainter& painter, double xScale, double yScale);

    WCSData m_wcsData;
    bool m_wcsOverlayEnabled = false;
    QVector<StarOverlay> m_starOverlays;
    
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
    
    // Overlay controls
    QCheckBox* m_showStarsCheck;
    QCheckBox* m_showCatalogCheck;
    QCheckBox* m_showValidationCheck;
  /*    
    // Stretch controls
    QPushButton* m_autoStretchButton;
    QLabel* m_minLabel;
    QSlider* m_minSlider;
    QSpinBox* m_minSpinBox;
    QLabel* m_maxLabel;
    QSlider* m_maxSlider;
    QSpinBox* m_maxSpinBox;
  */    
    // Data
    std::unique_ptr<ImageData> m_ownedImageData;  // Own the data
    const ImageData* m_imageData;
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

    // Star overlay data
    QVector<QPoint> m_starCenters;
    QVector<float> m_starRadii;
    bool m_showStars = false;
    
    // Catalog validation data
    ValidationResult* m_validationResults = nullptr;
    bool m_showCatalog = false;
    bool m_showValidation = false;
};

#endif // IMAGE_DISPLAY_WIDGET_H
