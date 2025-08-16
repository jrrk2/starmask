// ColorAnalysisDialog.h
#ifndef COLORANALYSISDIALOG_H
#define COLORANALYSISDIALOG_H

#include <QDialog>
#include <QFileDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QTextEdit>
#include <QProgressBar>
#include <QTableWidget>
#include <QtCharts/QChartView>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QChart>
#include <QtCharts/QValueAxis>
#include <QMessageBox>

#include "RGBPhotometryAnalyzer.h"
#include "ImageReader.h"
#include "StarCatalogValidator.h"

// QT_CHARTS_USE_NAMESPACE

class ColorAnalysisDialog : public QDialog
{
    Q_OBJECT

public:
  explicit ColorAnalysisDialog(const ImageData* imageData,
			       const QVector<QPoint>& centers,
			       const QVector<float>& radii,
			       const QVector<CatalogStar>& catalog,
			       QWidget *parent = nullptr);    

signals:
    void colorCalibrationReady(const ColorCalibrationResult& result);

private slots:
    void onRunColorAnalysis();
    void onCalculateCalibration();
    void onExportResults();
    void onParameterChanged();
    void onColorAnalysisCompleted(int starsAnalyzed);
    void onCalibrationCompleted(const ColorCalibrationResult& result);
    void onTableCellClicked(int row, int column);

private:
    void setupUI();
    void setupParametersGroup();
    void setupResultsGroup();
    void setupChartsGroup();
    void updateCharts();
    void updateStatistics();
    void populateResultsTable();
    void highlightStarOnChart(int starIndex);
    
    // UI Components
    QVBoxLayout* m_mainLayout;
    
    // Parameters group
    QGroupBox* m_parametersGroup;
    QDoubleSpinBox* m_apertureRadiusSpin;
    QDoubleSpinBox* m_backgroundInnerSpin;
    QDoubleSpinBox* m_backgroundOuterSpin;
    QComboBox* m_colorIndexCombo;
    QCheckBox* m_useSpectralTypesCheck;
    QPushButton* m_runAnalysisButton;
    
    // Results group
    QGroupBox* m_resultsGroup;
    QTableWidget* m_resultsTable;
    QTextEdit* m_statisticsText;
    QPushButton* m_calculateCalibButton;
    QPushButton* m_exportButton;
    QProgressBar* m_progressBar;
    
    // Charts group
    QGroupBox* m_chartsGroup;
    QChartView* m_colorScatterChart;
    QChartView* m_residualsChart;
    
    // Data members
    RGBPhotometryAnalyzer* m_analyzer;
    const ImageData* m_imageData;
    QVector<QPoint> m_starCenters;
    QVector<float> m_starRadii;
    QVector<CatalogStar> m_catalogStars;
    bool m_hasValidData;
    
    // Chart data
    QScatterSeries* m_observedColorSeries;
    QScatterSeries* m_catalogColorSeries;
    QScatterSeries* m_residualsSeries;
};

#endif // COLORANALYSISDIALOG_H
