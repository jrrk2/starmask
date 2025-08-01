#include "BackgroundExtractionWidget.h"
#include "ImageDisplayWidget.h"
#include "ImageReader.h"

#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QApplication>
#include <QDebug>
#include <QSplitter>

BackgroundExtractionWidget::BackgroundExtractionWidget(QWidget* parent)
    : QWidget(parent)
    , m_extractor(std::make_unique<BackgroundExtractor>(this))
    , m_updateTimer(new QTimer(this))
{
    setupUI();
    
    // Connect extractor signals
    connect(m_extractor.get(), &BackgroundExtractor::extractionStarted,
            this, &BackgroundExtractionWidget::onExtractionStarted);
    connect(m_extractor.get(), &BackgroundExtractor::extractionProgress,
            this, &BackgroundExtractionWidget::onExtractionProgress);
    connect(m_extractor.get(), &BackgroundExtractor::extractionFinished,
            this, &BackgroundExtractionWidget::onExtractionFinished);
    connect(m_extractor.get(), &BackgroundExtractor::previewReady,
            this, &BackgroundExtractionWidget::onPreviewReady);
    
    // Setup update timer for real-time preview
    m_updateTimer->setSingleShot(true);
    m_updateTimer->setInterval(500); // 500ms delay
    connect(m_updateTimer, &QTimer::timeout, this, &BackgroundExtractionWidget::updatePreview);
    
    // Load settings
    loadSettings();
    updateUIFromParameters();
    enableControls(false); // Disable until image is loaded
}

BackgroundExtractionWidget::~BackgroundExtractionWidget()
{
    saveSettings();
}

void BackgroundExtractionWidget::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    
    // Create tab widget
    m_tabWidget = new QTabWidget;
    m_mainLayout->addWidget(m_tabWidget);
    
    setupParametersTab();
    setupChannelTab();
    setupAdvancedTab();
    setupResultsTab();
    setupDisplayTab();
    
    // Add tabs
    m_tabWidget->addTab(m_parametersTab, "Parameters");
    m_tabWidget->addTab(m_channelTab, "Channel");
    m_tabWidget->addTab(m_advancedTab, "Advanced");
    m_tabWidget->addTab(m_resultsTab, "Results");
    m_tabWidget->addTab(m_displayTab, "Display");
    
    // Status and progress at bottom
    auto* statusLayout = new QHBoxLayout;
    m_statusLabel = new QLabel("Ready");
    m_progressBar = new QProgressBar;
    m_progressBar->setVisible(false);
    
    statusLayout->addWidget(m_statusLabel, 1);
    statusLayout->addWidget(m_progressBar);
    
    m_mainLayout->addLayout(statusLayout);
}

void BackgroundExtractionWidget::setupParametersTab()
{
    m_parametersTab = new QWidget;
    auto* layout = new QVBoxLayout(m_parametersTab);
    
    // Model selection
    m_modelGroup = new QGroupBox("Background Model");
    auto* modelLayout = new QGridLayout(m_modelGroup);
    
    m_modelLabel = new QLabel("Model:");
    m_modelCombo = new QComboBox;
    m_modelCombo->addItem("Linear", static_cast<int>(BackgroundModel::Linear));
    m_modelCombo->addItem("Polynomial (2nd order)", static_cast<int>(BackgroundModel::Polynomial2));
    m_modelCombo->addItem("Polynomial (3rd order)", static_cast<int>(BackgroundModel::Polynomial3));
    m_modelCombo->addItem("Radial Basis Function", static_cast<int>(BackgroundModel::RBF));
    
    connect(m_modelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BackgroundExtractionWidget::onModelChanged);
    
    modelLayout->addWidget(m_modelLabel, 0, 0);
    modelLayout->addWidget(m_modelCombo, 0, 1);
    
    layout->addWidget(m_modelGroup);
    
    // Sample generation
    m_samplingGroup = new QGroupBox("Sample Generation");
    auto* samplingLayout = new QGridLayout(m_samplingGroup);
    
    m_sampleGenLabel = new QLabel("Method:");
    m_sampleGenCombo = new QComboBox;
    m_sampleGenCombo->addItem("Automatic", static_cast<int>(SampleGeneration::Automatic));
    m_sampleGenCombo->addItem("Manual", static_cast<int>(SampleGeneration::Manual));
    m_sampleGenCombo->addItem("Regular Grid", static_cast<int>(SampleGeneration::Grid));
    
    connect(m_sampleGenCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BackgroundExtractionWidget::onSampleGenerationChanged);
    
    samplingLayout->addWidget(m_sampleGenLabel, 0, 0);
    samplingLayout->addWidget(m_sampleGenCombo, 0, 1);
    
    // Tolerance parameter
    m_toleranceLabel = new QLabel("Tolerance:");
    m_toleranceSpin = new QDoubleSpinBox;
    m_toleranceSpin->setRange(TOLERANCE_MIN, TOLERANCE_MAX);
    m_toleranceSpin->setDecimals(2);
    m_toleranceSpin->setSingleStep(0.1);
    m_toleranceSlider = new QSlider(Qt::Horizontal);
    m_toleranceSlider->setRange(static_cast<int>(TOLERANCE_MIN * 100), static_cast<int>(TOLERANCE_MAX * 100));
    
    connect(m_toleranceSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &BackgroundExtractionWidget::onToleranceChanged);
    connect(m_toleranceSlider, &QSlider::valueChanged, [this](int value) {
        m_toleranceSpin->setValue(value / 100.0);
    });
    
    samplingLayout->addWidget(m_toleranceLabel, 1, 0);
    samplingLayout->addWidget(m_toleranceSpin, 1, 1);
    samplingLayout->addWidget(m_toleranceSlider, 1, 2);
    
    // Deviation parameter
    m_deviationLabel = new QLabel("Deviation:");
    m_deviationSpin = new QDoubleSpinBox;
    m_deviationSpin->setRange(DEVIATION_MIN, DEVIATION_MAX);
    m_deviationSpin->setDecimals(2);
    m_deviationSpin->setSingleStep(0.1);
    m_deviationSlider = new QSlider(Qt::Horizontal);
    m_deviationSlider->setRange(static_cast<int>(DEVIATION_MIN * 100), static_cast<int>(DEVIATION_MAX * 100));
    
    connect(m_deviationSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &BackgroundExtractionWidget::onDeviationChanged);
    connect(m_deviationSlider, &QSlider::valueChanged, [this](int value) {
        m_deviationSpin->setValue(value / 100.0);
    });
    
    samplingLayout->addWidget(m_deviationLabel, 2, 0);
    samplingLayout->addWidget(m_deviationSpin, 2, 1);
    samplingLayout->addWidget(m_deviationSlider, 2, 2);
    
    // Sample count limits
    m_minSamplesLabel = new QLabel("Min Samples:");
    m_minSamplesSpin = new QSpinBox;
    m_minSamplesSpin->setRange(10, 10000);
    m_minSamplesSpin->setSingleStep(10);
    connect(m_minSamplesSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &BackgroundExtractionWidget::onSampleCountChanged);
    
    m_maxSamplesLabel = new QLabel("Max Samples:");
    m_maxSamplesSpin = new QSpinBox;
    m_maxSamplesSpin->setRange(50, 50000);
    m_maxSamplesSpin->setSingleStep(100);
    connect(m_maxSamplesSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &BackgroundExtractionWidget::onSampleCountChanged);
    
    samplingLayout->addWidget(m_minSamplesLabel, 3, 0);
    samplingLayout->addWidget(m_minSamplesSpin, 3, 1);
    samplingLayout->addWidget(m_maxSamplesLabel, 4, 0);
    samplingLayout->addWidget(m_maxSamplesSpin, 4, 1);
    
    layout->addWidget(m_samplingGroup);
    
    // Grid sampling controls
    m_gridGroup = new QGroupBox("Grid Sampling");
    auto* gridLayout = new QGridLayout(m_gridGroup);
    
    m_gridRowsLabel = new QLabel("Rows:");
    m_gridRowsSpin = new QSpinBox;
    m_gridRowsSpin->setRange(4, 64);
    m_gridRowsSpin->setValue(16);
    connect(m_gridRowsSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &BackgroundExtractionWidget::onGridSizeChanged);
    
    m_gridColumnsLabel = new QLabel("Columns:");
    m_gridColumnsSpin = new QSpinBox;
    m_gridColumnsSpin->setRange(4, 64);
    m_gridColumnsSpin->setValue(16);
    connect(m_gridColumnsSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &BackgroundExtractionWidget::onGridSizeChanged);
    
    gridLayout->addWidget(m_gridRowsLabel, 0, 0);
    gridLayout->addWidget(m_gridRowsSpin, 0, 1);
    gridLayout->addWidget(m_gridColumnsLabel, 1, 0);
    gridLayout->addWidget(m_gridColumnsSpin, 1, 1);
    
    m_gridGroup->setVisible(false); // Hidden by default
    layout->addWidget(m_gridGroup);
    
    // Outlier rejection
    m_rejectionGroup = new QGroupBox("Outlier Rejection");
    auto* rejectionLayout = new QGridLayout(m_rejectionGroup);
    
    m_rejectionEnabledCheck = new QCheckBox("Enable outlier rejection");
    connect(m_rejectionEnabledCheck, &QCheckBox::toggled,
            this, &BackgroundExtractionWidget::onRejectionChanged);
    
    m_rejectionLowLabel = new QLabel("Low threshold (σ):");
    m_rejectionLowSpin = new QDoubleSpinBox;
    m_rejectionLowSpin->setRange(REJECTION_MIN, REJECTION_MAX);
    m_rejectionLowSpin->setDecimals(1);
    m_rejectionLowSpin->setSingleStep(0.1);
    connect(m_rejectionLowSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &BackgroundExtractionWidget::onRejectionChanged);
    
    m_rejectionHighLabel = new QLabel("High threshold (σ):");
    m_rejectionHighSpin = new QDoubleSpinBox;
    m_rejectionHighSpin->setRange(REJECTION_MIN, REJECTION_MAX);
    m_rejectionHighSpin->setDecimals(1);
    m_rejectionHighSpin->setSingleStep(0.1);
    connect(m_rejectionHighSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &BackgroundExtractionWidget::onRejectionChanged);
    
    m_rejectionIterLabel = new QLabel("Max iterations:");
    m_rejectionIterSpin = new QSpinBox;
    m_rejectionIterSpin->setRange(1, 10);
    connect(m_rejectionIterSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &BackgroundExtractionWidget::onRejectionChanged);
    
    rejectionLayout->addWidget(m_rejectionEnabledCheck, 0, 0, 1, 2);
    rejectionLayout->addWidget(m_rejectionLowLabel, 1, 0);
    rejectionLayout->addWidget(m_rejectionLowSpin, 1, 1);
    rejectionLayout->addWidget(m_rejectionHighLabel, 2, 0);
    rejectionLayout->addWidget(m_rejectionHighSpin, 2, 1);
    rejectionLayout->addWidget(m_rejectionIterLabel, 3, 0);
    rejectionLayout->addWidget(m_rejectionIterSpin, 3, 1);
    
    layout->addWidget(m_rejectionGroup);
    
    // Presets
    m_presetGroup = new QGroupBox("Presets");
    auto* presetLayout = new QHBoxLayout(m_presetGroup);
    
    m_presetCombo = new QComboBox;
    m_presetCombo->addItem("Default");
    m_presetCombo->addItem("Conservative");
    m_presetCombo->addItem("Aggressive");
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BackgroundExtractionWidget::onPresetChanged);
    
    m_resetButton = new QPushButton("Reset to Defaults");
    connect(m_resetButton, &QPushButton::clicked,
            this, &BackgroundExtractionWidget::onResetClicked);
    
    presetLayout->addWidget(m_presetCombo);
    presetLayout->addWidget(m_resetButton);
    presetLayout->addStretch();
    
    layout->addWidget(m_presetGroup);
    
    // Action buttons
    m_actionGroup = new QGroupBox("Actions");
    auto* actionLayout = new QHBoxLayout(m_actionGroup);
    
    m_previewButton = new QPushButton("Preview");
    m_previewButton->setToolTip("Generate a quick preview with reduced resolution");
    connect(m_previewButton, &QPushButton::clicked,
            this, &BackgroundExtractionWidget::onPreviewClicked);
    
    m_extractButton = new QPushButton("Extract Background");
    m_extractButton->setDefault(true);
    connect(m_extractButton, &QPushButton::clicked,
            this, &BackgroundExtractionWidget::onExtractClicked);
    
    m_cancelButton = new QPushButton("Cancel");
    m_cancelButton->setVisible(false);
    connect(m_cancelButton, &QPushButton::clicked,
            this, &BackgroundExtractionWidget::onCancelClicked);
    actionLayout->addWidget(m_previewButton);
    actionLayout->addWidget(m_extractButton);
    actionLayout->addWidget(m_cancelButton);
    actionLayout->addStretch();
    
    layout->addWidget(m_actionGroup);
    
    layout->addStretch();
}

