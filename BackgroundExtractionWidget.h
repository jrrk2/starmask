// BackgroundExtractionWidget.h - Enhanced for per-channel background extraction UI
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
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QHeaderView>
#include <QScrollArea>
#include <memory>

#include "BackgroundExtractor.h"
#include "ImageReader.h"

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
    
    // NEW: Per-channel result access
    ChannelResult getChannelResult(int channel) const;
    QVector<float> getChannelBackgroundData(int channel) const;
    QVector<float> getChannelCorrectedData(int channel) const;

signals:
    void backgroundExtracted(const BackgroundExtractionResult& result);
    void backgroundModelChanged(const QVector<float>& backgroundData, int width, int height, int channels);
    void correctedImageReady(const QVector<float>& correctedData, int width, int height, int channels);
    
    // NEW: Per-channel signals
    void channelBackgroundReady(int channel, const QVector<float>& backgroundData, int width, int height);
    void channelCorrectedReady(int channel, const QVector<float>& correctedData, int width, int height);
    void channelAnalysisComplete(const QString& analysisReport);

public slots:
    void onPreviewReady();
    void onExtractionStarted();
    void onExtractionFinished(bool success);
    void onImageClicked(int x, int y, float value);
    void onExtractionProgress(int percentage, const QString& stage);
    
    // NEW: Per-channel progress slots
    void onChannelProgress(int channel, int percentage, const QString& message);
    void onChannelCompleted(int channel, const ChannelResult& result);

private slots:
    // Parameter controls
    void onModelChanged();
    void onSampleGenerationChanged();
    void onToleranceChanged();
    void onDeviationChanged();
    void onSampleCountChanged();
    void onRejectionChanged();
    void onGridSizeChanged();
    
    // NEW: Channel mode controls
    void onChannelModeChanged();
    void onPerChannelSettingsToggled(bool enabled);
    void onChannelWeightsChanged();
    void onCorrelationSettingsChanged();
    
    // Actions
    void onExtractClicked();
    void onPreviewClicked();
    void onCancelClicked();
    void onApplyClicked();
    void onResetClicked();
    void onPresetChanged();
    
    // NEW: Channel-specific actions
    void onAnalyzeChannelsClicked();
  //    void onApplyToChannelClicked();
  //    void onViewChannelResultClicked();
  //    void onCompareChannelsClicked();
    
    // Manual sampling
    void onManualSamplingToggled(bool enabled);
    void onClearSamplesClicked();
  //    void onChannelSelectionChanged();
    
    // Display options
    void onShowBackgroundToggled(bool show);
    void onShowCorrectedToggled(bool show);
    void onShowSamplesToggled(bool show);
    void onShowModelToggled(bool show);
  //    void onChannelDisplayChanged();
    
    // Results
    void onExportResultsClicked();
    void onSaveBackgroundClicked();
    void onSaveCorrectedClicked();
  //    void onExportChannelResultsClicked();

