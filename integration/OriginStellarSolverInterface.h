// OriginStellarSolverInterface.h - Interface between Origin TIFF metadata and StellarSolverManager
#ifndef ORIGIN_STELLARSOLVER_INTERFACE_H
#define ORIGIN_STELLARSOLVER_INTERFACE_H

#include <QObject>
#include <QString>
#include <QFileInfo>
#include <QDebug>
#include <QDateTime>
#include <tiffio.h>

// Your existing StellarSolver includes
#include <stellarsolver.h>
#include <parameters.h>
#include <structuredefinitions.h>

// Origin metadata extractor
#include "OriginMetadataExtractor.h"

// Enhanced job structure for Origin TIFF files
struct OriginStellarSolverJob {
    QString filename;
    int jobId;
    QDateTime startTime;
    QString status = "PENDING";
    
    // Standard StellarSolver results
    double ra = -1;
    double dec = -91;
    double pixelScale = 0;
    double orientation = 0;
    double solveTime = 0;
    double totalTime = 0;
    
    // Origin telescope hints
    bool hasOriginHints = false;
    QString objectName;
    double hintRA = -1.0;
    double hintDec = -91.0;
    double hintPixelScale = 0.0;
    double hintOrientation = 0.0;
    int stackedFrames = 0;
    QString hintQuality = "NONE";        // EXCELLENT, GOOD, POOR, NONE
    double searchRadius = 2.0;           // degrees
    double expectedSpeedup = 1.0;
    
    // Performance tracking
    double hintExtractionTime = 0.0;
    bool accelerated = false;
    double hintAccuracyRA = 0.0;         // arcsec difference
    double hintAccuracyDec = 0.0;        // arcsec difference
    double hintAccuracyScale = 0.0;      // percent difference
};

class OriginStellarSolverInterface : public QObject
{
    Q_OBJECT

public:
    explicit OriginStellarSolverInterface(QObject* parent = nullptr);
    
    // Convert TIFF file to format suitable for StellarSolver
    bool loadOriginTIFF(const QString& tiffFilePath, OriginStellarSolverJob& job);
    
    // Configure StellarSolver with Origin hints
    void configureSolverWithOriginHints(StellarSolver* solver, const OriginStellarSolverJob& job);
    
    // Analyze solver results against Origin hints
    void analyzeResults(OriginStellarSolverJob& job, const FITSImage::Solution& solution);
    // Convert TIFF image data for StellarSolver
    bool convertTIFFToStellarSolverFormat(const QString& tiffPath, 
                                         FITSImage::Statistic& stats, 
                                         std::vector<uint8_t>& buffer);
    
    // Utility methods
    static bool isOriginTIFF(const QString& filePath);
    static QString assessHintQuality(const OriginTelescopeMetadata& meta);
    static double getSearchRadiusForQuality(const QString& quality);
    static double getExpectedSpeedupForQuality(const QString& quality);

signals:
    void originHintsExtracted(const OriginStellarSolverJob& job);
    void solverConfigured(int jobId, const QString& configInfo);

private:
    // Extract Origin hints from TIFF file
    bool extractOriginHints(OriginStellarSolverJob& job);
    
    
    // TIFF reading helpers
    bool readTIFFImageData(const QString& tiffPath, 
                          uint32_t& width, uint32_t& height,
                          std::vector<uint16_t>& imageData);
    
    void logInfo(const QString& message);
    void logError(const QString& message);

private:
    bool m_verboseLogging = true;
    std::map<StellarSolver*, std::vector<uint8_t>> m_imageBuffers; // Keep buffers alive
};

#endif // ORIGIN_STELLARSOLVER_INTERFACE_H