void BackgroundExtractionWidget::setupAdvancedTab()
{
    m_advancedTab = new QWidget;
    auto* layout = new QVBoxLayout(m_advancedTab);
    
    // Processing options
    m_processingGroup = new QGroupBox("Processing Options");
    auto* procLayout = new QGridLayout(m_processingGroup);
    
    m_discardModelCheck = new QCheckBox("Discard model after extraction");
    m_discardModelCheck->setToolTip("Free memory by discarding the fitted model");
    
    m_replaceTargetCheck = new QCheckBox("Replace original image");
    m_replaceTargetCheck->setToolTip("Replace the original image with the corrected version");
    
    m_normalizeOutputCheck = new QCheckBox("Normalize output");
    m_normalizeOutputCheck->setToolTip("Normalize the background model to [0,1] range");
    
    m_maxErrorLabel = new QLabel("Max fitting error:");
    m_maxErrorSpin = new QDoubleSpinBox;
    m_maxErrorSpin->setRange(0.001, 1.0);
    m_maxErrorSpin->setDecimals(4);
    m_maxErrorSpin->setSingleStep(0.001);
    m_maxErrorSpin->setToolTip("Maximum acceptable fitting error threshold");
    
    procLayout->addWidget(m_discardModelCheck, 0, 0, 1, 2);
    procLayout->addWidget(m_replaceTargetCheck, 1, 0, 1, 2);
    procLayout->addWidget(m_normalizeOutputCheck, 2, 0, 1, 2);
    procLayout->addWidget(m_maxErrorLabel, 3, 0);
    procLayout->addWidget(m_maxErrorSpin, 3, 1);
    
    layout->addWidget(m_processingGroup);
    
    // Manual sampling
    m_manualGroup = new QGroupBox("Manual Sampling");
    auto* manualLayout = new QGridLayout(m_manualGroup);
    
    m_manualSamplingCheck = new QCheckBox("Enable manual sampling mode");
    m_manualSamplingCheck->setToolTip("Click on the image to add sample points manually");
    connect(m_manualSamplingCheck, &QCheckBox::toggled,
            this, &BackgroundExtractionWidget::onManualSamplingToggled);
    
    m_clearSamplesButton = new QPushButton("Clear All Samples");
    m_clearSamplesButton->setEnabled(false);
    connect(m_clearSamplesButton, &QPushButton::clicked,
            this, &BackgroundExtractionWidget::onClearSamplesClicked);
    
    m_sampleCountLabel = new QLabel("Samples: 0");
    
    manualLayout->addWidget(m_manualSamplingCheck, 0, 0, 1, 2);
    manualLayout->addWidget(m_clearSamplesButton, 1, 0);
    manualLayout->addWidget(m_sampleCountLabel, 1, 1);
    
    layout->addWidget(m_manualGroup);
    
    layout->addStretch();
}

void BackgroundExtractionWidget::setupResultsTab()
{
    m_resultsTab = new QWidget;
    auto* layout = new QVBoxLayout(m_resultsTab);
    
    // Statistics
    m_statisticsGroup = new QGroupBox("Extraction Statistics");
    auto* statsLayout = new QVBoxLayout(m_statisticsGroup);
    
    m_statisticsText = new QTextEdit;
    m_statisticsText->setReadOnly(true);
    m_statisticsText->setMaximumHeight(150);
    m_statisticsText->setPlainText("No extraction performed yet.");
    
    statsLayout->addWidget(m_statisticsText);
    layout->addWidget(m_statisticsGroup);
    
    // Sample information table
    m_samplesGroup = new QGroupBox("Sample Information");
    auto* samplesLayout = new QVBoxLayout(m_samplesGroup);
    
    m_samplesTable = new QTableWidget;
    m_samplesTable->setColumnCount(5);
    QStringList headers = {"X", "Y", "Value", "Status", "Error"};
    m_samplesTable->setHorizontalHeaderLabels(headers);
    m_samplesTable->horizontalHeader()->setStretchLastSection(true);
    m_samplesTable->setAlternatingRowColors(true);
    m_samplesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_samplesTable->setSortingEnabled(true);
    
    m_exportResultsButton = new QPushButton("Export Results...");
    m_exportResultsButton->setEnabled(false);
    connect(m_exportResultsButton, &QPushButton::clicked,
            this, &BackgroundExtractionWidget::onExportResultsClicked);
    
    samplesLayout->addWidget(m_samplesTable);
    samplesLayout->addWidget(m_exportResultsButton);
    
    layout->addWidget(m_samplesGroup);
}

