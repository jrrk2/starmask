#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <memory>

#include "ImageReader.h"
#include "ImageDisplayWidget.h"
#include "StarMaskGenerator.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onLoadImage();
    void onDetectStars();
    void runStarDetection();

private:
    std::unique_ptr<ImageReader> m_imageReader;
    ImageDisplayWidget* m_imageDisplayWidget;
    QPushButton* m_loadButton;
    QPushButton* m_detectButton;
    QLabel* m_statusLabel;
    const ImageData *m_imageData;

    StarMaskResult m_lastStarMask;
};

#endif // MAINWINDOW_H
