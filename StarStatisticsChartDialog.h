#ifndef STAR_STATISTICS_CHART_DIALOG_H
#define STAR_STATISTICS_CHART_DIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QScrollArea>
#include <QFrame>
#include <QPainter>
#include <QMouseEvent>
#include <QToolTip>
#include <QVector>
#include <QMap>
#include <QColor>
#include <QTimer>
#include "StarCatalogValidator.h"

class StarChartWidget;

class StarStatisticsChartDialog : public QDialog
{
    Q_OBJECT

public:
    explicit StarStatisticsChartDialog(const QVector<CatalogStar>& catalogStars, 
                                       QWidget* parent = nullptr);
    ~StarStatisticsChartDialog();

private slots:
    void onPlotModeChanged();
    void onColorSchemeChanged();
    void onShowLegendToggled(bool show);
    void onExportChart();
    void onResetZoom();
    void updateStatistics();

private:
    void setupUI();
    void setupControls();
    void updateChart();
    QColor getSpectralTypeColor(const QString& spectralType) const;
    double getMagnitudeSize(double magnitude, double minMag, double maxMag) const;
    QString formatStarInfo(const CatalogStar& star) const;

    // Data
    QVector<CatalogStar> m_catalogStars;
    
    // UI Components
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_controlsLayout;
    QVBoxLayout* m_leftLayout;
    
    // Controls
    QGroupBox* m_controlsGroup;
    QComboBox* m_plotModeCombo;
    QComboBox* m_colorSchemeCombo;
    QCheckBox* m_showLegendCheck;
    QCheckBox* m_showGridCheck;
    QPushButton* m_exportButton;
    QPushButton* m_resetZoomButton;
    
    // Statistics panel
    QGroupBox* m_statsGroup;
    QLabel* m_totalStarsLabel;
    QLabel* m_magRangeLabel;
    QLabel* m_spectralTypesLabel;
    QLabel* m_brightestStarLabel;
    QLabel* m_faintestStarLabel;
    
    // Chart widget
    StarChartWidget* m_chartWidget;
    
    // Dialog buttons
    QPushButton* m_closeButton;
    
    // Settings
    enum PlotMode {
        MagnitudeVsSpectralType,
        RAvsDecPosition,
        MagnitudeDistribution,
        SpectralTypeDistribution
    };
    
    enum ColorScheme {
        SpectralTypeColors,
        MagnitudeColors,
        MonochromeColors
    };
    
    PlotMode m_currentPlotMode;
    ColorScheme m_currentColorScheme;
    bool m_showLegend;
    bool m_showGrid;
};

// Custom widget for drawing the star chart
class StarChartWidget : public QWidget
{
    Q_OBJECT

public:
    explicit StarChartWidget(QWidget* parent = nullptr);
    void drawAxisTicks(QPainter& painter);    
    void setStarData(const QVector<CatalogStar>& stars);
    void setPlotMode(int mode);
    void setColorScheme(int scheme);
    void setShowLegend(bool show);
    void setShowGrid(bool show);
    void resetZoom();
    
    QSize sizeHint() const override { return QSize(800, 600); }
    QSize minimumSizeHint() const override { return QSize(400, 300); }

signals:
    void starClicked(const CatalogStar& star);
    void starHovered(const CatalogStar& star);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void drawChart(QPainter& painter);
    void drawMagnitudeVsSpectralChart(QPainter& painter);
    void drawPositionChart(QPainter& painter);
    void drawMagnitudeDistribution(QPainter& painter);
    void drawSpectralDistribution(QPainter& painter);
    void drawLegend(QPainter& painter);
    void drawGrid(QPainter& painter);
    void drawAxes(QPainter& painter);
    
    QColor getStarColor(const CatalogStar& star) const;
    double getStarSize(const CatalogStar& star) const;
    QPointF mapStarToWidget(const CatalogStar& star) const;
    int findStarAtPosition(const QPoint& pos) const;
    
    void calculateDataRanges();
    void updateTransform();
    
    // Data
    QVector<CatalogStar> m_stars;
    
    // Plot settings
    enum PlotMode {
        MagnitudeVsSpectralType = 0,
        RAvsDecPosition = 1,
        MagnitudeDistribution = 2,
        SpectralTypeDistribution = 3
    };
    
    enum ColorScheme {
        SpectralTypeColors = 0,
        MagnitudeColors = 1,
        MonochromeColors = 2
    };
    
    PlotMode m_plotMode;
    ColorScheme m_colorScheme;
    bool m_showLegend;
    bool m_showGrid;
    
    // Chart area and margins
    QRect m_chartRect;
    int m_leftMargin;
    int m_rightMargin;
    int m_topMargin;
    int m_bottomMargin;
    
    // Data ranges
    double m_minMagnitude;
    double m_maxMagnitude;
    double m_minRA;
    double m_maxRA;
    double m_minDec;
    double m_maxDec;
    QStringList m_spectralTypes;
    
    // Zoom and pan
    double m_zoomLevel;
    QPointF m_panOffset;
    QPoint m_lastPanPoint;
    bool m_isPanning;
    
    // Hover tracking
    int m_hoveredStarIndex;
    QTimer* m_hoverTimer;
    
    // Color maps
    QMap<QString, QColor> m_spectralColorMap;
    
    // Constants
    static const int MIN_STAR_SIZE = 3;
    static const int MAX_STAR_SIZE = 15;
    static const int LEGEND_WIDTH = 150;
    static const int GRID_SPACING = 50;
};

#endif // STAR_STATISTICS_CHART_DIALOG_H