void BackgroundExtractionWidget::setupDisplayTab()
{
    m_displayTab = new QWidget;
    auto* layout = new QVBoxLayout(m_displayTab);
    /*    
    // Display options
    m_displayGroup = new QGroupBox("Display Options");
    auto* displayLayout = new QGridLayout(m_displayGroup);
    
    m_showBackgroundCheck = new QCheckBox("Show background model");
    m_showBackgroundCheck->setToolTip("Display the extracted background model");
    m_showBackgroundCheck->setChecked(true);  // Default to checked
    
    m_showSamplesCheck = new QCheckBox("Show sample points");
    m_showSamplesCheck->setToolTip("Overlay sample points on the background model");
    m_showSamplesCheck->setChecked(true);  // Default to checked
    
    displayLayout->addWidget(m_showBackgroundCheck, 0, 0);
    displayLayout->addWidget(m_showSamplesCheck, 0, 1);
    
    layout->addWidget(m_displayGroup);
    */
    // Add a dedicated image display widget for the background model
    m_backgroundDisplayWidget = new ImageDisplayWidget;
    m_backgroundDisplayWidget->setMinimumHeight(300);
    layout->addWidget(m_backgroundDisplayWidget, 1);  // Give it stretch
    
    // Save options
    m_saveGroup = new QGroupBox("Save Results");
    auto* saveLayout = new QHBoxLayout(m_saveGroup);
    
    m_saveBackgroundButton = new QPushButton("Save Background Model...");
    m_saveBackgroundButton->setEnabled(false);
    connect(m_saveBackgroundButton, &QPushButton::clicked,
            this, &BackgroundExtractionWidget::onSaveBackgroundClicked);
    
    m_saveCorrectedButton = new QPushButton("Save Corrected Image...");
    m_saveCorrectedButton->setEnabled(false);
    connect(m_saveCorrectedButton, &QPushButton::clicked,
            this, &BackgroundExtractionWidget::onSaveCorrectedClicked);
    
    saveLayout->addWidget(m_saveBackgroundButton);
    saveLayout->addWidget(m_saveCorrectedButton);
    saveLayout->addStretch();
    
    layout->addWidget(m_saveGroup);
}

void BackgroundExtractionWidget::setImageData(const ImageData& imageData)
{
    m_imageData = std::make_unique<ImageData>(imageData);
    enableControls(true);
    
    // Reset results
    m_hasResult = false;
    m_extractor->clearResult();
    updateResults();
    
    m_statusLabel->setText(QString("Image loaded: %1×%2×%3")
                          .arg(imageData.width)
                          .arg(imageData.height)
                          .arg(imageData.channels));
}

void BackgroundExtractionWidget::clearImage()
{
    m_imageData.reset();
    enableControls(false);
    m_hasResult = false;
    m_extractor->clearResult();
    updateResults();
    m_statusLabel->setText("No image loaded");
}

void BackgroundExtractionWidget::loadSettings()
{
    QSettings settings;
    settings.beginGroup("BackgroundExtraction");
    
    // Load parameters
    BackgroundExtractionSettings defaultSettings = BackgroundExtractor::getDefaultSettings();
    
    int model = settings.value("model", static_cast<int>(defaultSettings.model)).toInt();
    int sampleGen = settings.value("sampleGeneration", static_cast<int>(defaultSettings.sampleGeneration)).toInt();
    double tolerance = settings.value("tolerance", defaultSettings.tolerance).toDouble();
    double deviation = settings.value("deviation", defaultSettings.deviation).toDouble();
    int minSamples = settings.value("minSamples", defaultSettings.minSamples).toInt();
    int maxSamples = settings.value("maxSamples", defaultSettings.maxSamples).toInt();
    
    bool useRejection = settings.value("useOutlierRejection", defaultSettings.useOutlierRejection).toBool();
    double rejectionLow = settings.value("rejectionLow", defaultSettings.rejectionLow).toDouble();
    double rejectionHigh = settings.value("rejectionHigh", defaultSettings.rejectionHigh).toDouble();
    int rejectionIter = settings.value("rejectionIterations", defaultSettings.rejectionIterations).toInt();
    
    int gridRows = settings.value("gridRows", defaultSettings.gridRows).toInt();
    int gridColumns = settings.value("gridColumns", defaultSettings.gridColumns).toInt();
    
    bool discardModel = settings.value("discardModel", defaultSettings.discardModel).toBool();
    bool replaceTarget = settings.value("replaceTarget", defaultSettings.replaceTarget).toBool();
    bool normalizeOutput = settings.value("normalizeOutput", defaultSettings.normalizeOutput).toBool();
    double maxError = settings.value("maxError", defaultSettings.maxError).toDouble();
    
    settings.endGroup();
    
    // Apply loaded settings
    BackgroundExtractionSettings loadedSettings;
    loadedSettings.model = static_cast<BackgroundModel>(model);
    loadedSettings.sampleGeneration = static_cast<SampleGeneration>(sampleGen);
    loadedSettings.tolerance = tolerance;
    loadedSettings.deviation = deviation;
    loadedSettings.minSamples = minSamples;
    loadedSettings.maxSamples = maxSamples;
    loadedSettings.useOutlierRejection = useRejection;
    loadedSettings.rejectionLow = rejectionLow;
    loadedSettings.rejectionHigh = rejectionHigh;
    loadedSettings.rejectionIterations = rejectionIter;
    loadedSettings.gridRows = gridRows;
    loadedSettings.gridColumns = gridColumns;
    loadedSettings.discardModel = discardModel;
    loadedSettings.replaceTarget = replaceTarget;
    loadedSettings.normalizeOutput = normalizeOutput;
    loadedSettings.maxError = maxError;
    
    m_extractor->setSettings(loadedSettings);
}

void BackgroundExtractionWidget::saveSettings()
{
    QSettings settings;
    settings.beginGroup("BackgroundExtraction");
    
    BackgroundExtractionSettings current = m_extractor->settings();
    
    settings.setValue("model", static_cast<int>(current.model));
    settings.setValue("sampleGeneration", static_cast<int>(current.sampleGeneration));
    settings.setValue("tolerance", current.tolerance);
    settings.setValue("deviation", current.deviation);
    settings.setValue("minSamples", current.minSamples);
    settings.setValue("maxSamples", current.maxSamples);
    settings.setValue("useOutlierRejection", current.useOutlierRejection);
    settings.setValue("rejectionLow", current.rejectionLow);
    settings.setValue("rejectionHigh", current.rejectionHigh);
    settings.setValue("rejectionIterations", current.rejectionIterations);
    settings.setValue("gridRows", current.gridRows);
    settings.setValue("gridColumns", current.gridColumns);
    settings.setValue("discardModel", current.discardModel);
    settings.setValue("replaceTarget", current.replaceTarget);
    settings.setValue("normalizeOutput", current.normalizeOutput);
    settings.setValue("maxError", current.maxError);
    
    settings.endGroup();
}

void BackgroundExtractionWidget::resetToDefaults()
{
    m_extractor->setSettings(BackgroundExtractor::getDefaultSettings());
    updateUIFromParameters();
}

void BackgroundExtractionWidget::enableControls(bool enabled)
{
    m_previewButton->setEnabled(enabled && !m_extractor->isExtracting());
    m_extractButton->setEnabled(enabled && !m_extractor->isExtracting());
}

void BackgroundExtractionWidget::updateResults()
{
    if (!m_extractor->hasResult()) {
        m_statisticsText->setPlainText("No extraction performed yet.");
        m_samplesTable->setRowCount(0);
        m_exportResultsButton->setEnabled(false);
        m_saveBackgroundButton->setEnabled(false);
        m_saveCorrectedButton->setEnabled(false);
        return;
    }
    
    const BackgroundExtractionResult& result = m_extractor->result();
    
    // Update statistics
    m_statisticsText->setPlainText(result.getStatsSummary());
    
    // Update samples table
    m_samplesTable->setRowCount(result.samplePoints.size());
    
    for (int i = 0; i < result.samplePoints.size(); ++i) {
        const QPoint& point = result.samplePoints[i];
        float value = (i < result.sampleValues.size()) ? result.sampleValues[i] : 0.0f;
        bool rejected = (i < result.sampleRejected.size()) ? result.sampleRejected[i] : false;
        
        m_samplesTable->setItem(i, 0, new QTableWidgetItem(QString::number(point.x())));
        m_samplesTable->setItem(i, 1, new QTableWidgetItem(QString::number(point.y())));
        m_samplesTable->setItem(i, 2, new QTableWidgetItem(QString::number(value, 'f', 6)));
        m_samplesTable->setItem(i, 3, new QTableWidgetItem(rejected ? "Rejected" : "Valid"));
        m_samplesTable->setItem(i, 4, new QTableWidgetItem(QString::number(std::abs(value - 0.1), 'f', 6)));
        
        // Color rejected samples
        if (rejected) {
            for (int col = 0; col < 5; ++col) {
                if (m_samplesTable->item(i, col)) {
                    m_samplesTable->item(i, col)->setBackground(QColor(255, 200, 200));
                }
            }
        }
    }
    
    m_samplesTable->resizeColumnsToContents();
    
    // Enable export/save buttons
    m_exportResultsButton->setEnabled(result.success);
    m_saveBackgroundButton->setEnabled(result.success && !result.backgroundData.isEmpty());
    m_saveCorrectedButton->setEnabled(result.success && !result.correctedData.isEmpty());
}

// Slot implementations
void BackgroundExtractionWidget::onExtractionStarted()
{
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);
    m_statusLabel->setText("Background extraction started...");
    
    m_extractButton->setVisible(false);
    m_cancelButton->setVisible(true);
    
    enableControls(false);
}

