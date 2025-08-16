// Minimal StarStatisticsChartDialog.h - Add these minimal changes to your existing header

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
#include <QTextEdit>         // NEW: Add this include

#include "StarChartWidget.h"
#include "StarCatalogValidator.h"
#include "StarMaskGenerator.h"    // NEW: Add this include for StarMaskResult

class StarStatisticsChartDialog : public QDialog
{
    Q_OBJECT

public:
    // Keep your existing constructor
    explicit StarStatisticsChartDialog(const QVector<CatalogStar>& catalogStars, 
                                       QWidget* parent = nullptr);
    
    // NEW: Add enhanced constructor (minimal version)
    explicit StarStatisticsChartDialog(const QVector<CatalogStar>& catalogStars,
                                       const StarMaskResult& detectedStars,
                                       QWidget* parent = nullptr);
    
    ~StarStatisticsChartDialog();

private slots:
    // Keep all your existing slots
    void onPlotModeChanged();
    void onColorSchemeChanged();
    void onShowLegendToggled(bool show);
    void onExportChart();
    void onResetZoom();
    void updateStatistics();
    
    // NEW: Add these minimal photometry slots
    void onPerformPhotometry();
    void onGeneratePhotometryReport();

private:
    // Keep all your existing methods
    void setupUI();
    void setupControls();
    void updateChart();
    QColor getSpectralTypeColor(const QString& spectralType) const;
    double getMagnitudeSize(double magnitude, double minMag, double maxMag) const;
    QString formatStarInfo(const CatalogStar& star) const;
    
    // NEW: Add these minimal helper methods
    void exportToCSV(const QString& fileName);
    void exportChartImage(const QString& fileName);

    // Keep all your existing data members
    QVector<CatalogStar> m_catalogStars;
    
    // NEW: Add these minimal data members
    StarMaskResult m_detectedStars;          // Only if enhanced mode
    bool m_hasDetectedStars = false;         // Whether we have detected stars
    bool m_photometryComplete = false;       // Whether photometry analysis is done
    
    // Keep all your existing UI components
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
    QLabel* m_detectedStarsLabel = nullptr;  // NEW: For detected stars count (optional)
    
    // Chart widget
    StarChartWidget* m_chartWidget;
    
    // Dialog buttons
    QPushButton* m_closeButton;
    QPushButton* m_reportButton = nullptr;   // NEW: Generate report button (optional)
    
    // NEW: Minimal photometry components (only created if needed)
    QTextEdit* m_photometryStatsText = nullptr;     // Results display
    
    // Keep your existing settings enums - no changes needed
    enum PlotMode {
        MagnitudeVsSpectralType,
        RAvsDecPosition,
        MagnitudeDistribution,
        SpectralTypeDistribution
        // Note: We're not adding new plot modes to keep it simple
    };
    
    enum ColorScheme {
        SpectralTypeColors,
        MagnitudeColors,
        MonochromeColors
        // Note: We're not adding new color schemes to keep it simple
    };
    
    // Keep all your existing settings
    PlotMode m_currentPlotMode;
    ColorScheme m_currentColorScheme;
    bool m_showLegend;
    bool m_showGrid;
    
    // Keep any other existing private members...
};

#endif // STAR_STATISTICS_CHART_DIALOG_H
