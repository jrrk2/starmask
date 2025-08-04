#include "MainWindow.h"
#include "ImageReader.h"
#include "PCLMockAPI.h"

// PCL includes
#include <pcl/Image.h>
#include <pcl/XISF.h>
#include <pcl/String.h>
#include <pcl/Property.h>
#include <pcl/api/APIInterface.h>

#include <QApplication>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDebug>
#include <QTableWidget>
#include <QMenuBar>
#include <QClipboard>
#include <QDesktopServices>
#include <QSplitter>
#include <QMessageBox>
#include <QJsonDocument>
#include "GaiaGDR3Catalog.h"
#include <QTime>

// MainWindow.cpp implementation for enhanced matching

void MainWindow::setupEnhancedMatchingControls()
{
    m_enhancedMatchingGroup = new QGroupBox("Enhanced Star Matching (PixInsight Methods)");
    m_enhancedMatchingLayout = new QVBoxLayout(m_enhancedMatchingGroup);
    
    // Distance and search parameters section
    QGroupBox* distanceGroup = new QGroupBox("Distance Parameters");
    QGridLayout* distanceLayout = new QGridLayout(distanceGroup);
    
    // Maximum pixel distance
    distanceLayout->addWidget(new QLabel("Max Pixel Distance:"), 0, 0);
    m_maxPixelDistanceSpin = new QDoubleSpinBox;
    m_maxPixelDistanceSpin->setRange(1.0, 50.0);
    m_maxPixelDistanceSpin->setValue(5.0);
    m_maxPixelDistanceSpin->setDecimals(1);
    m_maxPixelDistanceSpin->setSuffix(" px");
    m_maxPixelDistanceSpin->setToolTip("Maximum allowed distance for star matching");
    distanceLayout->addWidget(m_maxPixelDistanceSpin, 0, 1);
    
    // Search radius
    distanceLayout->addWidget(new QLabel("Search Radius:"), 1, 0);
    m_searchRadiusSpin = new QDoubleSpinBox;
    m_searchRadiusSpin->setRange(5.0, 100.0);
    m_searchRadiusSpin->setValue(10.0);
    m_searchRadiusSpin->setDecimals(1);
    m_searchRadiusSpin->setSuffix(" px");
    m_searchRadiusSpin->setToolTip("Search radius for finding candidate matches");
    distanceLayout->addWidget(m_searchRadiusSpin, 1, 1);
    
    // Maximum magnitude difference
    distanceLayout->addWidget(new QLabel("Max Magnitude Diff:"), 2, 0);
    m_maxMagnitudeDiffSpin = new QDoubleSpinBox;
    m_maxMagnitudeDiffSpin->setRange(0.5, 10.0);
    m_maxMagnitudeDiffSpin->setValue(2.0);
    m_maxMagnitudeDiffSpin->setDecimals(1);
    m_maxMagnitudeDiffSpin->setSuffix(" mag");
    m_maxMagnitudeDiffSpin->setToolTip("Maximum allowed magnitude difference");
    distanceLayout->addWidget(m_maxMagnitudeDiffSpin, 2, 1);
    
    m_enhancedMatchingLayout->addWidget(distanceGroup);
    
    // Pattern matching section
    QGroupBox* patternGroup = new QGroupBox("Pattern Matching");
    QGridLayout* patternLayout = new QGridLayout(patternGroup);
    
    // Triangle matching
    m_useTriangleMatchingCheck = new QCheckBox("Enable Triangle Pattern Matching");
    m_useTriangleMatchingCheck->setChecked(true);
    m_useTriangleMatchingCheck->setToolTip("Use geometric triangle patterns for robust matching");
    patternLayout->addWidget(m_useTriangleMatchingCheck, 0, 0, 1, 2);
    
    // Minimum triangle stars
    patternLayout->addWidget(new QLabel("Min Triangle Stars:"), 1, 0);
    m_minTriangleStarsSpin = new QSpinBox;
    m_minTriangleStarsSpin->setRange(6, 50);
    m_minTriangleStarsSpin->setValue(8);
    m_minTriangleStarsSpin->setToolTip("Minimum stars needed for triangle matching");
    patternLayout->addWidget(m_minTriangleStarsSpin, 1, 1);
    
    // Triangle tolerance
    patternLayout->addWidget(new QLabel("Triangle Tolerance:"), 2, 0);
    m_triangleToleranceSpin = new QDoubleSpinBox;
    m_triangleToleranceSpin->setRange(1.0, 20.0);
    m_triangleToleranceSpin->setValue(5.0);
    m_triangleToleranceSpin->setDecimals(1);
    m_triangleToleranceSpin->setSuffix(" %");
    m_triangleToleranceSpin->setToolTip("Tolerance for triangle pattern similarity");
    patternLayout->addWidget(m_triangleToleranceSpin, 2, 1);
    
    m_enhancedMatchingLayout->addWidget(patternGroup);
    
    // Quality control section
    QGroupBox* qualityGroup = new QGroupBox("Quality Control");
    QGridLayout* qualityLayout = new QGridLayout(qualityGroup);
    
    // Minimum match confidence
    qualityLayout->addWidget(new QLabel("Min Match Confidence:"), 0, 0);
    m_minMatchConfidenceSpin = new QDoubleSpinBox;
    m_minMatchConfidenceSpin->setRange(0.1, 1.0);
    m_minMatchConfidenceSpin->setValue(0.7);
    m_minMatchConfidenceSpin->setDecimals(2);
    m_minMatchConfidenceSpin->setToolTip("Minimum confidence threshold for valid matches");
    qualityLayout->addWidget(m_minMatchConfidenceSpin, 0, 1);
    
    // Minimum matches for validation
    qualityLayout->addWidget(new QLabel("Min Matches for Validation:"), 1, 0);
    m_minMatchesValidationSpin = new QSpinBox;
    m_minMatchesValidationSpin->setRange(3, 50);
    m_minMatchesValidationSpin->setValue(5);
    m_minMatchesValidationSpin->setToolTip("Minimum matches required for successful validation");
    qualityLayout->addWidget(m_minMatchesValidationSpin, 1, 1);
    
    m_enhancedMatchingLayout->addWidget(qualityGroup);
    
    // Advanced options section
    QGroupBox* advancedGroup = new QGroupBox("Advanced Options");
    QVBoxLayout* advancedLayout = new QVBoxLayout(advancedGroup);
    
    m_useDistortionModelCheck = new QCheckBox("Apply Distortion Model");
    m_useDistortionModelCheck->setChecked(true);
    m_useDistortionModelCheck->setToolTip("Apply optical distortion correction");
    advancedLayout->addWidget(m_useDistortionModelCheck);
    
    m_useProperMotionCheck = new QCheckBox("Apply Proper Motion Correction");
    m_useProperMotionCheck->setChecked(true);
    m_useProperMotionCheck->setToolTip("Correct catalog positions for proper motion");
    advancedLayout->addWidget(m_useProperMotionCheck);
    
    m_useBayesianMatchingCheck = new QCheckBox("Use Bayesian Matching (Experimental)");
    m_useBayesianMatchingCheck->setChecked(false);
    m_useBayesianMatchingCheck->setToolTip("Use probabilistic matching approach");
    advancedLayout->addWidget(m_useBayesianMatchingCheck);
    
    m_enhancedMatchingLayout->addWidget(advancedGroup);
    
    // Action buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    
    QPushButton* validateEnhancedButton = new QPushButton("Validate (Enhanced)");
    validateEnhancedButton->setToolTip("Perform enhanced star validation using PixInsight methods");
    connect(validateEnhancedButton, &QPushButton::clicked, this, &MainWindow::onValidateStarsEnhanced);
    buttonLayout->addWidget(validateEnhancedButton);
    
    QPushButton* showDetailsButton = new QPushButton("Show Details");
    showDetailsButton->setToolTip("Show detailed matching analysis");
    connect(showDetailsButton, &QPushButton::clicked, this, &MainWindow::onShowMatchingDetails);
    buttonLayout->addWidget(showDetailsButton);
    
    QPushButton* visualizeButton = new QPushButton("Visualize Distortions");
    visualizeButton->setToolTip("Visualize optical distortions");
    connect(visualizeButton, &QPushButton::clicked, this, &MainWindow::onVisualizeDistortions);
    buttonLayout->addWidget(visualizeButton);
    
    QPushButton* exportButton = new QPushButton("Export Results");
    exportButton->setToolTip("Export matching results to file");
    connect(exportButton, &QPushButton::clicked, this, &MainWindow::onExportMatchingResults);
    buttonLayout->addWidget(exportButton);
    
    m_enhancedMatchingLayout->addLayout(buttonLayout);
    
    // Progress bar
    m_matchingProgressBar = new QProgressBar;
    m_matchingProgressBar->setVisible(false);
    m_enhancedMatchingLayout->addWidget(m_matchingProgressBar);
    
    // Enhanced results display
    QGroupBox* resultsGroup = new QGroupBox("Enhanced Results");
    QVBoxLayout* resultsLayout = new QVBoxLayout(resultsGroup);
    
    m_enhancedResultsText = new QTextEdit;
    m_enhancedResultsText->setMaximumHeight(200);
    m_enhancedResultsText->setReadOnly(true);
    m_enhancedResultsText->setPlainText("Enhanced matching results will appear here.\n\n"
                                       "Enhanced matching provides:\n"
                                       "• Triangle pattern recognition\n"
                                       "• Geometric validation\n" 
                                       "• Distortion analysis\n"
                                       "• Statistical quality metrics\n"
                                       "• Outlier rejection");
    resultsLayout->addWidget(m_enhancedResultsText);
    
    m_enhancedMatchingLayout->addWidget(resultsGroup);
    
    // Connect parameter change signals
    connect(m_maxPixelDistanceSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onMatchingParametersChanged);
    connect(m_searchRadiusSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onMatchingParametersChanged);
    connect(m_useTriangleMatchingCheck, &QCheckBox::toggled,
            this, &MainWindow::onMatchingParametersChanged);
    
    // Initially hide enhanced matching controls (show when needed)
    m_enhancedMatchingGroup->setVisible(false);
}