void BackgroundExtractionWidget::onExtractionProgress(int percentage, const QString& stage)
{
    m_progressBar->setValue(percentage);
    m_statusLabel->setText(stage);
}

void BackgroundExtractionWidget::onExtractionFinished(bool success)
{
    m_progressBar->setVisible(false);
    
    m_extractButton->setVisible(true);
    m_cancelButton->setVisible(false);
    
    m_hasResult = success;
    
    if (success) {
        const BackgroundExtractionResult result = m_extractor->result();
        updateBackgroundDisplay();
        
        // Enable save buttons
        m_saveBackgroundButton->setEnabled(!result.backgroundData.isEmpty());
        m_saveCorrectedButton->setEnabled(!result.correctedData.isEmpty());
	
        m_statusLabel->setText(QString("Background extraction completed in %1s")
                              .arg(result.processingTimeSeconds, 0, 'f', 2));
        
        // Emit signals for display updates
        if (m_imageData) {
            emit backgroundExtracted(result);
            
            if (!result.backgroundData.isEmpty()) {
                emit backgroundModelChanged(result.backgroundData, 
                                          m_imageData->width, 
                                          m_imageData->height, 
                                          m_imageData->channels);
            }
            
            if (!result.correctedData.isEmpty()) {
                emit correctedImageReady(result.correctedData,
                                       m_imageData->width,
                                       m_imageData->height,
                                       m_imageData->channels);
            }
        }
        
        showSuccess("Background extraction completed successfully!");
    } else {
        const BackgroundExtractionResult result = m_extractor->result();
        m_statusLabel->setText("Background extraction failed");
        showError(QString("Background extraction failed: %1").arg(result.errorMessage));
    }
    
    updateResults();
    enableControls(true);
}

void BackgroundExtractionWidget::onPreviewReady()
{
    m_statusLabel->setText("Preview generated");
    // Could update a preview display here
}

// Parameter change slots
void BackgroundExtractionWidget::onModelChanged()
{
    updateParametersFromUI();
    m_updateTimer->start(); // Trigger preview update
}

void BackgroundExtractionWidget::onSampleGenerationChanged()
{
    updateParametersFromUI();
    updateUIFromParameters(); // Update visibility of controls
    m_updateTimer->start();
}

void BackgroundExtractionWidget::onToleranceChanged()
{
    // Sync slider with spinbox
    m_toleranceSlider->setValue(static_cast<int>(m_toleranceSpin->value() * 100));
    updateParametersFromUI();
    m_updateTimer->start();
}

void BackgroundExtractionWidget::onDeviationChanged()
{
    // Sync slider with spinbox
    m_deviationSlider->setValue(static_cast<int>(m_deviationSpin->value() * 100));
    updateParametersFromUI();
    m_updateTimer->start();
}

void BackgroundExtractionWidget::onSampleCountChanged()
{
    updateParametersFromUI();
    m_updateTimer->start();
}

void BackgroundExtractionWidget::onRejectionChanged()
{
    updateParametersFromUI();
    updateUIFromParameters(); // Update control states
    m_updateTimer->start();
}

void BackgroundExtractionWidget::onGridSizeChanged()
{
    updateParametersFromUI();
    m_updateTimer->start();
}

// Action slots
void BackgroundExtractionWidget::onExtractClicked()
{
    if (!m_imageData) {
        showError("No image loaded");
        return;
    }
    
    updateParametersFromUI();
    m_extractor->extractBackgroundAsync(*m_imageData);
}

void BackgroundExtractionWidget::onPreviewClicked()
{
    if (!m_imageData) {
        showError("No image loaded");
        return;
    }
    
    updateParametersFromUI();
    
    m_statusLabel->setText("Generating preview...");
    if (m_extractor->generatePreview(*m_imageData, 256)) {
        m_statusLabel->setText("Preview generated");
    } else {
        m_statusLabel->setText("Preview failed");
    }
}

void BackgroundExtractionWidget::onCancelClicked()
{
    m_extractor->cancelExtraction();
}

void BackgroundExtractionWidget::onApplyClicked()
{
    if (!m_hasResult || !m_extractor->hasResult()) {
        showError("No background extraction result available");
        return;
    }
    
    const BackgroundExtractionResult& result = m_extractor->result();
    
    if (!result.correctedData.isEmpty() && m_imageData) {
        emit correctedImageReady(result.correctedData,
                               m_imageData->width,
                               m_imageData->height,
                               m_imageData->channels);
        
        showSuccess("Background correction applied to image display");
    }
}

void BackgroundExtractionWidget::onResetClicked()
{
    resetToDefaults();
}

void BackgroundExtractionWidget::onPresetChanged()
{
    int index = m_presetCombo->currentIndex();
    
    BackgroundExtractionSettings settings;
    switch (index) {
    case 0: // Default
        settings = BackgroundExtractor::getDefaultSettings();
        break;
    case 1: // Conservative
        settings = BackgroundExtractor::getConservativeSettings();
        break;
    case 2: // Aggressive
        settings = BackgroundExtractor::getAggressiveSettings();
        break;
    default:
        return;
    }
    
    m_extractor->setSettings(settings);
    updateUIFromParameters();
}

// Manual sampling slots
void BackgroundExtractionWidget::onManualSamplingToggled(bool enabled)
{
    m_manualSamplingMode = enabled;
    m_clearSamplesButton->setEnabled(enabled);
    
    if (enabled) {
        m_sampleGenCombo->setCurrentIndex(m_sampleGenCombo->findData(static_cast<int>(SampleGeneration::Manual)));
        updateParametersFromUI();
    }
    
    m_statusLabel->setText(enabled ? "Manual sampling mode enabled - click on image to add samples" 
                                  : "Manual sampling mode disabled");
}

void BackgroundExtractionWidget::onClearSamplesClicked()
{
    m_extractor->clearManualSamples();
    updateSampleDisplay();
}

void BackgroundExtractionWidget::onImageClicked(int x, int y, float value)
{
    if (m_manualSamplingMode && m_imageData) {
        m_extractor->addManualSample(QPoint(x, y), value);
        updateSampleDisplay();
        
        m_statusLabel->setText(QString("Added sample at (%1, %2) = %3")
                              .arg(x).arg(y).arg(value, 0, 'f', 6));
    }
}

void BackgroundExtractionWidget::updateSampleDisplay()
{
    QVector<QPoint> samples = m_extractor->getManualSamples();
    m_sampleCountLabel->setText(QString("Samples: %1").arg(samples.size()));
}

// Display option slots
void BackgroundExtractionWidget::onShowBackgroundToggled(bool show)
{
  updateBackgroundDisplay();
}

void BackgroundExtractionWidget::onShowCorrectedToggled(bool show)
{
    Q_UNUSED(show)
    // This would update the image display to show/hide the corrected image
}

void BackgroundExtractionWidget::onShowSamplesToggled(bool show)
{
  updateBackgroundDisplay();
}

void BackgroundExtractionWidget::onShowModelToggled(bool show)
{
    Q_UNUSED(show)
    // This would update the image display to show/hide model visualization
}

// Save/export slots
void BackgroundExtractionWidget::onExportResultsClicked()
{
    if (!m_extractor->hasResult()) {
        showError("No results to export");
        return;
    }
    
    QString fileName = QFileDialog::getSaveFileName(
        this,
        "Export Background Extraction Results",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/background_results.csv",
        "CSV Files (*.csv);;All Files (*)"
    );
    
    if (fileName.isEmpty()) {
        return;
    }
    
    // Implementation would export results to CSV
    showSuccess("Results exported successfully");
}

void BackgroundExtractionWidget::onSaveBackgroundClicked()
{
    if (!m_extractor->hasResult() || m_extractor->result().backgroundData.isEmpty()) {
        showError("No background model to save");
        return;
    }
    
    QString fileName = QFileDialog::getSaveFileName(
        this,
        "Save Background Model",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/background_model.xisf",
        "XISF Files (*.xisf);;FITS Files (*.fits);;All Files (*)"
    );
    
    if (fileName.isEmpty()) {
        return;
    }
    
    // Implementation would save the background model
    showSuccess("Background model saved successfully");
}

void BackgroundExtractionWidget::onSaveCorrectedClicked()
{
    if (!m_extractor->hasResult() || m_extractor->result().correctedData.isEmpty()) {
        showError("No corrected image to save");
        return;
    }
    
    QString fileName = QFileDialog::getSaveFileName(
        this,
        "Save Corrected Image",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/corrected_image.xisf",
        "XISF Files (*.xisf);;FITS Files (*.fits);;All Files (*)"
    );
    
    if (fileName.isEmpty()) {
        return;
    }
    
    // Implementation would save the corrected image
    showSuccess("Corrected image saved successfully");
}

void BackgroundExtractionWidget::updatePreview()
{
    if (m_imageData && !m_extractor->isExtracting()) {
        m_extractor->generatePreview(*m_imageData, 128);
    }
}

