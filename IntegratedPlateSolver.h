// IntegratedPlateSolver.h - Using Internal Starlist Solver Engine
#ifndef INTEGRATED_PLATESOLVER_H
#define INTEGRATED_PLATESOLVER_H

#include <QObject>
#include <QTimer>
#include <QVector>
#include <QPoint>
#include <QString>
#include <QDebug>
#include <QThread>
#include <QMutex>
#include "PCLMockAPI.h"

// Forward declarations
class ImageData;
class StarCatalogValidator;

// C++ wrapper for the astrometry starlist solver
extern "C" {
#include "astrometry/engine.h"
#include "astrometry/onefield.h"
#include "astrometry/solver.h"
#include "astrometry/starxy.h"
#include "astrometry/matchobj.h"
#include "astrometry/starutil.h"
#include "astrometry/errors.h"
#include "astrometry/bl.h"
#include "astrometry/an-opts.h"
#include "astrometry/gslutils.h"
#include "astrometry/ioutils.h"
#include "astrometry/fileutils.h"
#include "astrometry/xylist.h"
#include "astrometry/fitstable.h"
#include "astrometry/fitsioutils.h"
#include "astrometry/sip_qfits.h"
}

// Structure for detected stars (matching your starlist solver)
struct DetectedStar {
    double x = 0.0;
    double y = 0.0;
    double flux = 1000.0;
    double radius = 5.0;
    double snr = 10.0;
    
    DetectedStar() = default;
    DetectedStar(double x_, double y_, double flux_ = 1000.0, double radius_ = 5.0) 
        : x(x_), y(y_), flux(flux_), radius(radius_) {}
};

// Solve options (matching your starlist solver configuration)
struct SolveOptions {
    QString indexPath = "/opt/homebrew/share/astrometry";
    QString configFile = "";  // Use default config resolution
    double minScale = 0.1;      // arcsec/pixel
    double maxScale = 60.0;     // arcsec/pixel  
    int imageWidth = 4000;
    int imageHeight = 3000;
    int maxStars = 200;
    double searchRadius = 15.0;  // degrees
    double logOddsThreshold = 14.0;
    bool verbose = false;
    
    // Optional field center guess for faster solving
    bool hasGuess = false;
    double raGuess = 0.0;       // degrees
    double decGuess = 0.0;      // degrees
    
    // Depths to try (corresponds to how many stars to use)
    QVector<int> depths = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
    
    // CPU time limit in seconds
    float cpuLimit = 60.0;
};

// Plate solve result structure  
struct PlatesolveResult {
    // Core data
    bool solved = false;
    QString errorMessage;
    QString indexUsed;
    
    // Position and scale (extracted from tan_t)
    double ra_center = 0.0;      // Center RA (degrees)
    double dec_center = 0.0;     // Center Dec (degrees)
    double pixscale = 1.0;       // Pixel scale (arcsec/pixel)
    double orientation = 0.0;    // Position angle (degrees)
    double fieldWidth = 0.0;     // Field width (arcmin)
    double fieldHeight = 0.0;    // Field height (arcmin)
    
    // CD matrix for precise transformations
    double cd11 = 0.0, cd12 = 0.0;
    double cd21 = 0.0, cd22 = 0.0;
    
    // Reference pixel (typically image center)
    double crpix1 = 0.0, crpix2 = 0.0;
    
    // Image parity (normal vs flipped)
    bool parity_positive = true;
    
    // Solution quality metrics
    double ra_error = 0.0;       // RA error (arcsec)
    double dec_error = 0.0;      // Dec error (arcsec)
    int matched_stars = 0;       // Number of matched catalog stars
    double solve_time = 0.0;     // Time to solve (seconds)
    
    // Internal WCS structure (from astrometry.net)
    tan_t wcs;
    
    // Convert to WCSData for your existing system
    WCSData toWCSData(int imageWidth = 0, int imageHeight = 0) const;
};

// Thread worker for running the solver engine
class SolverWorker : public QObject
{
    Q_OBJECT

public:
    SolverWorker(QObject* parent = nullptr);
    ~SolverWorker();

public slots:
    void solvePlate(const QVector<DetectedStar>& stars, const SolveOptions& options);

signals:
    void solveComplete(const PlatesolveResult& result);
    void solveFailed(const QString& error);
    void solveProgress(const QString& status);

private:
    engine_t* m_engine;
    bool m_engineInitialized;
    QMutex m_engineMutex;
    