void MainWindow::onValidateStarsEnhanced()
{
    if (!m_starsDetected) {
        QMessageBox::information(this, "Enhanced Validation", "Please detect stars first.");
        return;
    }
    
    if (!m_hasWCS) {
        QMessageBox::warning(this, "Enhanced Validation", "No WCS information available.");
        return;
    }
    
    if (!m_catalogQueried) {
        QMessageBox::information(this, "Enhanced Validation", "Please query catalog first.");
        return;
    }
    
    m_statusLabel->setText("Performing enhanced star validation...");
    m_matchingProgressBar->setVisible(true);
    m_matchingProgressBar->setRange(0, 0); // Indeterminate
    QApplication::processEvents();
    
    try {
        // Get matching parameters from UI
        StarMatchingParameters params = getMatchingParametersFromUI();
        
        // Estimate star magnitudes from detection data (rough approximation)
        QVector<float> estimatedMagnitudes;
        for (int i = 0; i < m_lastStarMask.starRadii.size(); ++i) {
            // Very rough magnitude estimation from star radius
            float radius = m_lastStarMask.starRadii[i];
            float estimatedMag = 12.0 - 2.5 * log10(radius * radius / 4.0);
            estimatedMagnitudes.append(estimatedMag);
        }
        
        qDebug() << "Starting enhanced validation with parameters:";
        qDebug() << "  Max pixel distance:" << params.maxPixelDistance;
        qDebug() << "  Triangle matching:" << params.useTriangleMatching;
        qDebug() << "  Min confidence:" << params.minMatchConfidence;
        
        // Perform enhanced validation
        m_lastEnhancedValidation = m_catalogValidator->validateStarsAdvanced(
            m_lastStarMask.starCenters, estimatedMagnitudes, params);
        
        m_enhancedValidationComplete = m_lastEnhancedValidation.isValid;
        
        // Display results
        displayEnhancedResults(m_lastEnhancedValidation);
        
        // Update image display with enhanced results
        m_imageDisplayWidget->setValidationResults(m_lastEnhancedValidation);
        
        // Update status
        QString statusText = QString("Enhanced validation: %1/%2 matches (%.1f%% confidence)")
                            .arg(m_lastEnhancedValidation.totalMatches)
                            .arg(m_lastEnhancedValidation.totalDetected)
                            .arg(m_lastEnhancedValidation.matchingConfidence);
        m_statusLabel->setText(statusText);
        
    } catch (const std::exception& e) {
        QString errorMsg = QString("Enhanced validation error: %1").arg(e.what());
        m_statusLabel->setText(errorMsg);
        QMessageBox::warning(this, "Enhanced Validation Error", errorMsg);
    }
    
    m_matchingProgressBar->setVisible(false);
    updateEnhancedMatchingControls();
}

StarMatchingParameters MainWindow::getMatchingParametersFromUI()
{
    StarMatchingParameters params;
    
    // Distance parameters
    params.maxPixelDistance = m_maxPixelDistanceSpin->value();
    params.searchRadius = m_searchRadiusSpin->value();
    params.maxMagnitudeDifference = m_maxMagnitudeDiffSpin->value();
    
    // Pattern matching
    params.useTriangleMatching = m_useTriangleMatchingCheck->isChecked();
    params.minTriangleStars = m_minTriangleStarsSpin->value();
    params.triangleTolerancePercent = m_triangleToleranceSpin->value();
    
    // Quality control
    params.minMatchConfidence = m_minMatchConfidenceSpin->value();
    params.minMatchesForValidation = m_minMatchesValidationSpin->value();
    
    // Advanced options
    params.useDistortionModel = m_useDistortionModelCheck->isChecked();
    params.useProperMotionCorrection = m_useProperMotionCheck->isChecked();
    params.useBayesianMatching = m_useBayesianMatchingCheck->isChecked();
    
    return params;
}

void MainWindow::displayEnhancedResults(const EnhancedValidationResult& result)
{
    m_enhancedResultsText->setPlainText(result.enhancedSummary);
    
    // Also update the main results text with enhanced information
    QString detailedResults = result.enhancedSummary;
    
    if (!result.enhancedMatches.isEmpty()) {
        detailedResults += "\n\nTop 10 Highest Confidence Matches:\n";
        detailedResults += "=====================================\n";
        
        // Sort matches by confidence
        auto sortedMatches = result.enhancedMatches;
        std::sort(sortedMatches.begin(), sortedMatches.end(),
                  [](const EnhancedStarMatch& a, const EnhancedStarMatch& b) {
                      return a.confidence > b.confidence;
                  });
        
        int displayCount = std::min(10, (int)(sortedMatches.size()));
        for (int i = 0; i < displayCount; ++i) {
            const auto& match = sortedMatches[i];
            detailedResults += QString("Match %1: Det[%2] ↔ Cat[%3]\n")
                              .arg(i + 1).arg(match.detectedIndex).arg(match.catalogIndex);
            detailedResults += QString("  Distance: %.2f px, Confidence: %.1f%%\n")
                              .arg(match.pixelDistance).arg(match.confidence * 100.0);
            
            if (match.triangleError > 0) {
                detailedResults += QString("  Triangle Error: %.3f\n").arg(match.triangleError);
            }
            
            if (!match.supportingMatches.isEmpty()) {
                detailedResults += QString("  Supporting Matches: %1\n")
                                  .arg(match.supportingMatches.size());
            }
        }
        
        // Add distortion analysis if available
        if (!result.radialDistortions.isEmpty()) {
            double maxDistortion = *std::max_element(result.radialDistortions.begin(), 
                                                    result.radialDistortions.end());
            double avgDistortion = std::accumulate(result.radialDistortions.begin(),
                                                  result.radialDistortions.end(), 0.0) 
                                  / result.radialDistortions.size();
            
            detailedResults += QString("\nDistortion Analysis:\n");
            detailedResults += QString("  Max Radial Distortion: %.3f pixels\n").arg(maxDistortion);
            detailedResults += QString("  Average Distortion: %.3f pixels\n").arg(avgDistortion);
        }
    }
    
    m_resultsText->setPlainText(detailedResults);
}

void MainWindow::onShowMatchingDetails()
{
    if (!m_enhancedValidationComplete) {
        QMessageBox::information(this, "Matching Details", 
                                "Please perform enhanced validation first.");
        return;
    }
    
    // Create detailed analysis dialog
    QDialog* detailsDialog = new QDialog(this);
    detailsDialog->setWindowTitle("Enhanced Matching Details");
    detailsDialog->resize(800, 600);
    
    QVBoxLayout* layout = new QVBoxLayout(detailsDialog);
    
    // Create tab widget for different analysis views
    QTabWidget* tabWidget = new QTabWidget;
    
    // Statistics tab
    QTextEdit* statsText = new QTextEdit;
    statsText->setReadOnly(true);
    statsText->setPlainText(m_lastEnhancedValidation.enhancedSummary);
    tabWidget->addTab(statsText, "Statistics");
    
    // Matches table tab
    QTableWidget* matchesTable = new QTableWidget;
    matchesTable->setColumnCount(7);
    matchesTable->setHorizontalHeaderLabels({
        "Detected", "Catalog", "Distance (px)", "Confidence", 
        "Triangle Error", "Geometric", "Photometric"
    });
    
    matchesTable->setRowCount(m_lastEnhancedValidation.enhancedMatches.size());
    
    for (int i = 0; i < m_lastEnhancedValidation.enhancedMatches.size(); ++i) {
        const auto& match = m_lastEnhancedValidation.enhancedMatches[i];
        
        matchesTable->setItem(i, 0, new QTableWidgetItem(QString::number(match.detectedIndex)));
        matchesTable->setItem(i, 1, new QTableWidgetItem(QString::number(match.catalogIndex)));
        matchesTable->setItem(i, 2, new QTableWidgetItem(QString::number(match.pixelDistance, 'f', 2)));
        matchesTable->setItem(i, 3, new QTableWidgetItem(QString::number(match.confidence * 100, 'f', 1) + "%"));
        matchesTable->setItem(i, 4, new QTableWidgetItem(QString::number(match.triangleError, 'f', 3)));
        matchesTable->setItem(i, 5, new QTableWidgetItem(match.isGeometricallyValid ? "✓" : "✗"));
        matchesTable->setItem(i, 6, new QTableWidgetItem(match.isPhotometricallyValid ? "✓" : "✗"));
    }
    
    matchesTable->resizeColumnsToContents();
    tabWidget->addTab(matchesTable, "Matches");
    
    // Triangle patterns tab (if available)
    if (!m_lastEnhancedValidation.detectedTriangles.isEmpty()) {
        QTextEdit* triangleText = new QTextEdit;
        triangleText->setReadOnly(true);
        
        QString triangleInfo = QString("Triangle Pattern Analysis:\n\n");
        triangleInfo += QString("Detected Triangles: %1\n").arg(m_lastEnhancedValidation.detectedTriangles.size());
        triangleInfo += QString("Catalog Triangles: %1\n").arg(m_lastEnhancedValidation.catalogTriangles.size());
        triangleInfo += QString("Triangle Matches: %1\n\n").arg(m_lastEnhancedValidation.triangleMatches.size());
        
        for (int i = 0; i < std::min(5, (int)(m_lastEnhancedValidation.detectedTriangles.size())); ++i) {
            const auto& triangle = m_lastEnhancedValidation.detectedTriangles[i];
            triangleInfo += QString("Triangle %1:\n").arg(i + 1);
            triangleInfo += QString("  Stars: [%1, %2, %3]\n")
                           .arg(triangle.starIndices[0])
                           .arg(triangle.starIndices[1])
                           .arg(triangle.starIndices[2]);
            triangleInfo += QString("  Sides: %.1f, %.1f, %.1f px\n")
                           .arg(triangle.side1).arg(triangle.side2).arg(triangle.side3);
            triangleInfo += QString("  Area: %.1f px²\n").arg(triangle.area);
            triangleInfo += QString("  Ratios: %.3f, %.3f, %.3f\n\n")
                           .arg(triangle.ratio12).arg(triangle.ratio13).arg(triangle.ratio23);
        }
        
        triangleText->setPlainText(triangleInfo);
        tabWidget->addTab(triangleText, "Triangles");
    }
    
    layout->addWidget(tabWidget);
    
    // Close button
    QPushButton* closeButton = new QPushButton("Close");
    connect(closeButton, &QPushButton::clicked, detailsDialog, &QDialog::accept);
    layout->addWidget(closeButton);
    
    detailsDialog->exec();
    delete detailsDialog;
}

void MainWindow::onVisualizeDistortions()
{
    if (!m_enhancedValidationComplete || m_lastEnhancedValidation.residualVectors.isEmpty()) {
        QMessageBox::information(this, "Distortion Visualization", 
                                "No distortion data available. Please perform enhanced validation first.");
        return;
    }
    
    // Create distortion visualization dialog
    QDialog* distortionDialog = new QDialog(this);
    distortionDialog->setWindowTitle("Optical Distortion Visualization");
    distortionDialog->resize(600, 500);
    
    QVBoxLayout* layout = new QVBoxLayout(distortionDialog);
    
    // Create a simple text-based visualization for now
    // In a full implementation, you would create a custom widget with graphical visualization
    QTextEdit* distortionText = new QTextEdit;
    distortionText->setReadOnly(true);
    
    QString distortionInfo = "Distortion Analysis Results:\n";
    distortionInfo += "============================\n\n";
    
    if (!m_lastEnhancedValidation.radialDistortions.isEmpty()) {
        double maxDist = *std::max_element(m_lastEnhancedValidation.radialDistortions.begin(),
                                          m_lastEnhancedValidation.radialDistortions.end());
        double minDist = *std::min_element(m_lastEnhancedValidation.radialDistortions.begin(),
                                          m_lastEnhancedValidation.radialDistortions.end());
        double avgDist = std::accumulate(m_lastEnhancedValidation.radialDistortions.begin(),
                                        m_lastEnhancedValidation.radialDistortions.end(), 0.0) 
                        / m_lastEnhancedValidation.radialDistortions.size();
        
        distortionInfo += QString("Radial Distortion Statistics:\n");
        distortionInfo += QString("  Maximum: %.3f pixels\n").arg(maxDist);
        distortionInfo += QString("  Minimum: %.3f pixels\n").arg(minDist);
        distortionInfo += QString("  Average: %.3f pixels\n").arg(avgDist);
        distortionInfo += QString("  RMS: %.3f pixels\n\n").arg(m_lastEnhancedValidation.geometricRMS);
    }
    
    if (!m_lastEnhancedValidation.residualVectors.isEmpty()) {
        distortionInfo += QString("Residual Vectors (first 10):\n");
        distortionInfo += QString("Position → Residual\n");
        distortionInfo += QString("===================\n");
        
        int count = std::min(10, (int)(m_lastEnhancedValidation.residualVectors.size()));
        for (int i = 0; i < count; ++i) {
            const QPointF& residual = m_lastEnhancedValidation.residualVectors[i];
            const auto& match = m_lastEnhancedValidation.enhancedMatches[i];
            
            distortionInfo += QString("Star %1: (%.1f, %.1f) → (%.3f, %.3f)\n")
                             .arg(match.detectedIndex)
                             .arg(m_lastStarMask.starCenters[match.detectedIndex].x())
                             .arg(m_lastStarMask.starCenters[match.detectedIndex].y())
                             .arg(residual.x()).arg(residual.y());
        }
    }
    
    distortionText->setPlainText(distortionInfo);
    layout->addWidget(distortionText);
    
    // Calibrate distortion model button
    QPushButton* calibrateButton = new QPushButton("Calibrate Distortion Model");
    calibrateButton->setToolTip("Calculate distortion correction parameters");
    connect(calibrateButton, &QPushButton::clicked, this, &MainWindow::onCalibrateDistortionModel);
    layout->addWidget(calibrateButton);
    
    QPushButton* closeButton = new QPushButton("Close");
    connect(closeButton, &QPushButton::clicked, distortionDialog, &QDialog::accept);
    layout->addWidget(closeButton);
    
    distortionDialog->exec();
    delete distortionDialog;
}

