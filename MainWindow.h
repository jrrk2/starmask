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
#include <QProgressDialog>
#include <QTextEdit>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QStatusBar>
#include <memory>

#include <QVector>
#include <QPoint>
#include <QImage>
#include <QString>
#include <QColor>
#include "ImageReader.h"
#include "ImageDisplayWidget.h"
#include "StarCatalogValidator.h"
// #include "IntegratedPlateSolver.h"
#include "SimplePlatesolver.h"
#include "ImageReader.h"
#include "StarMaskGenerator.h"
#include "PixelMatchingDebugger.h" // Include the debugger header

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onShowCatalogStats();     // Update existing or add new
  //    void onShowCatalogChart();     // Optional: separate chart action

    void onTestPlatesolveWithStarExtraction();
  //    void onTestPlatesolveComplete(const pcl::AstrometricMetadata& result, const WCSData& wcs);
    void onTestPlateSolveFailed(const QString& error);  
    void onDetectStarsIntegratedVersion();
  
    void onExtractStarsWithPlatesolve();
    void onPlatesolveStarted();
    void onPlatesolveProgress(const QString& status);
    void onPlatesolveComplete(const pcl::AstrometricMetadata& result, const WCSData& wcs);
    void onPlatesolveFailed(const QString& error);
    void configurePlatesolverSettings();
    void onWCSDataReceived(const WCSData& wcs);
    
    void initializePlatesolveIntegration();
    void setupPlatesolveMenus();
    void triggerPlatesolveWithCurrentStars();
    void addPlatesolveIntegration();
    //    void onExtractStarsWithPlatesolve();
    //    void onPlatesolveStarted();
    //    void onPlatesolveProgress(const QString& status);
    //    void onPlatesolveComplete(const pcl::AstrometricMetadata& result, const WCSData& wcs);
    //    void onPlatesolveFailed(const QString& error);
    //    void configurePlatesolverSettings();
    void onLoadImage();
  //    void onDetectStars();
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
    void onWCSDataReceived();
    void enhanceCatalogDisplay();
    void onPlotCatalogStarsEnhanced();
    void debugCatalogDisplayComparison();
    void syncCatalogDisplays();

private:
    QPointF calculatePixelPosition(double ra, double dec);
  //    StarMaskResult performStarExtraction();
    void updateStarDisplay(const StarMaskResult& starMask);
    void updateCoordinateDisplay(const WCSData& wcs);
    void showCatalogValidationResults(const ValidationResult& validation);
  //    void showCatalogValidationResults(const ExtractStarsWithPlateSolve* m_platesolveIntegration = nullptr);
    void showCatalogValidationResults(const SimplePlatesolver* m_platesolveIntegration = nullptr);
    const ImageData* m_imageData;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_coordinateLabel = nullptr;
    QTextEdit* m_resultsText = nullptr;
    ImageDisplayWidget* m_imageDisplayWidget = nullptr;
    //    void initializePlatesolveIntegration();
    //    void setupPlatesolveMenus();
    void extractStarsIntegrated();
    void addPlatesolvingTestButton();
    void addPlatesolvingTestMenu();    
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
  //    void onDetectStarsSimple();
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
    void showPlatesolveResults(const pcl::AstrometricMetadata& result);
    void updateImageDisplayWithWCS(const WCSData& wcs);
    void triggerCatalogValidation(const pcl::AstrometricMetadata& result);
    double calculateFieldRadius(const pcl::AstrometricMetadata& result);
    QString formatRACoordinates(double ra);
    QString formatDecCoordinates(double dec);
    
  //    ExtractStarsWithPlateSolve* m_platesolveIntegration;
    SimplePlatesolver* m_platesolveIntegration;
    QProgressDialog* m_platesolveProgressDialog;
    
    QString m_astrometryPath;
    QString m_indexPath;
    double m_minScale;
    double m_maxScale;
    int m_platesolveTimeout;
    int m_maxStarsForSolving;
    
    //    ExtractStarsWithPlateSolve* m_platesolveIntegration;
    //    QProgressDialog* m_platesolveProgressDialog;
    QAction* m_platesolveAction;
    QAction* m_toggleAutoPlatesolveAction;

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
  //    QDoubleSpinBox* m_fieldRadiusSpin;    // NEW: Field radius control
    
    QComboBox* m_validationModeCombo;
  //    QSpinBox* m_magnitudeLimitSpin;
  //    QSpinBox* m_pixelToleranceSpin;
  //    QProgressBar* m_queryProgressBar;
    
    // Status display
    //    QLabel* m_statusLabel;
    //    QTextEdit* m_resultsText;
    
    // Core components
    std::unique_ptr<ImageReader> m_imageReader;
    std::unique_ptr<StarCatalogValidator> m_catalogValidator;
    //    ImageDisplayWidget* m_imageDisplayWidget;
    
    // Data
    //    const ImageData* m_imageData;
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
  //    QPushButton* m_detectSimpleButton;

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