    bool initializeEngine(const SolveOptions& options);
    void cleanupEngine();
    QString findDefaultConfigFile();
    job_t* createJobFromStars(const QVector<DetectedStar>& stars, const SolveOptions& options);
    bool createInMemoryXylist(job_t* job, const QVector<DetectedStar>& stars, const SolveOptions& options);
    void setupOnefieldSolver(job_t* job, const SolveOptions& options);
    PlatesolveResult extractResultFromJob(job_t* job);
};

class IntegratedPlateSolver : public QObject
{
    Q_OBJECT

public:
    explicit IntegratedPlateSolver(QObject* parent = nullptr);
    ~IntegratedPlateSolver();

    // Configuration - these map to your starlist solver options
    void setAstrometryPath(const QString& path) { Q_UNUSED(path); } // Not used with internal solver
    void setIndexPath(const QString& path);
    void setConfigFile(const QString& configFile);
    void setScaleRange(double minArcsecPerPixel, double maxArcsecPerPixel);
    void setSearchRadius(double degrees);
    void setPosition(double ra, double dec);  // Initial guess
    void setTimeout(int seconds);
    void setMaxStars(int count);
    void setLogOddsThreshold(double threshold);
    void setVerbose(bool verbose);
    
    // Main solving interface - integrates with your star extraction
    void solveFromStarMask(const QVector<QPoint>& starCenters,
                          const QVector<float>& starFluxes,
                          const ImageData* imageData);
    
    void solveFromDetectedStars(const QVector<DetectedStar>& stars,
                               int imageWidth, int imageHeight);

    // Advanced solving with validation
    void solveWithValidation(const QVector<QPoint>& starCenters,
                            const QVector<float>& starFluxes,
                            const ImageData* imageData,
                            StarCatalogValidator* validator);

    // Status and results
    bool isSolving() const { return m_solving; }
    PlatesolveResult getLastResult() const { return m_lastResult; }
    void cancelSolve();

signals:
    void solveStarted();
    void solveProgress(const QString& status);
    void solveComplete(const PlatesolveResult& result);
    void solveFailed(const QString& error);

private slots:
    void onWorkerSolveComplete(const PlatesolveResult& result);
    void onWorkerSolveFailed(const QString& error);
    void onWorkerSolveProgress(const QString& status);
    void onSolverTimeout();

private:
    // Configuration parameters (matching starlist solver)
    SolveOptions m_options;
    bool m_solving;
    PlatesolveResult m_lastResult;
    
    // Threading for non-blocking solve
    QThread* m_solverThread;
    SolverWorker* m_solverWorker;
    QTimer* m_timeoutTimer;
    
    void initializeSolver();
    void cleanupSolver();
    QVector<DetectedStar> convertStarsFromMask(const QVector<QPoint>& starCenters,
                                               const QVector<float>& starFluxes);
};

// Convenience class for easy integration with MainWindow
class ExtractStarsWithPlateSolve : public QObject
{
    Q_OBJECT

public:
    ExtractStarsWithPlateSolve(QObject* parent = nullptr);
    
    // Main integration method - call this from your 'extract stars' button
    void extractStarsAndSolve(const ImageData* imageData,
                             const QVector<QPoint>& starCenters,
                             const QVector<float>& starFluxes = QVector<float>(),
                             const QVector<float>& starRadii = QVector<float>());

    // Configuration (maps to starlist solver configuration)
    void configurePlateSolver(const QString& astrometryPath,
                             const QString& indexPath,
                             double minScale = 0.5,
                             double maxScale = 60.0);
    
    void setStarCatalogValidator(StarCatalogValidator* validator);
    
    // Enable automatic WCS generation after star extraction
    void setAutoSolveEnabled(bool enabled) { m_autoSolve = enabled; }
    bool isAutoSolveEnabled() const { return m_autoSolve; }

signals:
    void platesolveStarted();
    void platesolveProgress(const QString& status);  
    void platesolveComplete(const PlatesolveResult& result, const WCSData& wcs);
    void platesolveFailed(const QString& error);
    void wcsDataAvailable(const WCSData& wcs);

private slots:
    void onSolveComplete(const PlatesolveResult& result);
    void onSolveFailed(const QString& error);

private:
    IntegratedPlateSolver* m_solver;
    StarCatalogValidator* m_validator;
    bool m_autoSolve;
    const ImageData* m_currentImageData;
};

#endif // INTEGRATED_PLATESOLVER_H