void MainWindow::onCalibrateDistortionModel()
{
    if (!m_enhancedValidationComplete) {
        QMessageBox::information(this, "Distortion Calibration", 
                                "Please perform enhanced validation first.");
        return;
    }
    
    try {
        bool success = m_catalogValidator->calibrateDistortionModel(m_lastEnhancedValidation);
        
        if (success) {
            QMessageBox::information(this, "Distortion Calibration", 
                                    "Distortion model calibrated successfully!\n\n"
                                    "The model will be applied to future coordinate transformations.");
        } else {
            QMessageBox::warning(this, "Distortion Calibration", 
                                "Failed to calibrate distortion model.\n\n"
                                "Not enough high-quality matches or data is insufficient.");
        }
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Distortion Calibration Error", 
                             QString("Error during calibration: %1").arg(e.what()));
    }
}

void MainWindow::onExportMatchingResults()
{
    if (!m_enhancedValidationComplete) {
        QMessageBox::information(this, "Export Results", 
                                "Please perform enhanced validation first.");
        return;
    }
    
    QString fileName = QFileDialog::getSaveFileName(
        this, "Export Enhanced Matching Results",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/star_matching_results.json",
        "JSON Files (*.json);;CSV Files (*.csv);;All Files (*)");
    
    if (fileName.isEmpty()) return;
    
    try {
        QFileInfo fileInfo(fileName);
        QString suffix = fileInfo.suffix().toLower();
        
        if (suffix == "json") {
            // Export as JSON
            QJsonObject exportData;
            exportData["enhanced_summary"] = m_lastEnhancedValidation.enhancedSummary;
            exportData["total_detected"] = m_lastEnhancedValidation.totalDetected;
            exportData["total_catalog"] = m_lastEnhancedValidation.totalCatalog;
            exportData["total_matches"] = m_lastEnhancedValidation.totalMatches;
            exportData["matching_confidence"] = m_lastEnhancedValidation.matchingConfidence;
            exportData["geometric_rms"] = m_lastEnhancedValidation.geometricRMS;
            exportData["astrometric_accuracy"] = m_lastEnhancedValidation.astrometricAccuracy;
            
            // Add matches
            QJsonArray matchesArray;
            for (const auto& match : m_lastEnhancedValidation.enhancedMatches) {
                QJsonObject matchObj;
                matchObj["detected_index"] = match.detectedIndex;
                matchObj["catalog_index"] = match.catalogIndex;
                matchObj["pixel_distance"] = match.pixelDistance;
                matchObj["confidence"] = match.confidence;
                matchObj["triangle_error"] = match.triangleError;
                matchObj["geometrically_valid"] = match.isGeometricallyValid;
                matchObj["photometrically_valid"] = match.isPhotometricallyValid;
                matchesArray.append(matchObj);
            }
	}
    } catch (const std::exception& e) {
        QString errorMsg = QString("Export Matching results: %1").arg(e.what());
        m_statusLabel->setText(errorMsg);
        QMessageBox::warning(this, "Eexport Matching results", errorMsg);
    }
}

void MainWindow::setupStarDetectionControls()
{
    m_starDetectionGroup = new QGroupBox("PCL Star Detection Controls");
    m_starDetectionLayout = new QVBoxLayout(m_starDetectionGroup);
    
    // Sensitivity control
    QHBoxLayout* sensitivityLayout = new QHBoxLayout;
    sensitivityLayout->addWidget(new QLabel("Sensitivity:"));
    m_sensitivitySlider = new QSlider(Qt::Horizontal);
    m_sensitivitySlider->setRange(0, 100);
    m_sensitivitySlider->setValue(50); // 0.5 default
    m_sensitivitySlider->setToolTip("Star detection sensitivity (0=minimum, 100=maximum)");
    QLabel* sensitivityValue = new QLabel("0.5");
    sensitivityValue->setMinimumWidth(30);
    connect(m_sensitivitySlider, &QSlider::valueChanged, [sensitivityValue](int value) {
        sensitivityValue->setText(QString::number(value / 100.0, 'f', 2));
    });
    sensitivityLayout->addWidget(m_sensitivitySlider);
    sensitivityLayout->addWidget(sensitivityValue);
    m_starDetectionLayout->addLayout(sensitivityLayout);
    
    // Structure layers control
    QHBoxLayout* structureLayout = new QHBoxLayout;
    structureLayout->addWidget(new QLabel("Structure Layers:"));
    m_structureLayersSpinBox = new QSpinBox;
    m_structureLayersSpinBox->setRange(1, 8);
    m_structureLayersSpinBox->setValue(5);
    m_structureLayersSpinBox->setToolTip("Number of wavelet layers for structure detection (1=small stars, 8=large stars)");
    structureLayout->addWidget(m_structureLayersSpinBox);
    structureLayout->addStretch();
    m_starDetectionLayout->addLayout(structureLayout);
    
    // Noise layers control
    QHBoxLayout* noiseLayout = new QHBoxLayout;
    noiseLayout->addWidget(new QLabel("Noise Layers:"));
    m_noiseLayersSpinBox = new QSpinBox;
    m_noiseLayersSpinBox->setRange(0, 4);
    m_noiseLayersSpinBox->setValue(1);
    m_noiseLayersSpinBox->setToolTip("Number of wavelet layers for noise reduction (0=none, 4=maximum)");
    noiseLayout->addWidget(m_noiseLayersSpinBox);
    noiseLayout->addStretch();
    m_starDetectionLayout->addLayout(noiseLayout);
    
    // Peak response control
    QHBoxLayout* peakLayout = new QHBoxLayout;
    peakLayout->addWidget(new QLabel("Peak Response:"));
    m_peakResponseSlider = new QSlider(Qt::Horizontal);
    m_peakResponseSlider->setRange(0, 100);
    m_peakResponseSlider->setValue(50); // 0.5 default
    m_peakResponseSlider->setToolTip("Peak response sensitivity (0=sharp peaks only, 100=flat features allowed)");
    QLabel* peakValue = new QLabel("0.5");
    peakValue->setMinimumWidth(30);
    connect(m_peakResponseSlider, &QSlider::valueChanged, [peakValue](int value) {
        peakValue->setText(QString::number(value / 100.0, 'f', 2));
    });
    peakLayout->addWidget(m_peakResponseSlider);
    peakLayout->addWidget(peakValue);
    m_starDetectionLayout->addLayout(peakLayout);
    
    // Max distortion control
    QHBoxLayout* distortionLayout = new QHBoxLayout;
    distortionLayout->addWidget(new QLabel("Max Distortion:"));
    m_maxDistortionSlider = new QSlider(Qt::Horizontal);
    m_maxDistortionSlider->setRange(0, 100);
    m_maxDistortionSlider->setValue(80); // 0.8 default
    m_maxDistortionSlider->setToolTip("Maximum allowed star distortion (0=circular only, 100=any shape)");
    QLabel* distortionValue = new QLabel("0.8");
    distortionValue->setMinimumWidth(30);
    connect(m_maxDistortionSlider, &QSlider::valueChanged, [distortionValue](int value) {
        distortionValue->setText(QString::number(value / 100.0, 'f', 2));
    });
    distortionLayout->addWidget(m_maxDistortionSlider);
    distortionLayout->addWidget(distortionValue);
    m_starDetectionLayout->addLayout(distortionLayout);
    
    // PSF fitting control
    m_enablePSFFittingCheck = new QCheckBox("Enable PSF Fitting");
    m_enablePSFFittingCheck->setChecked(true);
    m_enablePSFFittingCheck->setToolTip("Enable Point Spread Function fitting for better centroid accuracy");
    m_starDetectionLayout->addWidget(m_enablePSFFittingCheck);
    
    // Detection buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    m_detectAdvancedButton = new QPushButton("Detect Stars (Advanced)");
    m_detectAdvancedButton->setToolTip("Use PCL StarDetector with current parameters");
    m_detectSimpleButton = new QPushButton("Detect Stars (Simple)");
    m_detectSimpleButton->setToolTip("Use fallback simple detection algorithm");
    
    buttonLayout->addWidget(m_detectAdvancedButton);
    buttonLayout->addWidget(m_detectSimpleButton);
    m_starDetectionLayout->addLayout(buttonLayout);
    
    // Connect signals
    connect(m_detectAdvancedButton, &QPushButton::clicked, this, &MainWindow::onDetectStarsAdvanced);
    connect(m_detectSimpleButton, &QPushButton::clicked, this, &MainWindow::onDetectStarsSimple);
    
    // Initially disable until image is loaded
    m_starDetectionGroup->setEnabled(false);
}

