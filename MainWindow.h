#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QMenuBar>
#include <QStatusBar>
#include <QProgressBar>
#include <QSplitter>
#include <QGroupBox>
#include <QScrollArea>
#include <QFileDialog>
#include <QMessageBox>
#include <QTimer>
#include <QTime>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDialog>
#include <QPushButton>
#include <memory>

#include "ImageReader.h"
#include "ImageDisplayWidget.h"
#include "SimplifiedXISFWriter.h"
#include "BackgroundExtractionWidget.h"
#include "BackgroundExtractor.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onOpenFile();
    void onSaveAs();
    void onAbout();

    // Background extraction slots
    void onBackgroundExtracted(const BackgroundExtractionResult& result);
    void onBackgroundModelChanged(const QVector<float>& backgroundData, int width, int height, int channels);
    void onCorrectedImageReady(const QVector<float>& correctedData, int width, int height, int channels);
    
    // Background extraction menu actions
    void onExtractBackground();
    void onApplyBackgroundCorrection();

private:
    void setupUI();
    void setupMenuBar();
    void setupStatusBar();
    void setupViewTab();
    void setupBackgroundTab();
    
    void loadImageFile(const QString& filePath);
    void updateImageInfo();
    void logMessage(const QString& message);
    
    // Background extraction helper functions
    void createBackgroundImageWindow(const QVector<float>& backgroundData, int width, int height, int channels);
    void createCorrectedImageWindow(const QVector<float>& correctedData, int width, int height, int channels);
    
    // UI Components
    QWidget* m_centralWidget;
    QVBoxLayout* m_mainLayout;
    QTabWidget* m_tabWidget;
    
    // View tab
    QWidget* m_viewTab;
    QSplitter* m_viewSplitter;
    ImageDisplayWidget* m_imageDisplay;
    QGroupBox* m_infoGroup;
    QTextEdit* m_infoText;
    QTextEdit* m_logText;
    
    // Background tab
    QWidget* m_backgroundTab;
    BackgroundExtractionWidget* m_backgroundWidget;
    
    // Status bar
    QLabel* m_statusLabel;
    QProgressBar* m_statusProgress;
    
    // Data
    std::unique_ptr<ImageReader> m_imageReader;
    QString m_currentFilePath;
    
    // Menu actions (for enabling/disabling)
    QAction* m_saveAsAction;
};

#endif // MAINWINDOW_H
