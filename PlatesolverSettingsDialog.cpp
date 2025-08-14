// PlatesolverSettingsDialog.cpp
#include "PlatesolverSettingsDialog.h"

PlatesolverSettingsDialog::PlatesolverSettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUI();
    setWindowTitle("Plate Solver Settings");
    setModal(true);
    resize(500, 400);
    
    // Set default values
    resetToDefaults();
}

void PlatesolverSettingsDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    
    // Paths group
    auto* pathsGroup = new QGroupBox("Paths", this);
    auto* pathsLayout = new QFormLayout(pathsGroup);
    
    // Index path
    auto* indexLayout = new QHBoxLayout();
    m_indexPathEdit = new QLineEdit(this);
    m_indexPathBrowseButton = new QPushButton("Browse...", this);
    indexLayout->addWidget(m_indexPathEdit);
    indexLayout->addWidget(m_indexPathBrowseButton);
    pathsLayout->addRow("Index Path:", indexLayout);
    
    // Config file
    auto* configLayout = new QHBoxLayout();
    m_configFileEdit = new QLineEdit(this);
    m_configFileBrowseButton = new QPushButton("Browse...", this);
    configLayout->addWidget(m_configFileEdit);
    configLayout->addWidget(m_configFileBrowseButton);
    pathsLayout->addRow("Config File:", configLayout);
    
    mainLayout->addWidget(pathsGroup);
    
    // Scale group
    auto* scaleGroup = new QGroupBox("Scale Range", this);
    auto* scaleLayout = new QFormLayout(scaleGroup);
    
    m_minScaleSpinBox = new QDoubleSpinBox(this);
    m_minScaleSpinBox->setRange(0.01, 3600.0);
    m_minScaleSpinBox->setDecimals(2);
    m_minScaleSpinBox->setSuffix(" arcsec/pixel");
    scaleLayout->addRow("Minimum:", m_minScaleSpinBox);
    
    m_maxScaleSpinBox = new QDoubleSpinBox(this);
    m_maxScaleSpinBox->setRange(0.01, 3600.0);
    m_maxScaleSpinBox->setDecimals(2);
    m_maxScaleSpinBox->setSuffix(" arcsec/pixel");
    scaleLayout->addRow("Maximum:", m_maxScaleSpinBox);
    
    mainLayout->addWidget(scaleGroup);
    
    // Solver options group
    auto* optionsGroup = new QGroupBox("Solver Options", this);
    auto* optionsLayout = new QFormLayout(optionsGroup);
    
    m_timeoutSpinBox = new QSpinBox(this);
    m_timeoutSpinBox->setRange(10, 3600);
    m_timeoutSpinBox->setSuffix(" seconds");
    optionsLayout->addRow("Timeout:", m_timeoutSpinBox);
    
    m_maxStarsSpinBox = new QSpinBox(this);
    m_maxStarsSpinBox->setRange(10, 1000);
    optionsLayout->addRow("Max Stars:", m_maxStarsSpinBox);
    
    m_logOddsSpinBox = new QDoubleSpinBox(this);
    m_logOddsSpinBox->setRange(5.0, 50.0);
    m_logOddsSpinBox->setDecimals(1);
    optionsLayout->addRow("Log Odds Threshold:", m_logOddsSpinBox);
    
    m_verboseCheckBox = new QCheckBox("Enable verbose output", this);
    optionsLayout->addRow("", m_verboseCheckBox);
    
    mainLayout->addWidget(optionsGroup);
    
    // Buttons
    m_buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults,
        this);
    mainLayout->addWidget(m_buttonBox);
    
    // Connect signals
    connect(m_indexPathBrowseButton, &QPushButton::clicked,
            this, &PlatesolverSettingsDialog::browseIndexPath);
    connect(m_configFileBrowseButton, &QPushButton::clicked,
            this, &PlatesolverSettingsDialog::browseConfigFile);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_buttonBox->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked,
            this, &PlatesolverSettingsDialog::resetToDefaults);
}

void PlatesolverSettingsDialog::browseIndexPath()
{
    QString dir = QFileDialog::getExistingDirectory(
        this, 
        "Select Index Directory",
        m_indexPathEdit->text(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    
    if (!dir.isEmpty()) {
        m_indexPathEdit->setText(dir);
    }
}

void PlatesolverSettingsDialog::browseConfigFile()
{
    QString file = QFileDialog::getOpenFileName(
        this,
        "Select Config File", 
        m_configFileEdit->text(),
        "Config Files (*.cfg);;All Files (*)"
    );
    
    if (!file.isEmpty()) {
        m_configFileEdit->setText(file);
    }
}

void PlatesolverSettingsDialog::resetToDefaults()
{
    m_indexPathEdit->setText("/opt/homebrew/share/astrometry");
    m_configFileEdit->setText(""); // Auto-detect
    m_minScaleSpinBox->setValue(0.1);
    m_maxScaleSpinBox->setValue(60.0);
    m_timeoutSpinBox->setValue(60);
    m_maxStarsSpinBox->setValue(200);
    m_logOddsSpinBox->setValue(14.0);
    m_verboseCheckBox->setChecked(false);
}

// #include "PlatesolverSettingsDialog.moc"