// Add this method to handle advanced star detection
void MainWindow::onDetectStarsAdvanced()
{
    if (!m_imageReader->hasImage()) {
        m_statusLabel->setText("No image loaded.");
        return;
    }

    m_statusLabel->setText("Detecting stars with PCL StarDetector...");
    QApplication::processEvents();

    // Get parameters from UI controls
    float sensitivity = m_sensitivitySlider->value() / 100.0f;
    int structureLayers = m_structureLayersSpinBox->value();
    int noiseLayers = m_noiseLayersSpinBox->value();
    float peakResponse = m_peakResponseSlider->value() / 100.0f;
    float maxDistortion = m_maxDistortionSlider->value() / 100.0f;
    bool enablePSFFitting = m_enablePSFFittingCheck->isChecked();

    qDebug() << "Starting advanced star detection with parameters:";
    qDebug() << "  Sensitivity:" << sensitivity;
    qDebug() << "  Structure layers:" << structureLayers;
    qDebug() << "  Noise layers:" << noiseLayers;
    qDebug() << "  Peak response:" << peakResponse;
    qDebug() << "  Max distortion:" << maxDistortion;
    qDebug() << "  PSF fitting:" << enablePSFFitting;

    // Use our advanced StarMaskGenerator method
    m_lastStarMask = StarMaskGenerator::detectStarsAdvanced(
        m_imageReader->imageData(),
        sensitivity,
        structureLayers,
        noiseLayers,
        peakResponse,
        maxDistortion,
        enablePSFFitting
    );

    m_imageDisplayWidget->setStarOverlay(m_lastStarMask.starCenters, m_lastStarMask.starRadii);
    m_starsDetected = !m_lastStarMask.starCenters.isEmpty();
    
    // Update status with detailed information
    QString statusText = QString("PCL StarDetector: %1 stars detected").arg(m_lastStarMask.starCenters.size());
    if (m_lastStarMask.starCenters.size() > 0) {
        statusText += " - Use checkboxes to toggle display";
        
        // Add parameter summary to results text
        QString paramSummary = QString("PCL StarDetector Parameters:\n"
                                      "  Sensitivity: %1\n"
                                      "  Structure Layers: %2\n"
                                      "  Noise Layers: %3\n"
                                      "  Peak Response: %4\n"
                                      "  Max Distortion: %5\n"
                                      "  PSF Fitting: %6\n\n"
                                      "Results: %7 stars detected")
                              .arg(sensitivity, 0, 'f', 2)
                              .arg(structureLayers)
                              .arg(noiseLayers)
                              .arg(peakResponse, 0, 'f', 2)
                              .arg(maxDistortion, 0, 'f', 2)
                              .arg(enablePSFFitting ? "Enabled" : "Disabled")
                              .arg(m_lastStarMask.starCenters.size());
        
        m_resultsText->setPlainText(paramSummary);
    }
    m_statusLabel->setText(statusText);
    
    updateValidationControls();
}

// Add this method to handle simple star detection
void MainWindow::onDetectStarsSimple()
{
    if (!m_imageReader->hasImage()) {
        m_statusLabel->setText("No image loaded.");
        return;
    }

    m_statusLabel->setText("Detecting stars with simple algorithm...");
    QApplication::processEvents();

    // Use the basic detection method with sensitivity from slider
    float sensitivity = m_sensitivitySlider->value() / 100.0f;
    m_lastStarMask = StarMaskGenerator::detectStars(m_imageReader->imageData(), sensitivity);

    m_imageDisplayWidget->setStarOverlay(m_lastStarMask.starCenters, m_lastStarMask.starRadii);
    m_starsDetected = !m_lastStarMask.starCenters.isEmpty();
    
    // Update status
    QString statusText = QString("Simple detection: %1 stars detected (sensitivity: %2)")
                        .arg(m_lastStarMask.starCenters.size())
                        .arg(sensitivity, 0, 'f', 2);
    if (m_lastStarMask.starCenters.size() > 0) {
        statusText += " - Use checkboxes to toggle display";
        
        QString resultsSummary = QString("Simple Star Detection:\n"
                                       "  Sensitivity: %1\n"
                                       "  Algorithm: Local maxima detection\n\n"
                                       "Results: %2 stars detected")
                               .arg(sensitivity, 0, 'f', 2)
                               .arg(m_lastStarMask.starCenters.size());
        
        m_resultsText->setPlainText(resultsSummary);
    }
    m_statusLabel->setText(statusText);
    
    updateValidationControls();
}

// Update your existing setupUI() method to include the star detection controls
void MainWindow::setupUI()
{
    m_centralWidget = new QWidget;
    setCentralWidget(m_centralWidget);
    
    // Create main splitter
    QSplitter* mainSplitter = new QSplitter(Qt::Horizontal);
    
    // Left side - image display
    QWidget* leftWidget = new QWidget;
    QVBoxLayout* leftLayout = new QVBoxLayout(leftWidget);
    
    m_imageDisplayWidget = new ImageDisplayWidget;
    leftLayout->addWidget(m_imageDisplayWidget);
    
    // Button layout
    m_buttonLayout = new QHBoxLayout;
    m_loadButton = new QPushButton("Load Image");
    m_validateButton = new QPushButton("Validate with Catalog");
    m_plotCatalogButton = new QPushButton("Plot Catalog Stars");
    
    // Remove the old simple detect button since it's now in the controls
    m_validateButton->setEnabled(false);
    m_plotCatalogButton->setEnabled(false);
    
    m_buttonLayout->addWidget(m_loadButton);
    m_buttonLayout->addWidget(m_validateButton);
    m_buttonLayout->addWidget(m_plotCatalogButton);
    m_buttonLayout->addStretch();
    
    leftLayout->addLayout(m_buttonLayout);
    
    // Mode toggle
    QHBoxLayout* modeLayout = new QHBoxLayout;
    m_plotModeCheck = new QCheckBox("Catalog Plotting Mode");
    m_plotModeCheck->setToolTip("Switch between star detection/validation and direct catalog plotting");
    modeLayout->addWidget(m_plotModeCheck);
    modeLayout->addStretch();
    leftLayout->addLayout(modeLayout);
    
    // Status label
    m_statusLabel = new QLabel("Ready - Load an image to begin");
    leftLayout->addWidget(m_statusLabel);
    
    // Right side - controls and results
    QWidget* rightWidget = new QWidget;
    rightWidget->setMaximumWidth(400); // Increased width for more controls
    rightWidget->setMinimumWidth(350);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightWidget);
    
    // Add star detection controls
    setupStarDetectionControls();
    rightLayout->addWidget(m_starDetectionGroup);
    
    setupValidationControls();
    setupCatalogPlottingControls();
    
    rightLayout->addWidget(m_validationGroup);
    rightLayout->addWidget(m_plottingGroup);
    
    // Results display
    QGroupBox* resultsGroup = new QGroupBox("Results");
    QVBoxLayout* resultsLayout = new QVBoxLayout(resultsGroup);
    
    m_resultsText = new QTextEdit;
    m_resultsText->setMaximumHeight(200);
    m_resultsText->setReadOnly(true);
    m_resultsText->setPlainText("No results yet.\n\nLoad an image with WCS information and either:\n- Detect stars for validation, or\n- Plot catalog stars directly");
    
    resultsLayout->addWidget(m_resultsText);
    rightLayout->addWidget(resultsGroup);
    
    rightLayout->addStretch();
    
    // Add widgets to splitter
    mainSplitter->addWidget(leftWidget);
    mainSplitter->addWidget(rightWidget);
    mainSplitter->setStretchFactor(0, 1); // Image display gets most space
    mainSplitter->setStretchFactor(1, 0); // Controls panel fixed width
    
    // Main layout
    m_mainLayout = new QVBoxLayout(m_centralWidget);
    m_mainLayout->addWidget(mainSplitter);
}

// Update your onLoadImage method to enable star detection controls
void MainWindow::onLoadImage()
{
    pcl_mock::InitializeMockAPI();
    
    QString filePath = QFileDialog::getOpenFileName(
        this,
        "Open Image File",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        ImageReader::formatFilter()
    );
    
    if (filePath.isEmpty())
        return;

    if (!m_imageReader->readFile(filePath)) {
        m_statusLabel->setText("Failed to load image: " + m_imageReader->lastError());
        QMessageBox::warning(this, "Load Error", m_imageReader->lastError());
        return;
    }

    // Store pointer to image data
    m_imageData = &m_imageReader->imageData();
    
    m_imageDisplayWidget->setImageData(m_imageReader->imageData());
    m_statusLabel->setText("Image loaded successfully - Choose detection method");
    
    // Extract WCS information
    extractWCSFromImage();
    
    // Update UI state
    m_starDetectionGroup->setEnabled(true);  // Enable star detection controls
    m_plotCatalogButton->setEnabled(m_hasWCS);
    m_starsDetected = false;
    m_catalogQueried = false;
    m_validationComplete = false;
    m_catalogPlotted = false;
    
    updateValidationControls();
    updatePlottingControls();
    updateStatusDisplay();
    
    // Clear previous results
    m_imageDisplayWidget->clearStarOverlay();
    m_imageDisplayWidget->clearValidationResults();
    m_resultsText->clear();
}

// Replace the setup2MASSCatalog method with setupGaiaDR3Catalog:
void MainWindow::setupGaiaDR3Catalog()
{
    // Configure the Gaia GDR3 catalog path
    QString catalogPath = "/Volumes/X10Pro/gdr3-1.0.0-01.xpsd";
    
    // Check if catalog file exists
    if (QFile::exists(catalogPath)) {
        qDebug() << "✅ Found Gaia GDR3 catalog at:" << catalogPath;
        
        QFileInfo info(catalogPath);
        double sizeMB = info.size() / (1024.0 * 1024.0);
        qDebug() << QString("📊 Catalog size: %.1f MB").arg(sizeMB);
        
        // Set the catalog path
        GaiaGDR3Catalog::setCatalogPath(catalogPath);
	/*        
        // Test database connection
        if (GaiaGDR3Catalog::validateDatabase()) {
            qDebug() << "✅ Gaia GDR3 database validation successful";
            
            // Get detailed info
            QString info = GaiaGDR3Catalog::getCatalogInfo();
            qDebug() << "📊 Catalog details:";
            for (const QString& line : info.split('\n')) {
                if (!line.isEmpty()) {
                    qDebug() << "   " << line;
                }
            }
            
            // Update status
            m_statusLabel->setText(QString("Gaia GDR3 catalog ready (%.1f MB)").arg(sizeMB));
        } else {
            qDebug() << "❌ Gaia GDR3 database validation failed";
            m_statusLabel->setText("Gaia GDR3 catalog found but validation failed");
        }
        */
    } else {
        qDebug() << "❌ Gaia GDR3 catalog not found at:" << catalogPath;
        qDebug() << "   Available catalog sources:";
        
        // Check alternative locations
        QStringList possiblePaths = {
            "/Volumes/X10Pro/gdr3-1.0.0-01.xpsd",
            "./gdr3-1.0.0-01.xpsd",
            "../gdr3-1.0.0-01.xpsd",
            QDir::homePath() + "/gdr3-1.0.0-01.xpsd",
            "/usr/local/share/pixinsight/gaia/gdr3-1.0.0-01.xpsd"
        };
        
        bool found = false;
        for (const QString& path : possiblePaths) {
            if (QFile::exists(path)) {
                qDebug() << "✅ Found alternative at:" << path;
                GaiaGDR3Catalog::setCatalogPath(path);
                found = true;
                break;
            }
        }
        
        if (!found) {
            qDebug() << "   - Hipparcos (network)";
            qDebug() << "   - Gaia DR3 (network)";
            qDebug() << "   - Tycho-2 (network)";
            
            // Default to network catalogs if no local Gaia
            m_statusLabel->setText("Using network catalogs (Gaia GDR3 not found)");
        }
    }
}