private:
    void setupUI();
    void setupParametersTab();
    void setupChannelTab();        // NEW: Channel-specific settings
    void setupAdvancedTab();
    void setupResultsTab();
    void setupDisplayTab();
    
    void updateControls();
    void updateParametersFromUI();
    void updateUIFromParameters();
    void updateChannelSettings();   // NEW: Update channel-specific UI
    void updateResults();
    void updateChannelResults();    // NEW: Update per-channel results
    void updatePreview();
    void updateSampleDisplay();
    void updateChannelDisplay();    // NEW: Update channel visualization
    
    void enableControls(bool enabled);
    void resetProgress();
    void showError(const QString& message);
    void showSuccess(const QString& message);
    
    // NEW: Channel analysis methods
    void performChannelAnalysis();
    void displayChannelCorrelations();
    void updateChannelRecommendations();
    void updateChannelComparisonTable();
    
    // UI Components
    QVBoxLayout* m_mainLayout;
    QTabWidget* m_tabWidget;
    
    // Parameters Tab (existing)
    QWidget* m_parametersTab;
    QGroupBox* m_modelGroup;
    QGroupBox* m_samplingGroup;
    QGroupBox* m_rejectionGroup;
    QGroupBox* m_gridGroup;
    QGroupBox* m_presetGroup;
    QGroupBox* m_actionGroup;
    
    // Existing parameter controls
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
    QLabel* m_gridRowsLabel;
    QSpinBox* m_gridRowsSpin;
    QLabel* m_gridColumnsLabel;
    QSpinBox* m_gridColumnsSpin;
    QComboBox* m_presetCombo;
    QPushButton* m_resetButton;
    QPushButton* m_previewButton;
    QPushButton* m_extractButton;
    QPushButton* m_cancelButton;
    QPushButton* m_applyButton;
    
    // NEW: Channel Tab
    QWidget* m_channelTab;
    QGroupBox* m_channelModeGroup;
    QGroupBox* m_perChannelGroup;
    QGroupBox* m_channelWeightsGroup;
    QGroupBox* m_correlationGroup;
    QGroupBox* m_channelAnalysisGroup;
    
    QLabel* m_channelModeLabel;
    QComboBox* m_channelModeCombo;
    
    QCheckBox* m_perChannelSettingsCheck;
    QTableWidget* m_channelSettingsTable;
    QPushButton* m_copyToAllChannelsButton;
    QPushButton* m_resetChannelSettingsButton;
    
    QLabel* m_channelWeightsLabel;
    QTableWidget* m_channelWeightsTable;
    QPushButton* m_standardRGBWeightsButton;
    QPushButton* m_equalWeightsButton;
    
    QCheckBox* m_useCorrelationCheck;
    QLabel* m_correlationThresholdLabel;
    QDoubleSpinBox* m_correlationThresholdSpin;
    QCheckBox* m_shareGoodSamplesCheck;
    
    QPushButton* m_analyzeChannelsButton;
    QTextEdit* m_channelAnalysisText;
    QTableWidget* m_channelCorrelationTable;
    
    // Advanced Tab (enhanced)
    QWidget* m_advancedTab;
    QGroupBox* m_processingGroup;
    QGroupBox* m_manualGroup;
    QGroupBox* m_channelActionGroup;  // NEW
    
    QCheckBox* m_discardModelCheck;
    QCheckBox* m_replaceTargetCheck;
    QCheckBox* m_normalizeOutputCheck;
    QLabel* m_maxErrorLabel;
    QDoubleSpinBox* m_maxErrorSpin;
    
    QCheckBox* m_manualSamplingCheck;
    QPushButton* m_clearSamplesButton;
    QLabel* m_sampleCountLabel;
    QComboBox* m_activeChannelCombo;  // NEW: Select which channel for manual sampling
    
    QPushButton* m_applyToChannelButton;     // NEW
    QPushButton* m_viewChannelResultButton; // NEW
    QPushButton* m_compareChannelsButton;    // NEW
    
    // Results Tab (enhanced)
    QWidget* m_resultsTab;
    QGroupBox* m_statisticsGroup;
    QGroupBox* m_samplesGroup;
    QGroupBox* m_channelResultsGroup;  // NEW
    
    QTextEdit* m_statisticsText;
    QTableWidget* m_samplesTable;
    QPushButton* m_exportResultsButton;
    
    QTreeWidget* m_channelResultsTree;     // NEW: Tree view of per-channel results
    QPushButton* m_exportChannelResultsButton;  // NEW
    QTableWidget* m_channelComparisonTable;     // NEW: Compare channels side by side
    
    // Display Tab (enhanced)
    QWidget* m_displayTab;
    QGroupBox* m_displayGroup;
    QGroupBox* m_channelDisplayGroup;  // NEW
    QGroupBox* m_saveGroup;
    
    QCheckBox* m_showBackgroundCheck;
    QCheckBox* m_showCorrectedCheck;
    QCheckBox* m_showSamplesCheck;
    QCheckBox* m_showModelCheck;
    
    QLabel* m_displayChannelLabel;     // NEW
    QComboBox* m_displayChannelCombo;  // NEW: Select which channel to display
    QCheckBox* m_overlayChannelsCheck; // NEW: Overlay multiple channels
    QCheckBox* m_showDifferencesCheck; // NEW: Show channel differences
    
    QPushButton* m_saveBackgroundButton;
    QPushButton* m_saveCorrectedButton;
    QPushButton* m_saveChannelBackgroundButton;  // NEW
    QPushButton* m_saveChannelCorrectedButton;   // NEW
    
    // Status and progress
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;
    
    // NEW: Per-channel progress display
    QGroupBox* m_channelProgressGroup;
    QTableWidget* m_channelProgressTable;
    
    // Data
    std::unique_ptr<BackgroundExtractor> m_extractor;
    std::unique_ptr<ImageData> m_imageData;
    
    bool m_hasResult = false;
    bool m_manualSamplingMode = false;
    int m_activeChannelForSampling = 0;  // NEW: Which channel for manual sampling
    
    // NEW: Channel analysis data
    QVector<double> m_channelCorrelations;
    QString m_channelAnalysisReport;
    
    // Update timer for real-time parameter changes
    QTimer* m_updateTimer;
    
    // Constants
    static constexpr double TOLERANCE_MIN = 0.1;
    static constexpr double TOLERANCE_MAX = 10.0;
    static constexpr double DEVIATION_MIN = 0.1;
    static constexpr double DEVIATION_MAX = 5.0;
    static constexpr double REJECTION_MIN = 0.5;
    static constexpr double REJECTION_MAX = 5.0;
    static constexpr double CORRELATION_MIN = 0.1;
    static constexpr double CORRELATION_MAX = 1.0;
};

#endif // BACKGROUND_EXTRACTION_WIDGET_H