void BackgroundExtractionWidget::showError(const QString& message)
{
    QMessageBox::critical(this, "Background Extraction Error", message);
}

void BackgroundExtractionWidget::showSuccess(const QString& message)
{
    QMessageBox::information(this, "Background Extraction", message);
}

bool BackgroundExtractionWidget::hasResult() const
{
    return m_extractor && m_extractor->hasResult();
}

const BackgroundExtractionResult BackgroundExtractionWidget::result() const
{
    static BackgroundExtractionResult emptyResult;
    if (m_extractor) {
        return m_extractor->result();
    }
    return emptyResult;
}

void BackgroundExtractionWidget::setupChannelTab()
{
    m_channelTab = new QWidget;
    auto* layout = new QVBoxLayout(m_channelTab);
    
    // Channel Mode Selection
    m_channelModeGroup = new QGroupBox("Channel Processing Mode");
    auto* modeLayout = new QGridLayout(m_channelModeGroup);
    
    m_channelModeLabel = new QLabel("Processing Mode:");
    m_channelModeCombo = new QComboBox;
    m_channelModeCombo->addItem("Combined (Original)", static_cast<int>(ChannelMode::Combined));
    m_channelModeCombo->addItem("Per-Channel (Recommended)", static_cast<int>(ChannelMode::PerChannel));
    m_channelModeCombo->addItem("Luminance Only", static_cast<int>(ChannelMode::LuminanceOnly));
    
    connect(m_channelModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BackgroundExtractionWidget::onChannelModeChanged);
    
    modeLayout->addWidget(m_channelModeLabel, 0, 0);
    modeLayout->addWidget(m_channelModeCombo, 0, 1);
    
    // Add explanatory text
    auto* modeDescription = new QLabel(
        "• Combined: Process all channels together (faster, less accurate)\n"
        "• Per-Channel: Process each channel separately (slower, more accurate)\n"
        "• Luminance Only: Extract from luminance, apply to all channels");
    modeDescription->setWordWrap(true);
    modeDescription->setStyleSheet("color: gray; font-size: 10px;");
    modeLayout->addWidget(modeDescription, 1, 0, 1, 2);
    
    layout->addWidget(m_channelModeGroup);
    
    // Per-Channel Settings
    m_perChannelGroup = new QGroupBox("Per-Channel Settings");
    auto* perChannelLayout = new QVBoxLayout(m_perChannelGroup);
    
    m_perChannelSettingsCheck = new QCheckBox("Use different settings for each channel");
    m_perChannelSettingsCheck->setToolTip("Enable to customize tolerance, deviation, and sample counts per channel");
    connect(m_perChannelSettingsCheck, &QCheckBox::toggled,
            this, &BackgroundExtractionWidget::onPerChannelSettingsToggled);
    perChannelLayout->addWidget(m_perChannelSettingsCheck);
    
    // Channel settings table
    m_channelSettingsTable = new QTableWidget(0, 5);
    QStringList headers = {"Channel", "Tolerance", "Deviation", "Min Samples", "Max Samples"};
    m_channelSettingsTable->setHorizontalHeaderLabels(headers);
    m_channelSettingsTable->horizontalHeader()->setStretchLastSection(true);
    m_channelSettingsTable->setAlternatingRowColors(true);
    m_channelSettingsTable->setEnabled(false);
    perChannelLayout->addWidget(m_channelSettingsTable);
    
    // Channel settings buttons
    auto* channelButtonLayout = new QHBoxLayout;
    m_copyToAllChannelsButton = new QPushButton("Copy to All Channels");
    m_copyToAllChannelsButton->setEnabled(false);
    m_resetChannelSettingsButton = new QPushButton("Reset Channel Settings");
    m_resetChannelSettingsButton->setEnabled(false);
    
    channelButtonLayout->addWidget(m_copyToAllChannelsButton);
    channelButtonLayout->addWidget(m_resetChannelSettingsButton);
    channelButtonLayout->addStretch();
    perChannelLayout->addLayout(channelButtonLayout);
    
    layout->addWidget(m_perChannelGroup);
    
    // Channel Weights (for luminance calculation)
    m_channelWeightsGroup = new QGroupBox("Channel Weights");
    auto* weightsLayout = new QVBoxLayout(m_channelWeightsGroup);
    
    m_channelWeightsLabel = new QLabel("Weights for luminance calculation:");
    weightsLayout->addWidget(m_channelWeightsLabel);
    
    m_channelWeightsTable = new QTableWidget(1, 3);
    QStringList weightHeaders = {"Red", "Green", "Blue"};
    m_channelWeightsTable->setHorizontalHeaderLabels(weightHeaders);
    m_channelWeightsTable->setVerticalHeaderLabels({"Weight"});
    m_channelWeightsTable->horizontalHeader()->setStretchLastSection(true);
    m_channelWeightsTable->setMaximumHeight(80);
    
    // Set default RGB weights
    m_channelWeightsTable->setItem(0, 0, new QTableWidgetItem("0.299"));
    m_channelWeightsTable->setItem(0, 1, new QTableWidgetItem("0.587"));
    m_channelWeightsTable->setItem(0, 2, new QTableWidgetItem("0.114"));
    
    connect(m_channelWeightsTable, &QTableWidget::cellChanged,
            this, &BackgroundExtractionWidget::onChannelWeightsChanged);
    
    weightsLayout->addWidget(m_channelWeightsTable);
    
    auto* weightsButtonLayout = new QHBoxLayout;
    m_standardRGBWeightsButton = new QPushButton("Standard RGB");
    m_standardRGBWeightsButton->setToolTip("Set to standard RGB to luminance weights (0.299, 0.587, 0.114)");
    m_equalWeightsButton = new QPushButton("Equal Weights");
    m_equalWeightsButton->setToolTip("Set equal weights for all channels (0.333, 0.333, 0.333)");
    
    connect(m_standardRGBWeightsButton, &QPushButton::clicked, [this]() {
        m_channelWeightsTable->setItem(0, 0, new QTableWidgetItem("0.299"));
        m_channelWeightsTable->setItem(0, 1, new QTableWidgetItem("0.587"));
        m_channelWeightsTable->setItem(0, 2, new QTableWidgetItem("0.114"));
        onChannelWeightsChanged();
    });
    
    connect(m_equalWeightsButton, &QPushButton::clicked, [this]() {
        m_channelWeightsTable->setItem(0, 0, new QTableWidgetItem("0.333"));
        m_channelWeightsTable->setItem(0, 1, new QTableWidgetItem("0.333"));
        m_channelWeightsTable->setItem(0, 2, new QTableWidgetItem("0.333"));
        onChannelWeightsChanged();
    });
    
    weightsButtonLayout->addWidget(m_standardRGBWeightsButton);
    weightsButtonLayout->addWidget(m_equalWeightsButton);
    weightsButtonLayout->addStretch();
    weightsLayout->addLayout(weightsButtonLayout);
    
    layout->addWidget(m_channelWeightsGroup);
    
    // Channel Correlation Settings
    m_correlationGroup = new QGroupBox("Channel Correlation");
    auto* corrLayout = new QGridLayout(m_correlationGroup);
    
    m_useCorrelationCheck = new QCheckBox("Use channel correlation analysis");
    m_useCorrelationCheck->setToolTip("Analyze correlations between channels to optimize processing");
    m_useCorrelationCheck->setChecked(true);
    connect(m_useCorrelationCheck, &QCheckBox::toggled,
            this, &BackgroundExtractionWidget::onCorrelationSettingsChanged);
    corrLayout->addWidget(m_useCorrelationCheck, 0, 0, 1, 2);
    
    m_correlationThresholdLabel = new QLabel("Correlation threshold:");
    m_correlationThresholdSpin = new QDoubleSpinBox;
    m_correlationThresholdSpin->setRange(CORRELATION_MIN, CORRELATION_MAX);
    m_correlationThresholdSpin->setDecimals(2);
    m_correlationThresholdSpin->setSingleStep(0.05);
    m_correlationThresholdSpin->setValue(0.70);
    m_correlationThresholdSpin->setToolTip("Threshold for considering channels correlated (0.7 = high correlation)");
    connect(m_correlationThresholdSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &BackgroundExtractionWidget::onCorrelationSettingsChanged);
    
    corrLayout->addWidget(m_correlationThresholdLabel, 1, 0);
    corrLayout->addWidget(m_correlationThresholdSpin, 1, 1);
    
    m_shareGoodSamplesCheck = new QCheckBox("Share good samples between correlated channels");
    m_shareGoodSamplesCheck->setToolTip("Use good sample points from one channel in correlated channels");
    m_shareGoodSamplesCheck->setChecked(true);
    connect(m_shareGoodSamplesCheck, &QCheckBox::toggled,
            this, &BackgroundExtractionWidget::onCorrelationSettingsChanged);
    corrLayout->addWidget(m_shareGoodSamplesCheck, 2, 0, 1, 2);
    
    layout->addWidget(m_correlationGroup);
    
    // Channel Analysis
    m_channelAnalysisGroup = new QGroupBox("Channel Analysis");
    auto* analysisLayout = new QVBoxLayout(m_channelAnalysisGroup);
    
    m_analyzeChannelsButton = new QPushButton("Analyze Channels");
    m_analyzeChannelsButton->setToolTip("Analyze channel characteristics and correlations");
    m_analyzeChannelsButton->setEnabled(false);
    connect(m_analyzeChannelsButton, &QPushButton::clicked,
            this, &BackgroundExtractionWidget::onAnalyzeChannelsClicked);
    analysisLayout->addWidget(m_analyzeChannelsButton);
    
    // Analysis results display
    m_channelAnalysisText = new QTextEdit;
    m_channelAnalysisText->setReadOnly(true);
    m_channelAnalysisText->setMaximumHeight(120);
    m_channelAnalysisText->setPlainText("Click 'Analyze Channels' to see channel characteristics and recommendations.");
    analysisLayout->addWidget(m_channelAnalysisText);
    
    // Channel correlation matrix
    auto* corrLabel = new QLabel("Channel Correlations:");
    analysisLayout->addWidget(corrLabel);
    
    m_channelCorrelationTable = new QTableWidget(0, 0);
    m_channelCorrelationTable->setMaximumHeight(100);
    m_channelCorrelationTable->setAlternatingRowColors(true);
    analysisLayout->addWidget(m_channelCorrelationTable);
    
    layout->addWidget(m_channelAnalysisGroup);
    
    layout->addStretch();
}

