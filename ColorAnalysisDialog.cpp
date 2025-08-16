#include "ColorAnalysisDialog.h"

// ColorAnalysisDialog.cpp
ColorAnalysisDialog::ColorAnalysisDialog(const ImageData* imageData,
			       const QVector<QPoint>& centers,
			       const QVector<float>& radii,
			       const QVector<CatalogStar>& catalog,
			       QWidget *parent)   
    : QDialog(parent)
    , m_imageData(imageData)
    , m_catalogStars(catalog)
    , m_starCenters(centers)
    , m_starRadii(radii)
    , m_analyzer(new RGBPhotometryAnalyzer(this))
    , m_hasValidData(false)
    , m_observedColorSeries(nullptr)
    , m_catalogColorSeries(nullptr)
    , m_residualsSeries(nullptr)
{
    setWindowTitle("RGB Color Analysis & Telescope Calibration");
    setModal(false);
    resize(1000, 700);
    
    setupUI();
    
    // Connect analyzer signals
    connect(m_analyzer, &RGBPhotometryAnalyzer::colorAnalysisCompleted,
            this, &ColorAnalysisDialog::onColorAnalysisCompleted);
    connect(m_analyzer, &RGBPhotometryAnalyzer::calibrationCompleted,
            this, &ColorAnalysisDialog::onCalibrationCompleted);

    m_analyzer->setStarCatalogData(catalog);
    
    qDebug() << "Color analysis: Set catalog with" << catalog.size() << "stars";
    if (imageData->channels < 3) {
        QMessageBox::warning(this, "Color Analysis", 
                           "RGB color analysis requires a 3-channel color image.\n"
                           "Current image has " + QString::number(imageData->channels) + " channels.");
        m_runAnalysisButton->setEnabled(false);
        return;
    }
    
    m_hasValidData = !m_starCenters.isEmpty();
    m_runAnalysisButton->setEnabled(m_hasValidData);
    
    qDebug() << "Color analysis: Set image data" << imageData->width << "x" << imageData->height 
             << "channels:" << imageData->channels;

    

}

void ColorAnalysisDialog::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    
    // Create main groups
    setupParametersGroup();
    setupResultsGroup();
    setupChartsGroup();
    
    // Add groups to main layout
    m_mainLayout->addWidget(m_parametersGroup);
    m_mainLayout->addWidget(m_resultsGroup, 1);
    m_mainLayout->addWidget(m_chartsGroup, 1);
    
    // Initially disable analysis button
    m_runAnalysisButton->setEnabled(false);
}

void ColorAnalysisDialog::setupParametersGroup()
{
    m_parametersGroup = new QGroupBox("Analysis Parameters");
    QGridLayout* layout = new QGridLayout(m_parametersGroup);
    
    // Aperture radius
    layout->addWidget(new QLabel("Aperture Radius (pixels):"), 0, 0);
    m_apertureRadiusSpin = new QDoubleSpinBox;
    m_apertureRadiusSpin->setRange(2.0, 50.0);
    m_apertureRadiusSpin->setValue(8.0);
    m_apertureRadiusSpin->setSingleStep(0.5);
    layout->addWidget(m_apertureRadiusSpin, 0, 1);
    
    // Background annulus
    layout->addWidget(new QLabel("Background Inner Radius:"), 1, 0);
    m_backgroundInnerSpin = new QDoubleSpinBox;
    m_backgroundInnerSpin->setRange(5.0, 100.0);
    m_backgroundInnerSpin->setValue(12.0);
    layout->addWidget(m_backgroundInnerSpin, 1, 1);
    
    layout->addWidget(new QLabel("Background Outer Radius:"), 2, 0);
    m_backgroundOuterSpin = new QDoubleSpinBox;
    m_backgroundOuterSpin->setRange(10.0, 200.0);
    m_backgroundOuterSpin->setValue(20.0);
    layout->addWidget(m_backgroundOuterSpin, 2, 1);
    
    // Color index type
    layout->addWidget(new QLabel("Color Index:"), 3, 0);
    m_colorIndexCombo = new QComboBox;
    m_colorIndexCombo->addItems({"B-V", "V-R", "G-R"});
    layout->addWidget(m_colorIndexCombo, 3, 1);
    
    // Use spectral types
    m_useSpectralTypesCheck = new QCheckBox("Use Spectral Type Color Prediction");
    m_useSpectralTypesCheck->setChecked(true);
    layout->addWidget(m_useSpectralTypesCheck, 4, 0, 1, 2);
    
    // Run analysis button
    m_runAnalysisButton = new QPushButton("Run Color Analysis");
    m_runAnalysisButton->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; padding: 8px; }");
    layout->addWidget(m_runAnalysisButton, 5, 0, 1, 2);
    
    // Connect parameter change signals
    connect(m_apertureRadiusSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ColorAnalysisDialog::onParameterChanged);
    connect(m_backgroundInnerSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ColorAnalysisDialog::onParameterChanged);
    connect(m_backgroundOuterSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ColorAnalysisDialog::onParameterChanged);
    
    connect(m_runAnalysisButton, &QPushButton::clicked,
            this, &ColorAnalysisDialog::onRunColorAnalysis);
}

