// SimplePlatesolver.h - Drop-in replacement for your existing plate solver

#ifndef SIMPLE_PLATESOLVER_H
#define SIMPLE_PLATESOLVER_H

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QTemporaryDir>
#include <QVector>
#include <QPoint>
#include <FITS/FITS.h>
#include "structuredefinitions.h"

// Simple wrapper that matches your existing interface
class SimplePlatesolver : public QObject
{
    Q_OBJECT

public:
    explicit SimplePlatesolver(QObject* parent = nullptr);
    
    // Configuration (matches your existing interface)
    void configurePlateSolver(const QString& astrometryPath,
                             const QString& indexPath,
                             double minScale,
                             double maxScale);
    void setStarCatalogValidator(StarCatalogValidator* validator) { }
    void setTimeout(int seconds) { m_timeoutSeconds = seconds; }
    
    // Main solving method (matches your existing interface)
    void extractStarsAndSolve(const ImageData* imageData,
                             const QVector<QPoint>& starCenters,
                             const QVector<float>& starFluxes,
                             const QVector<float>& starRadii = QVector<float>());
    
    bool isSolving() const;
    void cancelSolve();
    void setAutoSolveEnabled(bool enabled) {  }
    bool isAutoSolveEnabled() const { return true; }

signals:
    void platesolveStarted();
    void platesolveProgress(const QString& status);
    void platesolveComplete(pcl::AstrometricMetadata & result, const WCSData& wcs);
    void platesolveFailed(const QString& error);
    void wcsDataAvailable(const WCSData& wcs);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onTimeout();

private:
    QString createTempXYFile(const QVector<QPoint>& starCenters, const QVector<float>& starFluxes);
    QStringList buildArguments(const QString& xyFilePath, int imageWidth, int imageHeight);
    pcl::AstrometricMetadata parseWCSOutput(const QString& outputPath);
    void cleanup();
    
    // Configuration
    QString m_solveFieldPath;
    QString m_indexPath;
    double m_minScale;
    double m_maxScale;
    int m_timeoutSeconds;
    
    // Process management
    QProcess* m_process;
    QTimer* m_timeoutTimer;
    QTemporaryDir* m_tempDir;
    
    // Current solve data
    int m_currentImageWidth;
    int m_currentImageHeight;
};

#endif // SIMPLE_PLATESOLVER_H