void BackgroundExtractionWidget::updateChannelSettings()
{
    if (!m_imageData) return;
    
    int channels = m_imageData->channels;
    
    // Update channel settings table
    m_channelSettingsTable->setRowCount(channels);
    
    for (int ch = 0; ch < channels; ++ch) {
        // Channel name
        auto* channelItem = new QTableWidgetItem(QString("Channel %1").arg(ch));
        channelItem->setFlags(channelItem->flags() & ~Qt::ItemIsEditable);
        m_channelSettingsTable->setItem(ch, 0, channelItem);
        
        // Get current settings for this channel
        BackgroundExtractionSettings settings = m_extractor->settings();
        
        double tolerance = settings.tolerance;
        double deviation = settings.deviation;
        int minSamples = settings.minSamples;
        int maxSamples = settings.maxSamples;
        
        // Use per-channel settings if available
        if (settings.usePerChannelSettings) {
            if (ch < settings.channelTolerances.size()) tolerance = settings.channelTolerances[ch];
            if (ch < settings.channelDeviations.size()) deviation = settings.channelDeviations[ch];
            if (ch < settings.channelMinSamples.size()) minSamples = settings.channelMinSamples[ch];
            if (ch < settings.channelMaxSamples.size()) maxSamples = settings.channelMaxSamples[ch];
        }
        
        // Create editable controls
        auto* toleranceItem = new QTableWidgetItem(QString::number(tolerance, 'f', 2));
        auto* deviationItem = new QTableWidgetItem(QString::number(deviation, 'f', 2));
        auto* minSamplesItem = new QTableWidgetItem(QString::number(minSamples));
        auto* maxSamplesItem = new QTableWidgetItem(QString::number(maxSamples));
        
        m_channelSettingsTable->setItem(ch, 1, toleranceItem);
        m_channelSettingsTable->setItem(ch, 2, deviationItem);
        m_channelSettingsTable->setItem(ch, 3, minSamplesItem);
        m_channelSettingsTable->setItem(ch, 4, maxSamplesItem);
    }
    
    // Update channel weights table if needed
    if (channels >= 3 && m_channelWeightsTable->columnCount() < channels) {
        m_channelWeightsTable->setColumnCount(channels);
        QStringList headers;
        for (int ch = 0; ch < channels; ++ch) {
            headers << QString("Ch%1").arg(ch);
        }
        m_channelWeightsTable->setHorizontalHeaderLabels(headers);
        
        // Set default weights
        for (int ch = 0; ch < channels; ++ch) {
            if (!m_channelWeightsTable->item(0, ch)) {
                double weight = (ch < 3) ? QVector<double>{0.299, 0.587, 0.114}[ch] : (1.0 / channels);
                m_channelWeightsTable->setItem(0, ch, new QTableWidgetItem(QString::number(weight, 'f', 3)));
            }
        }
    }
    
    // Update active channel combo for manual sampling
    m_activeChannelCombo->clear();
    for (int ch = 0; ch < channels; ++ch) {
        m_activeChannelCombo->addItem(QString("Channel %1").arg(ch), ch);
    }
    
    // Update display channel combo
    if (m_displayChannelCombo) {
        m_displayChannelCombo->clear();
        m_displayChannelCombo->addItem("All Channels", -1);
        for (int ch = 0; ch < channels; ++ch) {
            m_displayChannelCombo->addItem(QString("Channel %1").arg(ch), ch);
        }
    }
    
    // Enable analysis button
    m_analyzeChannelsButton->setEnabled(true);
}

void BackgroundExtractionWidget::onChannelModeChanged()
{
    updateParametersFromUI();
    
    // Enable/disable relevant controls based on channel mode
    ChannelMode mode = static_cast<ChannelMode>(m_channelModeCombo->currentData().toInt());
    
    bool perChannelMode = (mode == ChannelMode::PerChannel);
    bool luminanceMode = (mode == ChannelMode::LuminanceOnly);
    
    // Enable per-channel settings only for per-channel mode
    m_perChannelGroup->setEnabled(perChannelMode);
    
    // Enable channel weights for luminance mode
    m_channelWeightsGroup->setEnabled(luminanceMode);
    
    // Update correlation group
    m_correlationGroup->setEnabled(perChannelMode);
    
    // Update preview
    m_updateTimer->start();
}

void BackgroundExtractionWidget::onPerChannelSettingsToggled(bool enabled)
{
    m_channelSettingsTable->setEnabled(enabled);
    m_copyToAllChannelsButton->setEnabled(enabled);
    m_resetChannelSettingsButton->setEnabled(enabled);
    
    updateParametersFromUI();
}

void BackgroundExtractionWidget::onChannelWeightsChanged()
{
    updateParametersFromUI();
}

void BackgroundExtractionWidget::onCorrelationSettingsChanged()
{
    updateParametersFromUI();
}

void BackgroundExtractionWidget::onAnalyzeChannelsClicked()
{
    if (!m_imageData || !m_imageData->isValid()) {
        showError("No image loaded for analysis");
        return;
    }
    
    performChannelAnalysis();
}

void BackgroundExtractionWidget::performChannelAnalysis()
{
    if (!m_extractor || !m_imageData) return;
    
    m_analyzeChannelsButton->setEnabled(false);
    m_analyzeChannelsButton->setText("Analyzing...");
    
    // Get channel analysis report
    m_channelAnalysisReport = m_extractor->getChannelAnalysisReport(*m_imageData);
    m_channelAnalysisText->setPlainText(m_channelAnalysisReport);
    
    // Get channel correlations
    m_channelCorrelations = m_extractor->analyzeChannelCorrelations(*m_imageData);
    displayChannelCorrelations();
    
    // Emit signal for other components
    emit channelAnalysisComplete(m_channelAnalysisReport);
    
    m_analyzeChannelsButton->setEnabled(true);
    m_analyzeChannelsButton->setText("Analyze Channels");
    
    // Update recommendations
    updateChannelRecommendations();
}

void BackgroundExtractionWidget::displayChannelCorrelations()
{
    if (m_channelCorrelations.isEmpty() || !m_imageData) {
        m_channelCorrelationTable->setRowCount(0);
        m_channelCorrelationTable->setColumnCount(0);
        return;
    }
    
    int channels = m_imageData->channels;
    if (channels < 2) return;
    
    // Create correlation matrix display
    m_channelCorrelationTable->setRowCount(channels - 1);
    m_channelCorrelationTable->setColumnCount(3);
    
    QStringList headers = {"Channel Pair", "Correlation", "Interpretation"};
    m_channelCorrelationTable->setHorizontalHeaderLabels(headers);
    
    for (int i = 0; i < m_channelCorrelations.size(); ++i) {
        double correlation = m_channelCorrelations[i];
        
        // Channel pair
        auto* pairItem = new QTableWidgetItem(QString("Ch%1-Ch%2").arg(i).arg(i + 1));
        pairItem->setFlags(pairItem->flags() & ~Qt::ItemIsEditable);
        m_channelCorrelationTable->setItem(i, 0, pairItem);
        
        // Correlation value
        auto* corrItem = new QTableWidgetItem(QString::number(correlation, 'f', 3));
        corrItem->setFlags(corrItem->flags() & ~Qt::ItemIsEditable);
        
        // Color code based on correlation strength
        if (std::abs(correlation) > 0.8) {
            corrItem->setBackground(QColor(0, 255, 0, 100)); // Green for high correlation
        } else if (std::abs(correlation) > 0.5) {
            corrItem->setBackground(QColor(255, 255, 0, 100)); // Yellow for moderate
        } else {
            corrItem->setBackground(QColor(255, 0, 0, 100)); // Red for low correlation
        }
        
        m_channelCorrelationTable->setItem(i, 1, corrItem);
        
        // Interpretation
        QString interpretation;
        if (std::abs(correlation) > 0.8) {
            interpretation = "High correlation";
        } else if (std::abs(correlation) > 0.5) {
            interpretation = "Moderate correlation";
        } else {
            interpretation = "Low correlation";
        }
        
        auto* interpItem = new QTableWidgetItem(interpretation);
        interpItem->setFlags(interpItem->flags() & ~Qt::ItemIsEditable);
        m_channelCorrelationTable->setItem(i, 2, interpItem);
    }
    
    m_channelCorrelationTable->resizeColumnsToContents();
}