void ColorAnalysisDialog::setupResultsGroup()
{
    m_resultsGroup = new QGroupBox("Analysis Results");
    QHBoxLayout* layout = new QHBoxLayout(m_resultsGroup);
    
    // Results table
    m_resultsTable = new QTableWidget;
    m_resultsTable->setColumnCount(8);
    QStringList headers = {"Star", "Position", "R Value", "G Value", "B Value", 
                           "B-V (Obs)", "B-V (Cat)", "Error"};
    m_resultsTable->setHorizontalHeaderLabels(headers);
    m_resultsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultsTable->setAlternatingRowColors(true);
    layout->addWidget(m_resultsTable, 2);
    
    // Statistics and controls
    QVBoxLayout* rightLayout = new QVBoxLayout;
    
    m_statisticsText = new QTextEdit;
    m_statisticsText->setMaximumHeight(200);
    m_statisticsText->setReadOnly(true);
    m_statisticsText->setPlainText("Run color analysis to see statistics...");
    rightLayout->addWidget(new QLabel("Color Statistics:"));
    rightLayout->addWidget(m_statisticsText);
    
    // Progress bar
    m_progressBar = new QProgressBar;
    m_progressBar->setVisible(false);
    rightLayout->addWidget(m_progressBar);
    
    // Control buttons
    m_calculateCalibButton = new QPushButton("Calculate Color Calibration");
    m_calculateCalibButton->setEnabled(false);
    m_calculateCalibButton->setStyleSheet("QPushButton { background-color: #2196F3; color: white; font-weight: bold; padding: 6px; }");
    rightLayout->addWidget(m_calculateCalibButton);
    
    m_exportButton = new QPushButton("Export Results");
    m_exportButton->setEnabled(false);
    rightLayout->addWidget(m_exportButton);
    
    rightLayout->addStretch();
    layout->addLayout(rightLayout, 1);
    
    // Connect signals
    connect(m_resultsTable, &QTableWidget::cellClicked,
            this, &ColorAnalysisDialog::onTableCellClicked);
    connect(m_calculateCalibButton, &QPushButton::clicked,
            this, &ColorAnalysisDialog::onCalculateCalibration);
    connect(m_exportButton, &QPushButton::clicked,
            this, &ColorAnalysisDialog::onExportResults);
}

void ColorAnalysisDialog::setupChartsGroup()
{
    m_chartsGroup = new QGroupBox("Color Analysis Charts");
    QHBoxLayout* layout = new QHBoxLayout(m_chartsGroup);
    
    // Color-color diagram
    m_colorScatterChart = new QChartView;
    m_colorScatterChart->setRenderHint(QPainter::Antialiasing);
    layout->addWidget(m_colorScatterChart);
    
    // Residuals chart
    m_residualsChart = new QChartView;
    m_residualsChart->setRenderHint(QPainter::Antialiasing);
    layout->addWidget(m_residualsChart);
}

void ColorAnalysisDialog::onParameterChanged()
{
    // Update analyzer parameters
    m_analyzer->setApertureRadius(m_apertureRadiusSpin->value());
    m_analyzer->setBackgroundAnnulus(m_backgroundInnerSpin->value(), 
                                    m_backgroundOuterSpin->value());
    m_analyzer->setColorIndexType(m_colorIndexCombo->currentText());
}

