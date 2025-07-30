#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QTextEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QTimer>
#include <QTabWidget>
#include <QSplitter>
#include <memory>

// Forward declarations
class ImageReader;
class ImageDisplayWidget;
class ImageStatistics;
struct ImageData;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // File operations
    void onOpenFileClicked();
    void onSaveAsClicked();
    
    // Creation operations
    void onCreateXISFClicked();
    void onImageSizeChanged();
    void updatePreview();
    
    // Image display
    void onImageClicked(int x, int y, float value);
    void onZoomChanged(double factor);

private:
    void setupUI();
    void setupMenuBar();
    void setupCreateTab();
    void setupViewTab();
    
    // File operations
    void loadImageFile(const QString& filePath);
    void updateImageInfo();
    void updateImageStatistics();
    void setupInfoPanel();
    
    // Image creation
    void createTestImage(float* pixels, int width, int height);
    QString getCompressionName(int index);
    void logMessage(const QString& message);
    
    // UI components - Main layout
    QTabWidget* m_tabWidget;
    QWidget* m_createTab;
    QWidget* m_viewTab;
    
    // Create tab components
    QWidget* m_createCentralWidget;
    QGroupBox* m_imageGroup;
    QSpinBox* m_widthSpinBox;
    QSpinBox* m_heightSpinBox;
    QComboBox* m_compressionCombo;
    QLabel* m_previewLabel;
    QGroupBox* m_actionGroup;
    QPushButton* m_createButton;
    QProgressBar* m_progressBar;
    QGroupBox* m_logGroup;
    QTextEdit* m_logTextEdit;
    QTimer* m_previewTimer;
    
    // View tab components
    QSplitter* m_viewSplitter;
    ImageDisplayWidget* m_imageDisplay;
    QWidget* m_infoPanel;
    QGroupBox* m_fileInfoGroup;
    QTextEdit* m_fileInfoText;
    QGroupBox* m_imageInfoGroup;
    QTextEdit* m_imageInfoText;
    QGroupBox* m_statisticsGroup;
    QTextEdit* m_statisticsText;
    QGroupBox* m_pixelInfoGroup;
    QTextEdit* m_pixelInfoText;
    QGroupBox* m_metadataGroup;
    QTextEdit* m_metadataText;
    
    // File operations
    QPushButton* m_openButton;
    QPushButton* m_saveAsButton;
    QLabel* m_statusLabel;
    
    // Data
    std::unique_ptr<ImageReader> m_imageReader;
    std::unique_ptr<ImageStatistics> m_imageStats;
    QString m_currentFilePath;
};

#endif // MAINWINDOW_H