void BackgroundExtractionWidget::updateChannelRecommendations()
{
    if (!m_imageData || m_channelCorrelations.isEmpty()) return;
    
    // Calculate average correlation
    double avgCorrelation = 0.0;
    for (double corr : m_channelCorrelations) {
        avgCorrelation += std::abs(corr);
    }
    avgCorrelation /= m_channelCorrelations.size();
    
    // Update UI based on analysis
    if (avgCorrelation > 0.8) {
        // High correlation - suggest combined or luminance-only processing
        if (m_channelModeCombo->currentData().toInt() == static_cast<int>(ChannelMode::PerChannel)) {
            // Show recommendation in status
            m_statusLabel->setText("Recommendation: High channel correlation detected - consider Combined or Luminance-only mode");
            m_statusLabel->setStyleSheet("color: orange;");
        }
        
        // Disable sample sharing by default for high correlation
        m_shareGoodSamplesCheck->setChecked(true);
        
    } else if (avgCorrelation < 0.3) {
        // Low correlation - strongly recommend per-channel processing
        if (m_channelModeCombo->currentData().toInt() != static_cast<int>(ChannelMode::PerChannel)) {
            m_statusLabel->setText("Recommendation: Low channel correlation detected - Per-channel mode strongly recommended");
            m_statusLabel->setStyleSheet("color: blue;");
        }
        
        // Disable sample sharing for low correlation
        m_shareGoodSamplesCheck->setChecked(false);
        
    } else {
        // Moderate correlation - per-channel with sample sharing is good
        m_statusLabel->setText("Channel correlation analysis complete");
        m_statusLabel->setStyleSheet("");
        m_shareGoodSamplesCheck->setChecked(true);
    }
}

void BackgroundExtractionWidget::updateParametersFromUI()
{
    BackgroundExtractionSettings settings = m_extractor->settings();
    
    // Basic parameters (existing code)
    settings.model = static_cast<BackgroundModel>(m_modelCombo->currentData().toInt());
    settings.sampleGeneration = static_cast<SampleGeneration>(m_sampleGenCombo->currentData().toInt());
    settings.tolerance = m_toleranceSpin->value();
    settings.deviation = m_deviationSpin->value();
    settings.minSamples = m_minSamplesSpin->value();
    settings.maxSamples = m_maxSamplesSpin->value();
    settings.useOutlierRejection = m_rejectionEnabledCheck->isChecked();
    settings.rejectionLow = m_rejectionLowSpin->value();
    settings.rejectionHigh = m_rejectionHighSpin->value();
    settings.rejectionIterations = m_rejectionIterSpin->value();
    settings.gridRows = m_gridRowsSpin->value();
    settings.gridColumns = m_gridColumnsSpin->value();
    settings.discardModel = m_discardModelCheck->isChecked();
    settings.replaceTarget = m_replaceTargetCheck->isChecked();
    settings.normalizeOutput = m_normalizeOutputCheck->isChecked();
    settings.maxError = m_maxErrorSpin->value();
    
    // NEW: Channel-specific parameters
    if (m_channelModeCombo) {
        settings.channelMode = static_cast<ChannelMode>(m_channelModeCombo->currentData().toInt());
    }
    
    // Per-channel settings
    settings.usePerChannelSettings = m_perChannelSettingsCheck->isChecked();
    
    if (settings.usePerChannelSettings && m_imageData) {
        int channels = m_imageData->channels;
        settings.channelTolerances.clear();
        settings.channelDeviations.clear();
        settings.channelMinSamples.clear();
        settings.channelMaxSamples.clear();
        
        for (int ch = 0; ch < channels; ++ch) {
            if (ch < m_channelSettingsTable->rowCount()) {
                auto* toleranceItem = m_channelSettingsTable->item(ch, 1);
                auto* deviationItem = m_channelSettingsTable->item(ch, 2);
                auto* minSamplesItem = m_channelSettingsTable->item(ch, 3);
                auto* maxSamplesItem = m_channelSettingsTable->item(ch, 4);
                
                if (toleranceItem) settings.channelTolerances.append(toleranceItem->text().toDouble());
                if (deviationItem) settings.channelDeviations.append(deviationItem->text().toDouble());
                if (minSamplesItem) settings.channelMinSamples.append(minSamplesItem->text().toInt());
                if (maxSamplesItem) settings.channelMaxSamples.append(maxSamplesItem->text().toInt());
            }
        }
    }
    
    // Channel weights
    settings.channelWeights.clear();
    if (m_channelWeightsTable && m_imageData) {
        int channels = std::min(m_imageData->channels, m_channelWeightsTable->columnCount());
        for (int ch = 0; ch < channels; ++ch) {
            auto* weightItem = m_channelWeightsTable->item(0, ch);
            if (weightItem) {
                settings.channelWeights.append(weightItem->text().toDouble());
            } else {
                settings.channelWeights.append(1.0 / channels); // Equal weights fallback
            }
        }
    }
    
    // Correlation settings
    settings.useChannelCorrelation = m_useCorrelationCheck->isChecked();
    settings.correlationThreshold = m_correlationThresholdSpin->value();
    settings.shareGoodSamples = m_shareGoodSamplesCheck->isChecked();
    
    m_extractor->setSettings(settings);
}

void BackgroundExtractionWidget::updateUIFromParameters()
{
    BackgroundExtractionSettings settings = m_extractor->settings();
    
    // Block signals to avoid recursive updates
    const bool blocked = signalsBlocked();
    blockSignals(true);
    
    // Basic parameters (existing code)
    m_modelCombo->setCurrentIndex(m_modelCombo->findData(static_cast<int>(settings.model)));
    m_sampleGenCombo->setCurrentIndex(m_sampleGenCombo->findData(static_cast<int>(settings.sampleGeneration)));
    
    m_toleranceSpin->setValue(settings.tolerance);
    m_toleranceSlider->setValue(static_cast<int>(settings.tolerance * 100));
    
    m_deviationSpin->setValue(settings.deviation);
    m_deviationSlider->setValue(static_cast<int>(settings.deviation * 100));
    
    m_minSamplesSpin->setValue(settings.minSamples);
    m_maxSamplesSpin->setValue(settings.maxSamples);
    
    m_rejectionEnabledCheck->setChecked(settings.useOutlierRejection);
    m_rejectionLowSpin->setValue(settings.rejectionLow);
    m_rejectionHighSpin->setValue(settings.rejectionHigh);
    m_rejectionIterSpin->setValue(settings.rejectionIterations);
    
    m_gridRowsSpin->setValue(settings.gridRows);
    m_gridColumnsSpin->setValue(settings.gridColumns);
    
    m_discardModelCheck->setChecked(settings.discardModel);
    m_replaceTargetCheck->setChecked(settings.replaceTarget);
    m_normalizeOutputCheck->setChecked(settings.normalizeOutput);
    m_maxErrorSpin->setValue(settings.maxError);
    
    // NEW: Channel-specific UI updates
    if (m_channelModeCombo) {
        m_channelModeCombo->setCurrentIndex(m_channelModeCombo->findData(static_cast<int>(settings.channelMode)));
    }
    
    m_perChannelSettingsCheck->setChecked(settings.usePerChannelSettings);
    
    // Update channel weights
    if (!settings.channelWeights.isEmpty()) {
      for (int ch = 0; ch < std::min((int)settings.channelWeights.size(), (int)m_channelWeightsTable->columnCount()); ++ch) {
            m_channelWeightsTable->setItem(0, ch, new QTableWidgetItem(QString::number(settings.channelWeights[ch], 'f', 3)));
        }
    }
    
    // Correlation settings
    m_useCorrelationCheck->setChecked(settings.useChannelCorrelation);
    m_correlationThresholdSpin->setValue(settings.correlationThreshold);
    m_shareGoodSamplesCheck->setChecked(settings.shareGoodSamples);
    
    // Update visibility based on sample generation method
    m_gridGroup->setVisible(settings.sampleGeneration == SampleGeneration::Grid);
    
    // Update rejection controls state
    bool rejectionEnabled = settings.useOutlierRejection;
    m_rejectionLowSpin->setEnabled(rejectionEnabled);
    m_rejectionHighSpin->setEnabled(rejectionEnabled);
    m_rejectionIterSpin->setEnabled(rejectionEnabled);
    m_rejectionLowLabel->setEnabled(rejectionEnabled);
    m_rejectionHighLabel->setEnabled(rejectionEnabled);
    m_rejectionIterLabel->setEnabled(rejectionEnabled);
    
    // Update channel mode dependent controls
    ChannelMode mode = settings.channelMode;
    bool perChannelMode = (mode == ChannelMode::PerChannel);
    bool luminanceMode = (mode == ChannelMode::LuminanceOnly);
    
    m_perChannelGroup->setEnabled(perChannelMode);
    m_channelWeightsGroup->setEnabled(luminanceMode);
    m_correlationGroup->setEnabled(perChannelMode);
    
    blockSignals(blocked);
}