void ColorAnalysisDialog::onRunColorAnalysis()
{
    if (!m_hasValidData) {
        QMessageBox::warning(this, "Color Analysis", "No valid image data or detected stars available.");
        return;
    }
    
    // Update parameters
    onParameterChanged();
    
    // Show progress
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 0); // Indeterminate
    m_runAnalysisButton->setEnabled(false);
    
    // Run analysis
    qDebug() << "Starting color analysis with" << m_starCenters.size() << "stars";
    bool success = m_analyzer->analyzeStarColors(m_imageData, m_starCenters, m_starRadii);
    
    if (!success) {
        QMessageBox::warning(this, "Color Analysis", "Color analysis failed. Check image data and parameters.");
        m_progressBar->setVisible(false);
        m_runAnalysisButton->setEnabled(true);
    }
}

void ColorAnalysisDialog::onColorAnalysisCompleted(int starsAnalyzed)
{
    m_progressBar->setVisible(false);
    m_runAnalysisButton->setEnabled(true);
    m_calculateCalibButton->setEnabled(starsAnalyzed > 0);
    m_exportButton->setEnabled(starsAnalyzed > 0);
    
    populateResultsTable();
    updateStatistics();
    updateCharts();
    
    QMessageBox::information(this, "Color Analysis Complete", 
                           QString("Successfully analyzed %1 stars for color properties.\n"
                                 "Review results and calculate calibration if needed.")
                           .arg(starsAnalyzed));
}

void ColorAnalysisDialog::populateResultsTable()
{
    QVector<StarColorData> colorData = m_analyzer->getStarColorData();
    
    m_resultsTable->setRowCount(colorData.size());
    
    for (int i = 0; i < colorData.size(); ++i) {
        const StarColorData& star = colorData[i];
        
        // Star index
        m_resultsTable->setItem(i, 0, new QTableWidgetItem(QString::number(star.starIndex)));
        
        // Position
        QString posStr = QString("(%1, %2)").arg(star.position.x(), 0, 'f', 1)
                                          .arg(star.position.y(), 0, 'f', 1);
        m_resultsTable->setItem(i, 1, new QTableWidgetItem(posStr));
        
        // RGB values
        m_resultsTable->setItem(i, 2, new QTableWidgetItem(QString::number(star.redValue, 'f', 1)));
        m_resultsTable->setItem(i, 3, new QTableWidgetItem(QString::number(star.greenValue, 'f', 1)));
        m_resultsTable->setItem(i, 4, new QTableWidgetItem(QString::number(star.blueValue, 'f', 1)));
        
        // Color indices
        m_resultsTable->setItem(i, 5, new QTableWidgetItem(QString::number(star.bv_index, 'f', 3)));
        
        if (star.hasValidCatalogColor) {
            m_resultsTable->setItem(i, 6, new QTableWidgetItem(QString::number(star.catalogBV, 'f', 3)));
            m_resultsTable->setItem(i, 7, new QTableWidgetItem(QString::number(star.bv_difference, 'f', 3)));
            
            // Color code errors
            if (std::abs(star.bv_difference) > 0.2) {
                m_resultsTable->item(i, 7)->setBackground(QColor(255, 200, 200)); // Light red
            } else if (std::abs(star.bv_difference) > 0.1) {
                m_resultsTable->item(i, 7)->setBackground(QColor(255, 255, 200)); // Light yellow
            }
        } else {
            m_resultsTable->setItem(i, 6, new QTableWidgetItem("N/A"));
            m_resultsTable->setItem(i, 7, new QTableWidgetItem("N/A"));
        }
    }
    
    m_resultsTable->resizeColumnsToContents();
}

