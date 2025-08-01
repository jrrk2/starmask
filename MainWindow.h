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
#include <memory>

#include "ImageReader.h"
#include "ImageDisplayWidget.h"
#include "StarMaskGenerator.h"
#include "StarCatalogValidator.h"

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
    void onCatalogSourceChanged();
    void onValidationModeChanged();
    void onMagnitudeLimitChanged();
    void onPixelToleranceChanged();
    
    // Star catalog validator slots
    void onCatalogQueryStarted();
    void onCatalogQueryFinished(bool success, const QString& message);
    void onValidationCompleted(const ValidationResult& result);
    void onValidatorError(const QString& message);
    
    // Star overlay toggle slot
    void onStarOverlayToggled(bool visible);
    void onCatalogOverlayToggled(bool visible);
    void onValidationOverlayToggled(bool visible);
  
private:
    void setupUI();
    void setupValidationControls();
    void updateValidationControls();
    void updateStatusDisplay();
    void extractWCSFromImage();
    void runStarDetection();
    void performValidation();
  
    // UI Components
    QWidget* m_centralWidget;
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_buttonLayout;
    QHBoxLayout* m_controlsLayout;
    
    // Image controls
    QPushButton* m_loadButton;
    QPushButton* m_detectButton;
    QPushButton* m_validateButton;
    
    // Validation controls
    QGroupBox* m_validationGroup;
    QVBoxLayout* m_validationLayout;
    QComboBox* m_catalogSourceCombo;
    QComboBox* m_validationModeCombo;
    QSpinBox* m_magnitudeLimitSpin;
    QSpinBox* m_pixelToleranceSpin;
    QProgressBar* m_queryProgressBar;
    
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
};

#endif // MAINWINDOW_H