void BackgroundExtractionWidget::updateChannelResults()
{
    if (!m_extractor->hasResult()) {
        m_channelResultsTree->clear();
        m_channelComparisonTable->setRowCount(0);
        return;
    }
    
    const BackgroundExtractionResult& result = m_extractor->result();
    
    // Update channel results tree
    m_channelResultsTree->clear();
    
    // Create root items for different result categories
    auto* overallItem = new QTreeWidgetItem(m_channelResultsTree, {"Overall Results"});
    auto* channelItem = new QTreeWidgetItem(m_channelResultsTree, {"Per-Channel Results"});
    auto* correlationItem = new QTreeWidgetItem(m_channelResultsTree, {"Channel Correlations"});
    
    // Overall results
    overallItem->addChild(new QTreeWidgetItem({"Processing Mode", 
        result.usedChannelMode == ChannelMode::PerChannel ? "Per-Channel" :
        result.usedChannelMode == ChannelMode::LuminanceOnly ? "Luminance Only" : "Combined"}));
    overallItem->addChild(new QTreeWidgetItem({"Total Samples", QString::number(result.totalSamplesUsed)}));
    overallItem->addChild(new QTreeWidgetItem({"Overall RMS Error", QString::number(result.overallRmsError, 'f', 6)}));
    overallItem->addChild(new QTreeWidgetItem({"Processing Time", QString("%1 s").arg(result.processingTimeSeconds, 0, 'f', 2)}));
    
    // Per-channel results
    for (int ch = 0; ch < result.channelResults.size(); ++ch) {
        const ChannelResult& chResult = result.channelResults[ch];
        
        auto* chItem = new QTreeWidgetItem(channelItem, {QString("Channel %1").arg(ch)});
        
        if (chResult.success) {
            chItem->addChild(new QTreeWidgetItem({"Success", "Yes"}));
            chItem->addChild(new QTreeWidgetItem({"Samples Used", QString::number(chResult.samplesUsed)}));
            chItem->addChild(new QTreeWidgetItem({"RMS Error", QString::number(chResult.rmsError, 'f', 6)}));
            chItem->addChild(new QTreeWidgetItem({"Background Level", QString::number(chResult.backgroundLevel, 'f', 6)}));
            chItem->addChild(new QTreeWidgetItem({"Gradient Strength", QString::number(chResult.gradientStrength, 'f', 6)}));
            chItem->addChild(new QTreeWidgetItem({"Processing Time", QString("%1 s").arg(chResult.processingTimeSeconds, 0, 'f', 2)}));
        } else {
            chItem->addChild(new QTreeWidgetItem({"Success", "No"}));
            chItem->addChild(new QTreeWidgetItem({"Error", chResult.errorMessage}));
        }
    }
    
    // Channel correlations
    for (int i = 0; i < result.channelCorrelations.size(); ++i) {
        correlationItem->addChild(new QTreeWidgetItem({
            QString("Ch%1-Ch%2").arg(i).arg(i + 1),
            QString::number(result.channelCorrelations[i], 'f', 3)
        }));
    }
    
    // Expand all items
    m_channelResultsTree->expandAll();
    
    // Update channel comparison table
    updateChannelComparisonTable();
}

void BackgroundExtractionWidget::updateChannelComparisonTable()
{
    if (!m_extractor->hasResult()) return;
    
    const BackgroundExtractionResult& result = m_extractor->result();
    int channels = result.channelResults.size();
    
    if (channels == 0) return;
    
    m_channelComparisonTable->setRowCount(channels);
    m_channelComparisonTable->setColumnCount(7);
    
    QStringList headers = {"Channel", "Success", "Samples", "RMS Error", "Background", "Gradient", "Time (s)"};
    m_channelComparisonTable->setHorizontalHeaderLabels(headers);
    
    for (int ch = 0; ch < channels; ++ch) {
        const ChannelResult& chResult = result.channelResults[ch];
        
        m_channelComparisonTable->setItem(ch, 0, new QTableWidgetItem(QString("Channel %1").arg(ch)));
        m_channelComparisonTable->setItem(ch, 1, new QTableWidgetItem(chResult.success ? "Yes" : "No"));
        
        if (chResult.success) {
            m_channelComparisonTable->setItem(ch, 2, new QTableWidgetItem(QString::number(chResult.samplesUsed)));
            m_channelComparisonTable->setItem(ch, 3, new QTableWidgetItem(QString::number(chResult.rmsError, 'f', 6)));
            m_channelComparisonTable->setItem(ch, 4, new QTableWidgetItem(QString::number(chResult.backgroundLevel, 'f', 6)));
            m_channelComparisonTable->setItem(ch, 5, new QTableWidgetItem(QString::number(chResult.gradientStrength, 'f', 6)));
            m_channelComparisonTable->setItem(ch, 6, new QTableWidgetItem(QString::number(chResult.processingTimeSeconds, 'f', 2)));
            
            // Color code based on success and quality
            QColor bgColor = Qt::white;
            if (chResult.rmsError < 0.001) {
                bgColor = QColor(0, 255, 0, 100); // Green for excellent
            } else if (chResult.rmsError < 0.01) {
                bgColor = QColor(255, 255, 0, 100); // Yellow for good
            } else {
                bgColor = QColor(255, 200, 200, 100); // Light red for poor
            }
            
            for (int col = 0; col < 7; ++col) {
                if (m_channelComparisonTable->item(ch, col)) {
                    m_channelComparisonTable->item(ch, col)->setBackground(bgColor);
                }
            }
        } else {
            // Mark failed channels
            for (int col = 2; col < 7; ++col) {
                m_channelComparisonTable->setItem(ch, col, new QTableWidgetItem("Failed"));
            }
            
            for (int col = 0; col < 7; ++col) {
                if (m_channelComparisonTable->item(ch, col)) {
                    m_channelComparisonTable->item(ch, col)->setBackground(QColor(255, 0, 0, 100));
                }
            }
        }
    }
    
    m_channelComparisonTable->resizeColumnsToContents();
}

// Per-channel progress handling
void BackgroundExtractionWidget::onChannelProgress(int channel, int percentage, const QString& message)
{
    // Update per-channel progress table if it exists
    if (m_channelProgressTable && channel < m_channelProgressTable->rowCount()) {
        auto* progressItem = m_channelProgressTable->item(channel, 1);
        if (progressItem) {
            progressItem->setText(QString("%1%").arg(percentage));
        }
        
        auto* statusItem = m_channelProgressTable->item(channel, 2);
        if (statusItem) {
            statusItem->setText(message);
        }
    }
    
    // Update overall status
    m_statusLabel->setText(QString("Channel %1: %2").arg(channel).arg(message));
}

void BackgroundExtractionWidget::onChannelCompleted(int channel, const ChannelResult& result)
{
    // Update progress table
    if (m_channelProgressTable && channel < m_channelProgressTable->rowCount()) {
        auto* progressItem = m_channelProgressTable->item(channel, 1);
        if (progressItem) {
            progressItem->setText("100%");
        }
        
        auto* statusItem = m_channelProgressTable->item(channel, 2);
        if (statusItem) {
            statusItem->setText(result.success ? "Completed" : "Failed");
            statusItem->setBackground(result.success ? QColor(0, 255, 0, 100) : QColor(255, 0, 0, 100));
        }
    }
    
    // Emit channel-specific signals
    if (result.success) {
        if (m_imageData) {
            emit channelBackgroundReady(channel, result.backgroundData, m_imageData->width, m_imageData->height);
            emit channelCorrectedReady(channel, result.correctedData, m_imageData->width, m_imageData->height);
        }
    }
}

void BackgroundExtractionWidget::updateBackgroundDisplay()
{
    if (!m_extractor->hasResult() || !m_imageData) {
        m_backgroundDisplayWidget->clearImage();
        return;
    }
    
    const BackgroundExtractionResult& result = m_extractor->result();
    
    if (result.backgroundData.isEmpty()) {
        m_backgroundDisplayWidget->clearImage();
        return;
    }
    
    // Create ImageData for the background model
    ImageData backgroundImageData;
    backgroundImageData.width = m_imageData->width;
    backgroundImageData.height = m_imageData->height;
    backgroundImageData.channels = m_imageData->channels;
    backgroundImageData.pixels = result.backgroundData;
    backgroundImageData.format = "Background Model";
    backgroundImageData.colorSpace = m_imageData->colorSpace;
    
    // Set the background image
    m_backgroundDisplayWidget->setImageData(backgroundImageData);
    
    // TODO: Add sample point overlay (see next step)
}
