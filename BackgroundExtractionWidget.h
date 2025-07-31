#ifndef BACKGROUND_EXTRACTION_WIDGET_H
#define BACKGROUND_EXTRACTION_WIDGET_H

#include <QWidget>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QProgressBar>
#include <QTextEdit>
#include <QCheckBox>
#include <QSlider>
#include <QTabWidget>
#include <QSplitter>
#include <QTableWidget>
#include <QTimer>
#include <memory>

#include "BackgroundExtractor.h"
#include "ImageReader.h"  // Include for complete ImageData type

// Forward declarations
class ImageDisplayWidget;

class BackgroundExtractionWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BackgroundExtractionWidget(QWidget* parent = nullptr);
    ~BackgroundExtractionWidget();
    
    // Set the image to work with
    void setImageData(const ImageData& imageData);
    void clearImage();
    
    // Settings management
    void loadSettings();
    void saveSettings();
    void resetToDefaults();
    
    // Result access
    bool hasResult() const;
    const BackgroundExtractionResult& result() const;

signals:
    void backgroundExtracted(const BackgroundExtractionResult& result);
    void backgroundModelChanged(const QVector<float>& backgroundData, int width, int height, int channels);
    void correctedImageReady(const QVector<float>& correctedData, int width, int height, int channels);

public slots:
    void onPreviewReady();
    void onExtractionStarted();
    void onExtractionFinished(bool success);
    void onImageClicked(int x, int y, float value);
    void onExtractionProgress(int percentage, const QString& stage);

private slots:
    // Parameter controls
    void onModelChanged();
    void onSampleGenerationChanged();
    void onToleranceChanged();
    void onDeviationChanged();
    void onSampleCountChanged();
    void onRejectionChanged();
    void onGridSizeChanged();
    
    // Actions
    void onExtractClicked();
    void onPreviewClicked();
    void onCancelClicked();
    void onApplyClicked();
    void onResetClicked();
    void onPresetChanged();
    
    // Manual sampling
    void onManualSamplingToggled(bool enabled);
    void onClearSamplesClicked();
    
    // Display options
    void onShowBackgroundToggled(bool show);
    void onShowCorrectedToggled(bool show);
    void onShowSamplesToggled(bool show);
    void onShowModelToggled(bool show);
    
    // Results
    void onExportResultsClicked();
    void onSaveBackgroundClicked();
    void onSaveCorrectedClicked();

private:
    void setupUI();
    void setupParametersTab();
    void setupAdvancedTab();
    void setupResultsTab();
    void setupDisplayTab();
    
    void updateControls();
    void updateParametersFromUI();
    void updateUIFromParameters();
    void updateResults();
    void updatePreview();
    void updateSampleDisplay();
    
    void enableControls(bool enabled);
    void resetProgress();
    void showError(const QString& message);
    void showSuccess(const QString& message);
    
    // UI Components
    QVBoxLayout* m_mainLayout;
    QTabWidget* m_tabWidget;
    
    // Parameters Tab
    QWidget* m_parametersTab;
    QGroupBox* m_modelGroup;
    QGroupBox* m_samplingGroup;
    QGroupBox* m_rejectionGroup;
    
    QLabel* m_modelLabel;
    QComboBox* m_modelCombo;
    
    QLabel* m_sampleGenLabel;
    QComboBox* m_sampleGenCombo;
    
    QLabel* m_toleranceLabel;
    QDoubleSpinBox* m_toleranceSpin;
    QSlider* m_toleranceSlider;
    
    QLabel* m_deviationLabel;
    QDoubleSpinBox* m_deviationSpin;
    QSlider* m_deviationSlider;
    
    QLabel* m_minSamplesLabel;
    QSpinBox* m_minSamplesSpin;
    
    QLabel* m_maxSamplesLabel;
    QSpinBox* m_maxSamplesSpin;
    
    QCheckBox* m_rejectionEnabledCheck;
    QLabel* m_rejectionLowLabel;
    QDoubleSpinBox* m_rejectionLowSpin;
    QLabel* m_rejectionHighLabel;
    QDoubleSpinBox* m_rejectionHighSpin;
    QLabel* m_rejectionIterLabel;
    QSpinBox* m_rejectionIterSpin;
    
    // Grid sampling controls
    QGroupBox* m_gridGroup;
    QLabel* m_gridRowsLabel;
    QSpinBox* m_gridRowsSpin;
    QLabel* m_gridColumnsLabel;
    QSpinBox* m_gridColumnsSpin;
    
    // Preset controls
    QGroupBox* m_presetGroup;
    QComboBox* m_presetCombo;
    QPushButton* m_resetButton;
    
    // Advanced Tab
    QWidget* m_advancedTab;
    QGroupBox* m_processingGroup;
    QCheckBox* m_discardModelCheck;
    QCheckBox* m_replaceTargetCheck;
    QCheckBox* m_normalizeOutputCheck;
    QLabel* m_maxErrorLabel;
    QDoubleSpinBox* m_maxErrorSpin;
    
    // Manual sampling
    QGroupBox* m_manualGroup;
    QCheckBox* m_manualSamplingCheck;
    QPushButton* m_clearSamplesButton;
    QLabel* m_sampleCountLabel;
    
    // Action buttons
    QGroupBox* m_actionGroup;
    QPushButton* m_previewButton;
    QPushButton* m_extractButton;
    QPushButton* m_cancelButton;
    QPushButton* m_applyButton;
    
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;
    
    // Results Tab
    QWidget* m_resultsTab;
    QGroupBox* m_statisticsGroup;
    QTextEdit* m_statisticsText;
    
    QGroupBox* m_samplesGroup;
    QTableWidget* m_samplesTable;
    QPushButton* m_exportResultsButton;
    
    // Display Tab
    QWidget* m_displayTab;
    QGroupBox* m_displayGroup;
    QCheckBox* m_showBackgroundCheck;
    QCheckBox* m_showCorrectedCheck;
    QCheckBox* m_showSamplesCheck;
    QCheckBox* m_showModelCheck;
    
    QGroupBox* m_saveGroup;
    QPushButton* m_saveBackgroundButton;
    QPushButton* m_saveCorrectedButton;
    
    // Data
    std::unique_ptr<BackgroundExtractor> m_extractor;
    std::unique_ptr<ImageData> m_imageData;
    
    bool m_hasResult = false;
    bool m_manualSamplingMode = false;
    
    // Update timer for real-time parameter changes
    QTimer* m_updateTimer;
    
    // Constants
    static constexpr double TOLERANCE_MIN = 0.1;
    static constexpr double TOLERANCE_MAX = 10.0;
    static constexpr double DEVIATION_MIN = 0.1;
    static constexpr double DEVIATION_MAX = 5.0;
    static constexpr double REJECTION_MIN = 0.5;
    static constexpr double REJECTION_MAX = 5.0;
};

#endif // BACKGROUND_EXTRACTION_WIDGET_H