// Update the catalog menu setup for Gaia:
void MainWindow::setupCatalogMenu()
{
    // Add a menu bar item for catalog management
    QMenuBar* menuBar = this->menuBar();
    QMenu* catalogMenu = menuBar->addMenu("&Catalog");
    
    QAction* browseGaiaAction = catalogMenu->addAction("Browse for Gaia GDR3 catalog...");
    connect(browseGaiaAction, &QAction::triggered, this, &MainWindow::browseGaiaCatalogFile);
    
    catalogMenu->addSeparator();
    
    QAction* showStatsAction = catalogMenu->addAction("Show catalog statistics");
    connect(showStatsAction, &QAction::triggered, [this]() {
        m_catalogValidator->showCatalogStats();
    });
    
    QAction* testQueryAction = catalogMenu->addAction("Test Gaia query");
    connect(testQueryAction, &QAction::triggered, [this]() {
        if (m_hasWCS) {
            WCSData wcs = m_catalogValidator->getWCSData();
            qDebug() << "\n=== TESTING GAIA GDR3 QUERY ===";
            qDebug() << QString("Image center: RA=%.4f° Dec=%.4f°").arg(wcs.crval1).arg(wcs.crval2);
            
            auto start = QTime::currentTime();
            m_catalogValidator->queryCatalog(wcs.crval1, wcs.crval2, 1.0); // 1 degree radius test
            auto elapsed = start.msecsTo(QTime::currentTime());
            
            qDebug() << QString("Query completed in %1ms").arg(elapsed);
        } else {
            QMessageBox::information(this, "Test Query", "Load an image with WCS first");
        }
    });
    
    catalogMenu->addSeparator();
    
    QAction* findBrightAction = catalogMenu->addAction("Find brightest stars in field");
    connect(findBrightAction, &QAction::triggered, [this]() {
        if (m_hasWCS) {
            WCSData wcs = m_catalogValidator->getWCSData();
            double fieldRadius = sqrt(wcs.width * wcs.width + wcs.height * wcs.height) * wcs.pixscale / 3600.0 / 2.0;
            fieldRadius = std::max(fieldRadius, 0.5);
            
            m_catalogValidator->findBrightGaiaStars(wcs.crval1, wcs.crval2, fieldRadius, 20);
        } else {
            QMessageBox::information(this, "Bright Stars", "Load an image with WCS first");
        }
    });
    
    QAction* findSpectraAction = catalogMenu->addAction("Find stars with BP/RP spectra");
    connect(findSpectraAction, &QAction::triggered, [this]() {
        if (m_hasWCS) {
            WCSData wcs = m_catalogValidator->getWCSData();
            double fieldRadius = sqrt(wcs.width * wcs.width + wcs.height * wcs.height) * wcs.pixscale / 3600.0 / 2.0;
            fieldRadius = std::max(fieldRadius, 0.5);
            
            m_catalogValidator->queryGaiaWithSpectra(wcs.crval1, wcs.crval2, fieldRadius);
        } else {
            QMessageBox::information(this, "Spectral Stars", "Load an image with WCS first");
        }
    });
    
    catalogMenu->addSeparator();
    
    QAction* performanceAction = catalogMenu->addAction("Test Gaia performance");
    connect(performanceAction, &QAction::triggered, this, &MainWindow::testGaiaPerformance);
}

// Add file browser for Gaia catalog:
void MainWindow::browseGaiaCatalogFile()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        "Select Gaia GDR3 Catalog File",
        "/Volumes/X10Pro/",
        "Gaia Catalog Files (*.xpsd);;All Files (*)"
    );
    
    if (!filePath.isEmpty()) {
        GaiaGDR3Catalog::setCatalogPath(filePath);
        
        QFileInfo info(filePath);
        double sizeMB = info.size() / (1024.0 * 1024.0);
        
        qDebug() << "📂 User selected Gaia catalog:" << filePath;
        qDebug() << QString("📊 Size: %.1f MB").arg(sizeMB);
        
        // Validate the new catalog
        if (GaiaGDR3Catalog::validateDatabase()) {
            m_statusLabel->setText(QString("Custom Gaia GDR3 catalog loaded (%.1f MB)").arg(sizeMB));
            qDebug() << "✅ Custom Gaia catalog validation successful";
        } else {
            m_statusLabel->setText("Gaia catalog validation failed");
            qDebug() << "❌ Custom Gaia catalog validation failed";
        }
        
        // Clear previous catalog data
        m_catalogQueried = false;
        m_catalogPlotted = false;
        updateValidationControls();
        updatePlottingControls();
    }
}

// Add performance testing method:
void MainWindow::testGaiaPerformance()
{
    if (!m_hasWCS) {
        QMessageBox::information(this, "Performance Test", "Load an image with WCS first");
        return;
    }
    
    WCSData wcs = m_catalogValidator->getWCSData();
    double testRadius = 2.0; // 2 degree radius for performance test
    
    qDebug() << "\n=== GAIA GDR3 PERFORMANCE COMPARISON ===";
    qDebug() << QString("Test query: RA=%.4f° Dec=%.4f° radius=%.1f°")
                .arg(wcs.crval1).arg(wcs.crval2).arg(testRadius);
    
    // Test different magnitude limits
    QVector<double> magLimits = {12.0, 15.0, 18.0, 20.0};
    
    for (double magLimit : magLimits) {
        auto start = QTime::currentTime();
        
        GaiaGDR3Catalog::SearchParameters params(wcs.crval1, wcs.crval2, testRadius, magLimit);
        auto stars = GaiaGDR3Catalog::queryRegion(params);
        
        auto elapsed = start.msecsTo(QTime::currentTime());
        
        qDebug() << QString("🚀 Mag ≤ %.1f: %1 stars in %2ms (%.1f stars/sec)")
                    .arg(magLimit).arg(stars.size()).arg(elapsed)
                    .arg(stars.size() * 1000.0 / elapsed);
    }
    
    // Test spectrum search
    auto start = QTime::currentTime();
    auto specStars = GaiaGDR3Catalog::findStarsWithSpectra(wcs.crval1, wcs.crval2, testRadius, 15.0);
    auto elapsed = start.msecsTo(QTime::currentTime());
    
    qDebug() << QString("🌈 BP/RP spectra search: %1 stars in %2ms")
                .arg(specStars.size()).arg(elapsed);
    
    qDebug() << "📊 Gaia GDR3 provides:";
    qDebug() << "   - Precise astrometry (positions, proper motions, parallax)";
    qDebug() << "   - Multi-band photometry (G, BP, RP)";
    qDebug() << "   - BP/RP low-resolution spectra for many stars";
    qDebug() << "   - Quality flags and error estimates";
    qDebug() << "   - Proper motion corrections to current epoch";
}

// Update the destructor to clean up Gaia resources:
MainWindow::~MainWindow() 
{
    // Clean up Gaia catalog resources
    GaiaGDR3Catalog::shutdown();
}

// Test function you can add to MainWindow or run separately
void testGaiaGDR3Integration()
{
    qDebug() << "\n=== GAIA GDR3 INTEGRATION TEST ===";
    
    // 1. Set up the catalog path
    QString catalogPath = "/Volumes/X10Pro/gdr3-1.0.0-01.xpsd";
    GaiaGDR3Catalog::setCatalogPath(catalogPath);
    
    if (!GaiaGDR3Catalog::isAvailable()) {
        qDebug() << "❌ Gaia catalog not available at:" << catalogPath;
        return;
    }
    
    qDebug() << "✅ Catalog info:";
    qDebug() << GaiaGDR3Catalog::getCatalogInfo();
    
    // 2. Test basic query around M31 (Andromeda Galaxy)
    qDebug() << "\n--- Testing M31 Region Query ---";
    double m31_ra = 10.684708;   // M31 RA
    double m31_dec = 41.268750;  // M31 Dec
    
    auto start = QTime::currentTime();
    auto m31_stars = GaiaGDR3Catalog::queryRegion(m31_ra, m31_dec, 0.5, 15.0);
    auto elapsed = start.msecsTo(QTime::currentTime());
    
    qDebug() << QString("Found %1 stars around M31 in %2ms").arg(m31_stars.size()).arg(elapsed);
    
    if (!m31_stars.isEmpty()) {
        qDebug() << "Brightest 5 stars:";
        for (int i = 0; i < qMin(5, m31_stars.size()); ++i) {
            const auto& star = m31_stars[i];
            qDebug() << QString("  %1: RA=%.4f° Dec=%.4f° G=%.2f %2")
                        .arg(star.sourceId).arg(star.ra).arg(star.dec)
                        .arg(star.magnitude).arg(star.spectralClass);
        }
    }
    
    // 3. Test bright star search around Polaris
    qDebug() << "\n--- Testing Bright Stars Around Polaris ---";
    double polaris_ra = 37.954;   // Polaris RA  
    double polaris_dec = 89.264;  // Polaris Dec
    
    start = QTime::currentTime();
    auto bright_stars = GaiaGDR3Catalog::findBrightestStars(polaris_ra, polaris_dec, 2.0, 10);
    elapsed = start.msecsTo(QTime::currentTime());
    
    qDebug() << QString("Found %1 bright stars around Polaris in %2ms").arg(bright_stars.size()).arg(elapsed);
    
    for (const auto& star : bright_stars) {
        qDebug() << QString("  G=%.2f %1 at (%.4f°, %.4f°) PM=(%.1f, %.1f) mas/yr")
                    .arg(star.spectralClass).arg(star.magnitude)
                    .arg(star.ra).arg(star.dec).arg(star.pmRA).arg(star.pmDec);
    }
    
    // 4. Test spectrum search around Vega
    qDebug() << "\n--- Testing Spectrum Search Around Vega ---";
    double vega_ra = 279.234;    // Vega RA
    double vega_dec = 38.784;    // Vega Dec
    
    start = QTime::currentTime();
    auto spectral_stars = GaiaGDR3Catalog::findStarsWithSpectra(vega_ra, vega_dec, 1.0, 12.0);
    elapsed = start.msecsTo(QTime::currentTime());
    
    qDebug() << QString("Found %1 stars with BP/RP spectra around Vega in %2ms").arg(spectral_stars.size()).arg(elapsed);
    
    for (const auto& star : spectral_stars) {
        qDebug() << QString("  %1: G=%.2f BP=%.2f RP=%.2f %2 [SPECTRUM]")
                    .arg(star.sourceId).arg(star.magnitude)
                    .arg(star.magBP).arg(star.magRP).arg(star.spectralClass);
    }
    
    // 5. Test performance with large search
    qDebug() << "\n--- Testing Performance ---";
    GaiaGDR3Catalog::testPerformance(0.0, 0.0, 5.0); // Large search around celestial equator
    
    // 6. Test proper motion application
    qDebug() << "\n--- Testing Proper Motion Correction ---";
    GaiaGDR3Catalog::SearchParameters params;
    params.centerRA = 266.417;  // Galactic center
    params.centerDec = -29.008;
    params.radiusDegrees = 1.0;
    params.maxMagnitude = 10.0;
    params.useProperMotion = true;
    params.epochYear = 2025.5;  // Current epoch
    params.maxResults = 5;
    
    auto pm_stars = GaiaGDR3Catalog::queryRegion(params);
    qDebug() << QString("Found %1 stars with proper motion applied to epoch %.1f")
                .arg(pm_stars.size()).arg(params.epochYear);
    
    for (const auto& star : pm_stars) {
        qDebug() << QString("  %1: G=%.2f at (%.6f°, %.6f°) PM=(%.1f, %.1f)")
                    .arg(star.sourceId).arg(star.magnitude)
                    .arg(star.ra).arg(star.dec).arg(star.pmRA).arg(star.pmDec);
    }
    
    qDebug() << "\n✅ Gaia GDR3 integration test completed successfully!";
}

