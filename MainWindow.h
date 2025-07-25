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

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onCreateXISFClicked();
    void onImageSizeChanged();
    void updatePreview();

private:
    void setupUI();
    void createTestImage(float* pixels, int width, int height);
    QString getCompressionName(int index);
    void logMessage(const QString& message);

    // UI components
    QWidget* m_centralWidget;
    
    // Image settings group
    QGroupBox* m_imageGroup;
    QSpinBox* m_widthSpinBox;
    QSpinBox* m_heightSpinBox;
    QComboBox* m_compressionCombo;
    QLabel* m_previewLabel;
    
    // Action group
    QGroupBox* m_actionGroup;
    QPushButton* m_createButton;
    QProgressBar* m_progressBar;
    
    // Log group
    QGroupBox* m_logGroup;
    QTextEdit* m_logTextEdit;
    
    // Preview update timer
    QTimer* m_previewTimer;
};

#endif // MAINWINDOW_H