void ColorAnalysisDialog::updateStatistics()
{
    QVector<StarColorData> colorData = m_analyzer->getStarColorData();
    
    if (colorData.isEmpty()) {
        m_statisticsText->setPlainText("No color data available.");
        return;
    }
    
    // Calculate statistics
    int totalStars = colorData.size();
    int starsWithCatalog = 0;
    double sumBVError = 0, sumBVErrorSq = 0;
    double minBV = 999, maxBV = -999;
    double minCatBV = 999, maxCatBV = -999;
    
    for (const auto& star : colorData) {
        minBV = qMin(minBV, star.bv_index);
        maxBV = qMax(maxBV, star.bv_index);
        
        if (star.hasValidCatalogColor) {
            starsWithCatalog++;
            sumBVError += star.bv_difference;
            sumBVErrorSq += star.bv_difference * star.bv_difference;
            minCatBV = qMin(minCatBV, star.catalogBV);
            maxCatBV = qMax(maxCatBV, star.catalogBV);
        }
    }
    
    QString stats = QString(
        "COLOR ANALYSIS STATISTICS\n"
        "========================\n\n"
        "Total Stars Analyzed: %1\n"
        "Stars with Catalog Colors: %2\n"
        "Coverage: %3%\n\n"
        "Observed B-V Range: %4 to %5\n"
        "Catalog B-V Range: %6 to %7\n\n"
    ).arg(totalStars)
     .arg(starsWithCatalog)
     .arg(starsWithCatalog > 0 ? (100 * starsWithCatalog / totalStars) : 0)
     .arg(minBV, 0, 'f', 3)
     .arg(maxBV, 0, 'f', 3)
     .arg(starsWithCatalog > 0 ? minCatBV : 0.0, 0, 'f', 3)
     .arg(starsWithCatalog > 0 ? maxCatBV : 0.0, 0, 'f', 3);
    
    if (starsWithCatalog > 0) {
        double meanError = sumBVError / starsWithCatalog;
        double rmsError = std::sqrt(sumBVErrorSq / starsWithCatalog);
        
        stats += QString(
            "COLOR ACCURACY:\n"
            "Mean B-V Error: %1\n"
            "RMS B-V Error: %2\n\n"
        ).arg(meanError, 0, 'f', 4)
         .arg(rmsError, 0, 'f', 4);
        
        if (std::abs(meanError) > 0.1) {
            stats += "⚠️ SIGNIFICANT COLOR BIAS DETECTED\n";
            stats += "Consider telescope color calibration\n\n";
        } else if (rmsError < 0.05) {
            stats += "✅ EXCELLENT color accuracy\n\n";
        } else if (rmsError < 0.1) {
            stats += "✅ GOOD color accuracy\n\n";
        } else {
            stats += "⚠️ FAIR color accuracy - some improvement possible\n\n";
        }
    }
    
    stats += "TIP: Click on table rows to highlight stars in charts";
    
    m_statisticsText->setPlainText(stats);
}

void ColorAnalysisDialog::updateCharts()
{
    QVector<StarColorData> colorData = m_analyzer->getStarColorData();
    
    if (colorData.isEmpty()) {
        return;
    }
    
    // Clear existing charts
    if (m_observedColorSeries) {
        delete m_colorScatterChart->chart();
    }
    if (m_residualsSeries) {
        delete m_residualsChart->chart();
    }
    
    // Create color-color scatter plot
    m_observedColorSeries = new QScatterSeries;
    m_observedColorSeries->setName("Observed Colors");
    m_observedColorSeries->setMarkerSize(8.0);
    m_observedColorSeries->setColor(QColor(0, 120, 255));
    
    m_catalogColorSeries = new QScatterSeries;
    m_catalogColorSeries->setName("Catalog Colors");
    m_catalogColorSeries->setMarkerSize(6.0);
    m_catalogColorSeries->setColor(QColor(255, 100, 0));
    
    // Create residuals plot
    m_residualsSeries = new QScatterSeries;
    m_residualsSeries->setName("Color Residuals");
    m_residualsSeries->setMarkerSize(6.0);
    m_residualsSeries->setColor(QColor(200, 0, 0));
    
    double minMag = 999, maxMag = -999;
    
    for (int i = 0; i < colorData.size(); ++i) {
        const StarColorData& star = colorData[i];
        
        // Add to color plot (magnitude vs B-V)
        if (star.magnitude > 0) {
            minMag = qMin(minMag, star.magnitude);
            maxMag = qMax(maxMag, star.magnitude);
            
            m_observedColorSeries->append(star.magnitude, star.bv_index);
            
            if (star.hasValidCatalogColor) {
                m_catalogColorSeries->append(star.magnitude, star.catalogBV);
                
                // Add to residuals plot
                m_residualsSeries->append(star.magnitude, star.bv_difference);
            }
        }
    }
    
    // Setup color chart
    QChart* colorChart = new QChart;
    colorChart->addSeries(m_observedColorSeries);
    colorChart->addSeries(m_catalogColorSeries);
    colorChart->setTitle("Color-Magnitude Diagram");
    colorChart->setAnimationOptions(QChart::SeriesAnimations);
    
    QValueAxis* magAxis = new QValueAxis;
    magAxis->setTitleText("Magnitude");
    magAxis->setRange(maxMag + 0.5, minMag - 0.5); // Inverted for astronomy convention
    colorChart->addAxis(magAxis, Qt::AlignBottom);
    
    QValueAxis* colorAxis = new QValueAxis;
    colorAxis->setTitleText("B-V Color Index");
    colorChart->addAxis(colorAxis, Qt::AlignLeft);
    
    m_observedColorSeries->attachAxis(magAxis);
    m_observedColorSeries->attachAxis(colorAxis);
    m_catalogColorSeries->attachAxis(magAxis);
    m_catalogColorSeries->attachAxis(colorAxis);
    
    m_colorScatterChart->setChart(colorChart);
    
    // Setup residuals chart  
    QChart* residualsChart = new QChart;
    residualsChart->addSeries(m_residualsSeries);
    residualsChart->setTitle("Color Residuals (Observed - Catalog)");
    residualsChart->setAnimationOptions(QChart::SeriesAnimations);
    
    QValueAxis* magAxis2 = new QValueAxis;
    magAxis2->setTitleText("Magnitude");
    magAxis2->setRange(maxMag + 0.5, minMag - 0.5);
    residualsChart->addAxis(magAxis2, Qt::AlignBottom);
    
    QValueAxis* residualAxis = new QValueAxis;
    residualAxis->setTitleText("B-V Residual");
    residualsChart->addAxis(residualAxis, Qt::AlignLeft);
    
    m_residualsSeries->attachAxis(magAxis2);
    m_residualsSeries->attachAxis(residualAxis);
    
    m_residualsChart->setChart(residualsChart);
}