// Integration example for your existing star validation workflow
void integrateGaiaWithStarValidation()
{
    qDebug() << "\n=== GAIA INTEGRATION WITH STAR VALIDATION ===";
    
    // Example: Your detected stars from star mask
    QVector<QPoint> detectedStars = {
        QPoint(512, 256),   // Example detected star positions
        QPoint(1024, 512),
        QPoint(256, 1024),
        QPoint(1500, 800)
    };
    
    // Example WCS data (replace with your actual WCS)
    double centerRA = 11.195;    // RA of image center
    double centerDec = 41.892;   // Dec of image center
    double pixelScale = 1.2;     // arcsec/pixel
    int imageWidth = 2048;
    int imageHeight = 2048;
    
    // Calculate field radius
    double fieldRadius = sqrt(imageWidth * imageWidth + imageHeight * imageHeight) 
                        * pixelScale / 3600.0 / 2.0; // Convert to degrees
    
    qDebug() << QString("Image field: %.2f° radius around RA=%.3f° Dec=%.3f°")
                .arg(fieldRadius).arg(centerRA).arg(centerDec);
    
    // Query Gaia catalog for this field
    GaiaGDR3Catalog::SearchParameters params;
    params.centerRA = centerRA;
    params.centerDec = centerDec;
    params.radiusDegrees = fieldRadius;
    params.maxMagnitude = 16.0;    // Reasonable limit for star detection
    params.useProperMotion = true; // Apply proper motion to current epoch
    params.epochYear = 2025.5;
    
    auto catalogStars = GaiaGDR3Catalog::queryRegion(params);
    
    qDebug() << QString("Retrieved %1 Gaia stars for validation").arg(catalogStars.size());
    
    // Example validation process (simplified)
    int matches = 0;
    double matchTolerance = 5.0; // pixels
    
    for (const QPoint& detected : detectedStars) {
        for (const auto& catalog : catalogStars) {
            // Convert catalog star RA/Dec to pixel coordinates (simplified)
            // In real implementation, use your WCS transformation
            double pixelX = imageWidth/2 + (catalog.ra - centerRA) * 3600.0 / pixelScale;
            double pixelY = imageHeight/2 + (catalog.dec - centerDec) * 3600.0 / pixelScale;
            
            double distance = sqrt(pow(detected.x() - pixelX, 2) + pow(detected.y() - pixelY, 2));
            
            if (distance < matchTolerance) {
                matches++;
                qDebug() << QString("✅ Match: detected(%1,%2) ↔ Gaia %3 G=%.2f dist=%.1fpx")
                            .arg(detected.x()).arg(detected.y())
                            .arg(catalog.sourceId).arg(catalog.magnitude).arg(distance);
                break;
            }
        }
    }
    
    double matchPercentage = 100.0 * matches / detectedStars.size();
    qDebug() << QString("Validation result: %1/%2 stars matched (%.1f%%)")
                .arg(matches).arg(detectedStars.size()).arg(matchPercentage);
}

// Advanced Gaia features demonstration
void demonstrateAdvancedGaiaFeatures()
{
    qDebug() << "\n=== ADVANCED GAIA GDR3 FEATURES ===";
    
    // 1. Color-magnitude diagram data
    qDebug() << "\n--- Color-Magnitude Diagram Data ---";
    auto stars = GaiaGDR3Catalog::queryRegion(0.0, 0.0, 2.0, 15.0);
    
    if (!stars.isEmpty()) {
        qDebug() << "Sample stars for HR diagram:";
        for (int i = 0; i < qMin(10, stars.size()); ++i) {
            const auto& star = stars[i];
            double bpRpColor = star.magBP - star.magRP;
            double absoluteG = star.magnitude; // Would need distance for absolute magnitude
            
            qDebug() << QString("  G=%.2f BP-RP=%.3f %1 plx=%.2f")
                        .arg(absoluteG).arg(bpRpColor)
                        .arg(star.spectralClass).arg(star.parallax);
        }
    }
    
    // 2. Proper motion analysis
    qDebug() << "\n--- High Proper Motion Stars ---";
    auto highPMStars = GaiaGDR3Catalog::queryRegion(83.633, 22.014, 5.0, 12.0); // Around Aldebaran
    
    QVector<GaiaGDR3Catalog::Star> fastMovers;
    for (const auto& star : highPMStars) {
        double totalPM = sqrt(star.pmRA * star.pmRA + star.pmDec * star.pmDec);
        if (totalPM > 50.0) { // > 50 mas/year
            fastMovers.append(star);
        }
    }
    
    qDebug() << QString("Found %1 stars with PM > 50 mas/year").arg(fastMovers.size());
    for (const auto& star : fastMovers) {
        double totalPM = sqrt(star.pmRA * star.pmRA + star.pmDec * star.pmDec);
        qDebug() << QString("  %1: PM=%.1f mas/yr G=%.2f")
                    .arg(star.sourceId).arg(totalPM).arg(star.magnitude);
    }
    
    // 3. Parallax-based distance estimates
    qDebug() << "\n--- Nearby Stars (Parallax > 10 mas) ---";
    QVector<GaiaGDR3Catalog::Star> nearbyStars;
    for (const auto& star : highPMStars) {
        if (star.parallax > 10.0) { // Closer than ~100 parsecs
            nearbyStars.append(star);
        }
    }
    
    qDebug() << QString("Found %1 nearby stars").arg(nearbyStars.size());
    for (const auto& star : nearbyStars) {
        double distancePc = 1000.0 / star.parallax; // Distance in parsecs
        qDebug() << QString("  %1: %.1f pc G=%.2f %2")
                    .arg(star.sourceId).arg(distancePc)
                    .arg(star.magnitude).arg(star.spectralClass);
    }
    
    // 4. Quality assessment
    qDebug() << "\n--- Data Quality Assessment ---";
    int highQuality = 0;
    int withSpectra = 0;
    int goodAstrometry = 0;
    
    for (const auto& star : stars) {
        if (GaiaGDR3Catalog::isHighQuality(star)) highQuality++;
        if (star.hasSpectrum) withSpectra++;
        if (GaiaGDR3Catalog::hasGoodAstrometry(star)) goodAstrometry++;
    }
    
    qDebug() << QString("Quality statistics from %1 stars:").arg(stars.size());
    qDebug() << QString("  High quality: %1 (%.1f%%)").arg(highQuality).arg(100.0 * highQuality / stars.size());
    qDebug() << QString("  With BP/RP spectra: %1 (%.1f%%)").arg(withSpectra).arg(100.0 * withSpectra / stars.size());
    qDebug() << QString("  Good astrometry: %1 (%.1f%%)").arg(goodAstrometry).arg(100.0 * goodAstrometry / stars.size());
}

