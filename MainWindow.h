#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QSpinBox>
#include <QGroupBox>
#include <QProgressBar>
#include <QTextEdit>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <memory>

#include "ImageReader.h"
#include "ImageDisplayWidget.h"
#include "StarMaskGenerator.h"
#include "StarCatalogValidator.h"
#include "PixelMatchingDebugger.h" // Include the debugger header

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onLoadImage();
    void onDetectStars();
    void onValidateStars();
    void onPlotCatalogStars();  // NEW: Direct catalog plotting
    void onValidationModeChanged();
  //    void onMagnitudeLimitChanged();
    void onPixelToleranceChanged();
    void onPlotModeToggled(bool plotMode);  // NEW: Toggle between modes
    
    // Star catalog validator slots
    void onCatalogQueryStarted();
    void onCatalogQueryFinished(bool success, const QString& message);
    void onValidationCompleted(const ValidationResult& result);
    void onValidatorError(const QString& message);
    
    // Star overlay toggle slot
    void onStarOverlayToggled(bool visible);
    void onCatalogOverlayToggled(bool visible);
    void onValidationOverlayToggled(bool visible);
  //    void onValidateStarsEnhanced();
    void onMatchingParametersChanged();
    void onShowMatchingDetails();
    void onExportMatchingResults();
    void onVisualizeDistortions();
    void onCalibrateDistortionModel();
  //    void onDebugPixelMatching();
  //    void onAnalyzeMatchingCriteria();
  //    void onTestParameterSensitivity();
    void onDebugStarCorrelation();
private:
    void setupUI();
    void setupCatalogPlottingControls();  // NEW: Setup plotting controls
    void updateValidationControls();
    void updatePlottingControls();        // NEW: Update plotting controls
    void updateStatusDisplay();
    void extractWCSFromImage();
    void runStarDetection();
    void performValidation();
    void plotCatalogStarsDirectly();      // NEW: Direct catalog plotting
    void debugCatalogQuery();
    void addDebugButton();
    void setupGaiaDR3Catalog();
    void setupGaiaCatalog();
    void browseCatalogFile();
    void setupCatalogMenu();
    void compareCatalogPerformance();
    void browseGaiaCatalogFile();
    void testGaiaPerformance();
    void onDetectStarsAdvanced();
    void onDetectStarsSimple();
    void setupStarDetectionControls();
    void setupEnhancedMatchingControls();
    void updateEnhancedMatchingControls();
    StarMatchingParameters getMatchingParametersFromUI();
    void displayEnhancedResults(const EnhancedValidationResult& result);
    void visualizeTrianglePatterns(const EnhancedValidationResult& result);
    void setupDebuggingMenu();
  //    void showPixelDebugDialog();
    void showWCSDebugInfo();
    void testWCSTransformations();
    void rerunValidationWithDebug();
    void testPixelMatchingDebug();
    void addTestButton();
    void quickDiagnoseCurrentMatches();
    void debugCurrentValidationResults();

    std::unique_ptr<PixelMatchingDebugger> m_pixelDebugger;
  
    // UI Components
    QWidget* m_centralWidget;
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_buttonLayout;
    QHBoxLayout* m_controlsLayout;
    
    // Image controls
    QPushButton* m_loadButton;
    QPushButton* m_plotCatalogButton;     // NEW: Plot catalog button
    
    // Mode controls
    QCheckBox* m_plotModeCheck;           // NEW: Toggle plot mode
    
    // Validation controls
    QVBoxLayout* m_validationLayout;
    
    // Catalog plotting controls
    QGroupBox* m_plottingGroup;           // NEW: Plotting controls group
    QVBoxLayout* m_plottingLayout;        // NEW: Plotting layout
  //    QDoubleSpinBox* m_plotMagnitudeSpin;  // NEW: Magnitude limit for plotting
    QDoubleSpinBox* m_fieldRadiusSpin;    // NEW: Field radius control
    
    QComboBox* m_validationModeCombo;
  //    QSpinBox* m_magnitudeLimitSpin;
  //    QSpinBox* m_pixelToleranceSpin;
  //    QProgressBar* m_queryProgressBar;
    
    // Status display
    QLabel* m_statusLabel;
    QTextEdit* m_resultsText;
    
    // Core components
    std::unique_ptr<ImageReader> m_imageReader;
    std::unique_ptr<StarCatalogValidator> m_catalogValidator;
    ImageDisplayWidget* m_imageDisplayWidget;
    
    // Data
    const ImageData* m_imageData;
    StarMaskResult m_lastStarMask;
    ValidationResult m_lastValidation;
    
    // State
    bool m_hasWCS;
    bool m_starsDetected;
    bool m_catalogQueried;
    bool m_validationComplete;
    bool m_plotMode;                      // NEW: Plot mode flag
    bool m_catalogPlotted;                // NEW: Catalog plotted flag

    // Star detection controls
    QGroupBox* m_starDetectionGroup;
    QVBoxLayout* m_starDetectionLayout;
    QSlider* m_sensitivitySlider;
    QSpinBox* m_structureLayersSpinBox;
    QSpinBox* m_noiseLayersSpinBox;
    QSlider* m_peakResponseSlider;
    QSlider* m_maxDistortionSlider;
    QCheckBox* m_enablePSFFittingCheck;
    QPushButton* m_detectAdvancedButton;
    QPushButton* m_detectSimpleButton;

    // Enhanced matching controls
    QGroupBox* m_enhancedMatchingGroup;
    QVBoxLayout* m_enhancedMatchingLayout;
    
    // Distance and search parameters
    QDoubleSpinBox* m_maxPixelDistanceSpin;
    QDoubleSpinBox* m_searchRadiusSpin;
    QDoubleSpinBox* m_maxMagnitudeDiffSpin;
    
    // Pattern matching controls
    QCheckBox* m_useTriangleMatchingCheck;
    QSpinBox* m_minTriangleStarsSpin;
    QDoubleSpinBox* m_triangleToleranceSpin;
    
    // Quality controls
    QDoubleSpinBox* m_minMatchConfidenceSpin;
    QSpinBox* m_minMatchesValidationSpin;
    
    // Advanced options
    QCheckBox* m_useDistortionModelCheck;
    QCheckBox* m_useProperMotionCheck;
    QCheckBox* m_useBayesianMatchingCheck;
    
    // Results display
    QTextEdit* m_enhancedResultsText;
    QProgressBar* m_matchingProgressBar;
    
    // Enhanced validation results
    EnhancedValidationResult m_lastEnhancedValidation;
    bool m_enhancedValidationComplete;
  
};

#endif // MAINWINDOW_H