void ColorAnalysisDialog::onCalculateCalibration()
{
    ColorCalibrationResult result = m_analyzer->calculateColorCalibration();
    
    QString message = QString(
        "COLOR CALIBRATION RESULTS\n"
        "========================\n\n"
        "Stars Used: %1\n"
        "Quality: %2\n"
        "RMS Color Error: %3\n"
        "Systematic B-V Error: %4\n\n"
        "Recommended Color Corrections:\n"
        "Red Scale: %5\n"
        "Green Scale: %6\n"
        "Blue Scale: %7\n\n"
    ).arg(result.starsUsed)
     .arg(result.calibrationQuality)
     .arg(result.rmsColorError, 0, 'f', 4)
     .arg(result.systematicBVError, 0, 'f', 4)
     .arg(result.redScale, 0, 'f', 4)
     .arg(result.greenScale, 0, 'f', 4)
     .arg(result.blueScale, 0, 'f', 4);
    
    if (!result.recommendations.isEmpty()) {
        message += "RECOMMENDATIONS:\n";
        for (const QString& rec : result.recommendations) {
            message += "• " + rec + "\n";
        }
    }
    
    QMessageBox::information(this, "Color Calibration Complete", message);
    
    emit colorCalibrationReady(result);
}

void ColorAnalysisDialog::onExportResults()
{
    QString fileName = QFileDialog::getSaveFileName(this,
        "Save Color Analysis Results", "color_analysis_results.csv",
        "CSV Files (*.csv);;Text Files (*.txt)");
    
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            
            // Header
            out << "StarIndex,Position_X,Position_Y,Red_Value,Green_Value,Blue_Value,"
                   "BV_Observed,BV_Catalog,BV_Error,Spectral_Type,Magnitude\n";
            
            // Data
            QVector<StarColorData> colorData = m_analyzer->getStarColorData();
            for (const auto& star : colorData) {
                out << star.starIndex << ","
                    << star.position.x() << "," << star.position.y() << ","
                    << star.redValue << "," << star.greenValue << "," << star.blueValue << ","
                    << star.bv_index << ","
                    << (star.hasValidCatalogColor ? QString::number(star.catalogBV) : "N/A") << ","
                    << (star.hasValidCatalogColor ? QString::number(star.bv_difference) : "N/A") << ","
                    << star.spectralType << ","
                    << (star.magnitude > 0 ? QString::number(star.magnitude) : "N/A") << "\n";
            }
            
            QMessageBox::information(this, "Export Complete", 
                                   QString("Results saved to:\n%1").arg(fileName));
        }
    }
}

void ColorAnalysisDialog::onTableCellClicked(int row, int column)
{
    Q_UNUSED(column)
    highlightStarOnChart(row);
}

void ColorAnalysisDialog::highlightStarOnChart(int starIndex)
{
    // This would highlight the selected star in the charts
    // Implementation depends on your specific charting needs
    qDebug() << "Highlighting star" << starIndex << "in charts";
}

void ColorAnalysisDialog::onCalibrationCompleted(const ColorCalibrationResult& result)
{
    Q_UNUSED(result)
    // Additional processing if needed when calibration completes
}