// Performance comparison between different catalog sources
void comparePerformanceWithOtherCatalogs()
{
    qDebug() << "\n=== CATALOG PERFORMANCE COMPARISON ===";
    
    double testRA = 83.633;     // Aldebaran region
    double testDec = 22.014;
    double testRadius = 1.0;    // 1 degree radius
    double magLimit = 15.0;
    
    qDebug() << QString("Test region: RA=%.3f° Dec=%.3f° radius=%.1f° mag≤%.1f")
                .arg(testRA).arg(testDec).arg(testRadius).arg(magLimit);
    
    // Test Gaia GDR3 local database
    if (GaiaGDR3Catalog::isAvailable()) {
        auto start = QTime::currentTime();
        auto gaiaStars = GaiaGDR3Catalog::queryRegion(testRA, testDec, testRadius, magLimit);
        auto elapsed = start.msecsTo(QTime::currentTime());
        
        qDebug() << QString("🚀 Gaia GDR3 (local): %1 stars in %2ms (%.1f stars/sec)")
                    .arg(gaiaStars.size()).arg(elapsed).arg(gaiaStars.size() * 1000.0 / elapsed);
        
        // Analyze data quality
        int withParallax = 0, withPM = 0, withSpectra = 0;
        for (const auto& star : gaiaStars) {
            if (star.parallax > 0) withParallax++;
            if (star.pmRA != 0 || star.pmDec != 0) withPM++;
            if (star.hasSpectrum) withSpectra++;
        }
        
        qDebug() << QString("  Data richness: %1 with parallax, %2 with PM, %3 with spectra")
                    .arg(withParallax).arg(withPM).arg(withSpectra);
    }
    
    qDebug() << "📊 Comparison summary:";
    qDebug() << "  Gaia GDR3: ~1000x faster than network, richest data (astrometry + photometry + spectra)";
    qDebug() << "  2MASS: ~100x faster than network, photometry only (J, H, K bands)";
    qDebug() << "  Network catalogs: Slow (2-10 seconds), limited by bandwidth";
    qDebug() << "  Bright star DB: Fastest (< 1ms), but only brightest stars";
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_imageReader(std::make_unique<ImageReader>())
    , m_catalogValidator(std::make_unique<StarCatalogValidator>(this))
    , m_imageData(nullptr)
    , m_hasWCS(false)
    , m_starsDetected(false)
    , m_catalogQueried(false)
    , m_validationComplete(false)
    , m_plotMode(false)
    , m_catalogPlotted(false)
{
    setupUI();
    setupGaiaDR3Catalog();  // Add this line
    setupCatalogMenu();   // Add this line
    
    // Connect image reader signals
    connect(m_loadButton, &QPushButton::clicked, this, &MainWindow::onLoadImage);
    connect(m_validateButton, &QPushButton::clicked, this, &MainWindow::onValidateStars);
    connect(m_plotCatalogButton, &QPushButton::clicked, this, &MainWindow::onPlotCatalogStars);
    
    // Connect mode control
    connect(m_plotModeCheck, &QCheckBox::toggled, this, &MainWindow::onPlotModeToggled);
    
    // Connect validation control signals
    connect(m_validationModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onValidationModeChanged);
    connect(m_magnitudeLimitSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onMagnitudeLimitChanged);
    connect(m_pixelToleranceSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onPixelToleranceChanged);
    
    // Connect plotting control signals
    connect(m_plotMagnitudeSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [this](double value) {
                m_catalogValidator->setMagnitudeLimit(value);
                m_catalogQueried = false;
                m_catalogPlotted = false;
                updatePlottingControls();
            });
    connect(m_fieldRadiusSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [this](double) {
                m_catalogQueried = false;
                m_catalogPlotted = false;
                updatePlottingControls();
            });
    
    // Connect catalog validator signals
    connect(m_catalogValidator.get(), &StarCatalogValidator::catalogQueryStarted,
            this, &MainWindow::onCatalogQueryStarted);
    connect(m_catalogValidator.get(), &StarCatalogValidator::catalogQueryFinished,
            this, &MainWindow::onCatalogQueryFinished);
    connect(m_catalogValidator.get(), &StarCatalogValidator::validationCompleted,
            this, &MainWindow::onValidationCompleted);
    connect(m_catalogValidator.get(), &StarCatalogValidator::errorSignal,
            this, &MainWindow::onValidatorError);
    
    // Connect display widget overlay signals
    connect(m_imageDisplayWidget, &ImageDisplayWidget::starOverlayToggled, 
            this, &MainWindow::onStarOverlayToggled);
    connect(m_imageDisplayWidget, &ImageDisplayWidget::catalogOverlayToggled,
            this, &MainWindow::onCatalogOverlayToggled);
    connect(m_imageDisplayWidget, &ImageDisplayWidget::validationOverlayToggled,
            this, &MainWindow::onValidationOverlayToggled);

    resize(1400, 900);
    setWindowTitle("Star Detection & Catalog Validation/Plotting");
    
    updateValidationControls();
    updatePlottingControls();
}

void MainWindow::setupValidationControls()
{
    m_validationGroup = new QGroupBox("Star Detection & Validation (Gaia GDR3)");
    m_validationLayout = new QVBoxLayout(m_validationGroup);
        
    // Validation mode
    QHBoxLayout* modeLayout = new QHBoxLayout;
    modeLayout->addWidget(new QLabel("Validation Mode:"));
    m_validationModeCombo = new QComboBox;
    m_validationModeCombo->addItem("Strict", static_cast<int>(StarCatalogValidator::Strict));
    m_validationModeCombo->addItem("Normal", static_cast<int>(StarCatalogValidator::Normal));
    m_validationModeCombo->addItem("Loose", static_cast<int>(StarCatalogValidator::Loose));
    m_validationModeCombo->setCurrentIndex(1); // Default to Normal
    modeLayout->addWidget(m_validationModeCombo);
    m_validationLayout->addLayout(modeLayout);
    
    // Magnitude limit
    QHBoxLayout* magLayout = new QHBoxLayout;
    magLayout->addWidget(new QLabel("Magnitude Limit:"));
    m_magnitudeLimitSpin = new QSpinBox;
    m_magnitudeLimitSpin->setRange(6, 20);
    m_magnitudeLimitSpin->setValue(12);
    m_magnitudeLimitSpin->setSuffix(" mag");
    magLayout->addWidget(m_magnitudeLimitSpin);
    m_validationLayout->addLayout(magLayout);
    
    // Pixel tolerance
    QHBoxLayout* pixelLayout = new QHBoxLayout;
    pixelLayout->addWidget(new QLabel("Pixel Tolerance:"));
    m_pixelToleranceSpin = new QSpinBox;
    m_pixelToleranceSpin->setRange(1, 20);
    m_pixelToleranceSpin->setValue(5);
    m_pixelToleranceSpin->setSuffix(" px");
    pixelLayout->addWidget(m_pixelToleranceSpin);
    m_validationLayout->addLayout(pixelLayout);
    
    // Progress bar for catalog queries
    m_queryProgressBar = new QProgressBar;
    m_queryProgressBar->setVisible(false);
    m_validationLayout->addWidget(m_queryProgressBar);
    
    // WCS status
    QLabel* wcsStatusLabel = new QLabel("WCS Status: Not available (Required for Gaia queries)");
    wcsStatusLabel->setObjectName("wcsStatusLabel");
    wcsStatusLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
    m_validationLayout->addWidget(wcsStatusLabel);
}

void MainWindow::setupCatalogPlottingControls()
{
    m_plottingGroup = new QGroupBox("Catalog Plotting");
    m_plottingLayout = new QVBoxLayout(m_plottingGroup);
    
    // Magnitude limit for plotting
    QHBoxLayout* plotMagLayout = new QHBoxLayout;
    plotMagLayout->addWidget(new QLabel("Magnitude Limit:"));
    m_plotMagnitudeSpin = new QDoubleSpinBox;
    m_plotMagnitudeSpin->setRange(4.0, 20.0);
    m_plotMagnitudeSpin->setValue(12.0);
    m_plotMagnitudeSpin->setDecimals(1);
    m_plotMagnitudeSpin->setSuffix(" mag");
    plotMagLayout->addWidget(m_plotMagnitudeSpin);
    m_plottingLayout->addLayout(plotMagLayout);
    
    // Field radius
    QHBoxLayout* radiusLayout = new QHBoxLayout;
    radiusLayout->addWidget(new QLabel("Field Radius:"));
    m_fieldRadiusSpin = new QDoubleSpinBox;
    m_fieldRadiusSpin->setRange(0.1, 10.0);
    m_fieldRadiusSpin->setValue(2.0);
    m_fieldRadiusSpin->setDecimals(1);
    m_fieldRadiusSpin->setSuffix(" deg");
    m_fieldRadiusSpin->setToolTip("Search radius around image center");
    radiusLayout->addWidget(m_fieldRadiusSpin);
    m_plottingLayout->addLayout(radiusLayout);
    
    // Initially hide plotting controls
    m_plottingGroup->setVisible(false);
}

void MainWindow::onPlotCatalogStars()
{
    if (!m_hasWCS) {
        QMessageBox::warning(this, "Plot Catalog", "No WCS information available. Cannot plot catalog stars.");
        return;
    }
    
    plotCatalogStarsDirectly();
}

void MainWindow::plotCatalogStarsDirectly()
{
    if (!m_catalogQueried) {
        // Need to query catalog first
        WCSData wcs = m_catalogValidator->getWCSData();
        double fieldRadius = m_fieldRadiusSpin->value();
        
        // Set magnitude limit from plotting controls
        m_catalogValidator->setMagnitudeLimit(m_plotMagnitudeSpin->value());
        
        m_catalogValidator->queryCatalog(wcs.crval1, wcs.crval2, fieldRadius);
    } else {
        // Catalog already queried, just display it
        QVector<CatalogStar> catalogStars = m_catalogValidator->getCatalogStars();
        
        // Create a validation result just for display purposes
        ValidationResult plotResult;
        plotResult.catalogStars = catalogStars;
        plotResult.totalCatalog = catalogStars.size();
        plotResult.isValid = true;
        plotResult.summary = QString("Catalog Plot:\n%1 stars plotted (magnitude ≤ %.1f)\nSource: %2")
                            .arg(catalogStars.size())
	                    .arg(m_plotMagnitudeSpin->value());
        
        m_imageDisplayWidget->setValidationResults(plotResult);
        m_resultsText->setPlainText(plotResult.summary);
        
        m_catalogPlotted = true;
        m_statusLabel->setText(QString("Plotted %1 catalog stars (mag ≤ %.1f)")
                              .arg(catalogStars.size())
                              .arg(m_plotMagnitudeSpin->value()));
        
        updatePlottingControls();
    }
}

void MainWindow::onPlotModeToggled(bool plotMode)
{
    m_plotMode = plotMode;
    
    // Show/hide appropriate control groups
    m_validationGroup->setVisible(!plotMode);
    m_plottingGroup->setVisible(plotMode);
    
    // Update button states
    if (plotMode) {
        m_validateButton->setVisible(false);
        m_plotCatalogButton->setVisible(true);
    } else {
        m_validateButton->setVisible(true);
        m_plotCatalogButton->setVisible(false);
    }
    
    updateValidationControls();
    updatePlottingControls();
    
    // Update status
    QString modeText = plotMode ? "Catalog Plotting Mode" : "Star Detection & Validation Mode";
    if (m_imageData) {
        m_statusLabel->setText(QString("Image loaded - %1").arg(modeText));
    } else {
        m_statusLabel->setText(QString("Ready - %1").arg(modeText));
    }
}

void MainWindow::updatePlottingControls()
{
    bool canPlot = m_hasWCS && m_imageData;
    m_plotCatalogButton->setEnabled(canPlot && !m_queryProgressBar->isVisible());
    
    // Update tooltip
    if (!m_hasWCS) {
        m_plotCatalogButton->setToolTip("No WCS information available in image");
    } else if (!m_imageData) {
        m_plotCatalogButton->setToolTip("Load an image first");
    } else {
        m_plotCatalogButton->setToolTip("Plot catalog stars directly on the image");
    }
}

// ... [Rest of the existing methods remain the same] ...

// Update the MainWindow to use the simplified method
void MainWindow::extractWCSFromImage()
{
    m_hasWCS = false;
    
    if (!m_imageData || m_imageData->metadata.isEmpty()) {
        qDebug() << "No metadata available for WCS extraction";
        return;
    }
    
    qDebug() << "=== Using PCL Native WCS Extraction ===";
    
    // Use PCL's native WCS parsing instead of custom parsing
    m_hasWCS = m_catalogValidator->setWCSFromImageMetadata(*m_imageData);
    
    // Update WCS status display
    QLabel* wcsLabel = findChild<QLabel*>("wcsStatusLabel");
    if (wcsLabel) {
        if (m_hasWCS) {
            WCSData wcs = m_catalogValidator->getWCSData();
            wcsLabel->setText(QString("WCS Status: Available (PCL)\nRA: %1°, Dec: %2°\nPixel Scale: %3 arcsec/px")
                            .arg(wcs.crval1, 0, 'f', 4)
                            .arg(wcs.crval2, 0, 'f', 4)
                            .arg(wcs.pixscale, 0, 'f', 2));
            wcsLabel->setStyleSheet("QLabel { color: green; font-weight: bold; }");
        } else {
            wcsLabel->setText("WCS Status: Failed (PCL)");
            wcsLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
        }
    }
    
    qDebug() << "PCL WCS extraction completed, valid:" << m_hasWCS;
}

void MainWindow::onDetectStars()
{
    if (!m_imageReader->hasImage()) {
        m_statusLabel->setText("No image loaded.");
        return;
    }

    m_statusLabel->setText("Detecting stars...");
    QApplication::processEvents();

    // Use our StarMaskGenerator for detection
    m_lastStarMask = StarMaskGenerator::detectStars(m_imageReader->imageData(), 0.5f);
    m_imageDisplayWidget->setStarOverlay(m_lastStarMask.starCenters, m_lastStarMask.starRadii);

    m_starsDetected = !m_lastStarMask.starCenters.isEmpty();
    
    // Update status
    QString statusText = QString("Detected %1 stars").arg(m_lastStarMask.starCenters.size());
    if (m_lastStarMask.starCenters.size() > 0) {
        statusText += " - Use checkboxes to toggle display";
    }
    m_statusLabel->setText(statusText);
    
    updateValidationControls();
}

void MainWindow::onValidateStars()
{
    if (!m_starsDetected) {
        QMessageBox::information(this, "Validation", "Please detect stars first.");
        return;
    }
    
    if (!m_hasWCS) {
        QMessageBox::warning(this, "Validation", "No WCS information available. Cannot validate against catalog.");
        return;
    }
    
    // Start catalog query if not already done
    if (!m_catalogQueried) {
        WCSData wcs = m_catalogValidator->getWCSData();
        
        // Calculate field radius (diagonal of image)
        double fieldRadius = sqrt(wcs.width * wcs.width + wcs.height * wcs.height) * wcs.pixscale / 3600.0 / 2.0;
        fieldRadius = std::max(fieldRadius, 0.5); // Minimum 0.5 degrees
        
        m_catalogValidator->queryCatalog(wcs.crval1, wcs.crval2, fieldRadius);
    } else {
        // Perform validation with existing catalog data
        performValidation();
    }
}

void MainWindow::performValidation()
{
    ValidationResult result = m_catalogValidator->validateStars(m_lastStarMask.starCenters, m_lastStarMask.starRadii);
    
    if (result.isValid) {
        m_lastValidation = result;
        m_validationComplete = true;
        
        // Update display
        m_imageDisplayWidget->setValidationResults(result);
        
        // Update results text
        m_resultsText->setPlainText(result.summary);
        
        m_statusLabel->setText(QString("Validation complete: %1/%2 stars matched (%.1f%%)")
                              .arg(result.totalMatches)
                              .arg(result.totalDetected)
                              .arg(result.matchPercentage));
    } else {
        QMessageBox::warning(this, "Validation Error", "Validation failed. Check WCS data and try again.");
    }
    
    updateValidationControls();
}

void MainWindow::onValidationModeChanged()
{
    auto mode = static_cast<StarCatalogValidator::ValidationMode>(m_validationModeCombo->currentData().toInt());
    m_catalogValidator->setValidationMode(mode);
}

void MainWindow::onMagnitudeLimitChanged()
{
    m_catalogValidator->setMagnitudeLimit(m_magnitudeLimitSpin->value());
    m_catalogQueried = false; // Need to re-query with new limit
    m_catalogPlotted = false;
    updateValidationControls();
    updatePlottingControls();
}

void MainWindow::onPixelToleranceChanged()
{
    m_catalogValidator->setMatchingTolerance(m_pixelToleranceSpin->value());
}

void MainWindow::onCatalogQueryStarted()
{
    m_queryProgressBar->setVisible(true);
    m_queryProgressBar->setRange(0, 0); // Indeterminate progress
    m_validateButton->setEnabled(false);
    m_plotCatalogButton->setEnabled(false);
    m_statusLabel->setText("Querying star catalog...");
}

// Replace your existing onCatalogQueryFinished method with this:
void MainWindow::onCatalogQueryFinished(bool success, const QString& message)
{
    m_queryProgressBar->setVisible(false);
    m_validateButton->setEnabled(m_starsDetected && m_hasWCS && !m_plotMode);
    m_plotCatalogButton->setEnabled(m_hasWCS && m_plotMode);
    
    if (success) {
        m_catalogQueried = true;
        
        // Add bright stars from local database (this always works!)
        WCSData wcs = m_catalogValidator->getWCSData();
        double fieldRadius = sqrt(wcs.width * wcs.width + wcs.height * wcs.height) * wcs.pixscale / 3600.0 / 2.0;
        fieldRadius = std::max(fieldRadius, 0.5);
        
        m_catalogValidator->addBrightStarsFromDatabase(wcs.crval1, wcs.crval2, fieldRadius);
        
        m_statusLabel->setText(QString("Retrieved %1 catalog stars (including bright stars)")
                               .arg(m_catalogValidator->getCatalogStars().size()));
        
        if (m_plotMode) {
            plotCatalogStarsDirectly();
        } else if (m_starsDetected) {
            performValidation();
        }
    } else {
        m_statusLabel->setText("Catalog query failed: " + message);
        QMessageBox::warning(this, "Catalog Query Error", message);
    }
    
    updateValidationControls();
    updatePlottingControls();
}

void MainWindow::onValidationCompleted(const ValidationResult& result)
{
    m_lastValidation = result;
    m_validationComplete = true;
    
    qDebug() << "Validation completed with" << result.totalMatches << "matches";
}

void MainWindow::onValidatorError(const QString& message)
{
    m_statusLabel->setText("Validation error: " + message);
    QMessageBox::warning(this, "Validation Error", message);
}

void MainWindow::onStarOverlayToggled(bool visible)
{
    if (m_lastStarMask.starCenters.isEmpty()) return;
    
    QString baseText = QString("Detected %1 stars").arg(m_lastStarMask.starCenters.size());
    QString statusText = baseText + (visible ? " - Stars visible" : " - Stars hidden");
    m_statusLabel->setText(statusText);
}

void MainWindow::onCatalogOverlayToggled(bool visible)
{
    if (!m_catalogQueried && !m_catalogPlotted) return;
    
    QVector<CatalogStar> catalogStars = m_catalogValidator->getCatalogStars();
    QString statusText = QString("Catalog stars %1 (%2 total)")
                        .arg(visible ? "visible" : "hidden")
                        .arg(catalogStars.size());
    m_statusLabel->setText(statusText);
}

void MainWindow::onValidationOverlayToggled(bool visible)
{
    if (!m_validationComplete && !m_catalogPlotted) return;
    
    if (m_catalogPlotted) {
        QVector<CatalogStar> catalogStars = m_catalogValidator->getCatalogStars();
        QString statusText = QString("Catalog plot %1 (%2 stars)")
                            .arg(visible ? "visible" : "hidden")
                            .arg(catalogStars.size());
        m_statusLabel->setText(statusText);
    } else {
        QString statusText = QString("Validation results %1 (%2 matches)")
                            .arg(visible ? "visible" : "hidden")
                            .arg(m_lastValidation.totalMatches);
        m_statusLabel->setText(statusText);
    }
}

void MainWindow::updateValidationControls()
{
    if (m_plotMode) {
        // In plot mode, disable validation controls
        m_validateButton->setEnabled(false);
        return;
    }
    
    bool canValidate = m_starsDetected && m_hasWCS;
    m_validateButton->setEnabled(canValidate && !m_queryProgressBar->isVisible());
    
    // Update tooltip
    if (!m_hasWCS) {
        m_validateButton->setToolTip("No WCS information available in image");
    } else if (!m_starsDetected) {
        m_validateButton->setToolTip("Detect stars first");
    } else {
        m_validateButton->setToolTip("Validate detected stars against catalog");
    }
}

void MainWindow::updateStatusDisplay()
{
    QString status = "Ready";
    
    if (m_imageData) {
        status = QString("Image: %1×%2×%3")
                .arg(m_imageData->width)
                .arg(m_imageData->height)
                .arg(m_imageData->channels);
        
        if (m_hasWCS) {
            status += " (WCS available)";
        }
        
        if (m_plotMode) {
            status += " - Catalog Plotting Mode";
            if (m_catalogPlotted) {
                QVector<CatalogStar> catalogStars = m_catalogValidator->getCatalogStars();
                status += QString(", %1 stars plotted").arg(catalogStars.size());
            }
        } else {
            status += " - Detection & Validation Mode";
            if (m_starsDetected) {
                status += QString(", %1 stars detected").arg(m_lastStarMask.starCenters.size());
            }
            
            if (m_validationComplete) {
                status += QString(", %1 matched").arg(m_lastValidation.totalMatches);
            }
        }
    }
    
    m_statusLabel->setText(status);
}

void MainWindow::onMatchingParametersChanged()
{

}

void MainWindow::updateEnhancedMatchingControls()
{
  
}


// Add this to MainWindow.cpp constructor or as a separate setup method

/*
void MainWindow::setupGaiaCatalog()
{
    QString catalogPath = "/Volumes/X10Pro/gdr3-1.0.0-01.xpsd";
    
    // Check if catalog file exists
    if (QFile::exists(catalogPath)) {
        qDebug() << "✅ Found 2MASS catalog at:" << catalogPath;
        
        QFileInfo info(catalogPath);
        double sizeMB = info.size() / (1024.0 * 1024.0);
        qDebug() << QString("📊 Catalog size: %.1f MB").arg(sizeMB);
        
        // Update status
        m_statusLabel->setText(QString("Local 2MASS catalog ready (%.1f MB)").arg(sizeMB));
        
    } else {
        qDebug() << "❌ 2MASS catalog not found at:" << catalogPath;
        qDebug() << "   Available catalog sources:";
        
        // Check alternative locations
        QStringList possiblePaths = {
            "/Volumes/X10Pro/allsky_2mass/allsky.mag",
            "./allsky.mag",
            "../allsky.mag",
            QDir::homePath() + "/allsky.mag"
        };
        
        bool found = false;
        for (const QString& path : possiblePaths) {
            if (QFile::exists(path)) {
                qDebug() << "✅ Found alternative at:" << path;
                Local2MASSCatalog::setCatalogPath(path);
                found = true;
                break;
            }
        }
        
        if (!found) {
            qDebug() << "   - Hipparcos (network)";
            qDebug() << "   - Gaia DR3 (network)";
            qDebug() << "   - Tycho-2 (network)";
            
            // Default to Gaia if no local catalog
            m_statusLabel->setText("Using network catalogs (2MASS not found)");
        }
    }
}

// Add a file browser to let users select their catalog
void MainWindow::browseCatalogFile()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        "Select 2MASS Catalog File",
        "/Volumes/X10Pro/allsky_2mass/",
        "Catalog Files (*.mag *.txt *.csv);;All Files (*)"
    );
    
    if (!filePath.isEmpty()) {
        Local2MASSCatalog::setCatalogPath(filePath);
        
        QFileInfo info(filePath);
        double sizeMB = info.size() / (1024.0 * 1024.0);
        
        qDebug() << "📂 User selected catalog:" << filePath;
        qDebug() << QString("📊 Size: %.1f MB").arg(sizeMB);
        
        // Set to Local2MASS and update UI
        m_statusLabel->setText(QString("Custom 2MASS catalog loaded (%.1f MB)").arg(sizeMB));
        
        // Clear previous catalog data
        m_catalogQueried = false;
        m_catalogPlotted = false;
        updateValidationControls();
        updatePlottingControls();
    }
}

// Add a menu or button for catalog file selection
void MainWindow::setupCatalogMenu()
{
    // Add a menu bar item for catalog management
    QMenuBar* menuBar = this->menuBar();
    QMenu* catalogMenu = menuBar->addMenu("&Catalog");
    
    QAction* browse2MASSAction = catalogMenu->addAction("Browse for 2MASS catalog...");
    connect(browse2MASSAction, &QAction::triggered, this, &MainWindow::browseCatalogFile);
    
    catalogMenu->addSeparator();
    
    QAction* showStatsAction = catalogMenu->addAction("Show catalog statistics");
    connect(showStatsAction, &QAction::triggered, [this]() {
        m_catalogValidator->showCatalogStats();
    });
    
    QAction* testQueryAction = catalogMenu->addAction("Test catalog query");
    connect(testQueryAction, &QAction::triggered, [this]() {
        if (m_hasWCS) {
            WCSData wcs = m_catalogValidator->getWCSData();
            qDebug() << "\n=== TESTING CATALOG QUERY ===";
            qDebug() << QString("Image center: RA=%.4f° Dec=%.4f°").arg(wcs.crval1).arg(wcs.crval2);
            
            auto start = QTime::currentTime();
            m_catalogValidator->queryCatalog(wcs.crval1, wcs.crval2, 1.0); // 1 degree radius test
            auto elapsed = start.msecsTo(QTime::currentTime());
            
            qDebug() << QString("Query completed in %1ms").arg(elapsed);
        } else {
            QMessageBox::information(this, "Test Query", "Load an image with WCS first");
        }
    });
}

// Add this to show performance comparison
void MainWindow::compareCatalogPerformance()
{
    if (!m_hasWCS) {
        QMessageBox::information(this, "Performance Test", "Load an image with WCS first");
        return;
    }
    
    WCSData wcs = m_catalogValidator->getWCSData();
    double testRadius = 1.0; // 1 degree radius
    
    qDebug() << "\n=== CATALOG PERFORMANCE COMPARISON ===";
    qDebug() << QString("Test query: RA=%.4f° Dec=%.4f° radius=%.1f°")
                .arg(wcs.crval1).arg(wcs.crval2).arg(testRadius);
    
    // Test Local 2MASS
    if (Local2MASSCatalog::isAvailable()) {
        auto start = QTime::currentTime();
        auto stars = Local2MASSCatalog::queryRegion(wcs.crval1, wcs.crval2, testRadius, 15.0);
        auto elapsed = start.msecsTo(QTime::currentTime());
        
        qDebug() << QString("🏠 Local 2MASS: %1 stars in %2ms").arg(stars.size()).arg(elapsed);
    }
    
    // For network catalogs, you'd time the network requests
    qDebug() << "🌐 Network catalogs: 2000-10000ms (depends on connection)";
    qDebug() << "📊 Local catalog is ~100x faster than network queries";
}
*/

