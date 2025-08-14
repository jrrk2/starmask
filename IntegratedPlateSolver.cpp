// IntegratedPlateSolver.cpp - Using Internal Starlist Solver Engine
#include "StarCatalogValidator.h"
#include "IntegratedPlateSolver.h"
#include "ImageReader.h"  // Assuming you have this for ImageData
#include <QDir>
#include <QDebug>
#include <QThread>
#include <QMutexLocker>
#include <QCoreApplication>
#include <cmath>
#include <algorithm>
#include "PCLMockAPI.h"
#include "AstrometryDirectSolver.h"

// WCSData conversion implementation
WCSData PlatesolveResult::toWCSData(int imageWidth, int imageHeight) const
{
    WCSData wcs;
    
    if (!solved) {
        wcs.isValid = false;
        return wcs;
    }
    
    wcs.isValid = true;
    wcs.crval1 = ra_center;
    wcs.crval2 = dec_center;
    wcs.crpix1 = (crpix1 > 0) ? crpix1 : (imageWidth / 2.0 + 1.0);   // FITS 1-based
    wcs.crpix2 = (crpix2 > 0) ? crpix2 : (imageHeight / 2.0 + 1.0);
    wcs.cd11 = cd11;
    wcs.cd12 = cd12;
    wcs.cd21 = cd21;
    wcs.cd22 = cd22;
    wcs.pixscale = pixscale;
    wcs.orientation = orientation;
    wcs.width = imageWidth;
    wcs.height = imageHeight;
    
    return wcs;
}

// SolverWorker Implementation (runs in separate thread)

SolverWorker::SolverWorker(QObject* parent)
    : QObject(parent)
    , m_engine(nullptr)
    , m_engineInitialized(false)
{
    // Initialize GSL and logging like the starlist solver does
    gslutils_use_error_system();
    loginit();
    errors_log_to(stderr);
}

SolverWorker::~SolverWorker()
{
    cleanupEngine();
}

void SolverWorker::solvePlate(const QVector<DetectedStar>& stars, const SolveOptions& options)
{
    emit solveProgress("Initializing astrometry engine...");
    
    // Initialize engine if needed
    if (!m_engineInitialized) {
        if (!initializeEngine(options)) {
            emit solveFailed("Failed to initialize astrometry engine");
            return;
        }
    }
    
    if (stars.isEmpty()) {
        emit solveFailed("No stars provided for solving");
        return;
    }
    
    emit solveProgress(QString("Creating job with %1 stars...").arg(stars.size()));
    
    // Create job directly from stars (like your starlist solver)
    job_t* job = createJobFromStars(stars, options);
    if (!job) {
        emit solveFailed("Failed to create job from star data");
        return;
    }
    
    emit solveProgress("Running astrometry engine...");
    
    QMutexLocker locker(&m_engineMutex);
    
    // Run the job using the engine (this is the core solving from your starlist solver)
    int solve_result = engine_run_job(m_engine, job);
    
    PlatesolveResult result;
    
    if (solve_result == 0) {
        // Success - extract WCS solution
        result = extractResultFromJob(job);
        if (result.solved) {
            emit solveComplete(result);
        } else {
            emit solveFailed("Failed to extract WCS solution from job");
        }
    } else {
        result.solved = false;
        result.errorMessage = "Engine failed to find solution";
        emit solveFailed(result.errorMessage);
    }
    
    // Clean up job
    job_free(job);
}

bool SolverWorker::initializeEngine(const SolveOptions& options)
{
    if (m_engineInitialized) {
        return true;
    }
    
    // Create engine exactly like your starlist solver does
    m_engine = engine_new();
    if (!m_engine) {
        qDebug() << "Failed to create astrometry engine";
        return false;
    }
    
    // Set CPU limit
    m_engine->cpulimit = options.cpuLimit;
    
    if (options.verbose) {
        qDebug() << "Loading configuration and indexes...";
    }
    
    // Load configuration file (replicate starlist solver logic)
    QString configFile = options.configFile;
    if (configFile.isEmpty()) {
        configFile = findDefaultConfigFile();
    }
    
    if (configFile != "none" && !configFile.isEmpty()) {
        if (engine_parse_config_file(m_engine, configFile.toLocal8Bit().data())) {
            qDebug() << "Failed to parse config file:" << configFile;
            engine_free(m_engine);
            m_engine = nullptr;
            return false;
        }
    }
    
    // Add index files from the specified directory
    if (!options.indexPath.isEmpty()) {
        engine_add_search_path(m_engine, options.indexPath.toLocal8Bit().data());
    }
    
    // Load indexes
    engine_autoindex_search_paths(m_engine);
    
    if (options.verbose) {
        qDebug() << "Loaded" << pl_size(m_engine->indexes) << "index files";
    }
    
    // Set default field width constraints (from starlist solver)
    if (m_engine->minwidth <= 0.0) m_engine->minwidth = 0.1;
    if (m_engine->maxwidth <= 0.0) m_engine->maxwidth = 180.0;
    
    // Set default depths if not specified in config
    if (!il_size(m_engine->default_depths)) {
        for (int depth : options.depths) {
            il_append(m_engine->default_depths, depth);
        }
    }
    
    m_engineInitialized = true;
    return true;
}

void SolverWorker::cleanupEngine()
{
    QMutexLocker locker(&m_engineMutex);
    if (m_engine) {
        engine_free(m_engine);
        m_engine = nullptr;
    }
    m_engineInitialized = false;
}

QString SolverWorker::findDefaultConfigFile()
{
    // Replicate the config file search logic from your starlist solver
    QStringList tryPaths = {
        "/opt/homebrew/etc/astrometry.cfg",  // Common on macOS with Homebrew
        "/usr/local/astrometry/etc/astrometry.cfg",
        "/etc/astrometry.cfg",
        "../etc/astrometry.cfg",
        "./astrometry.cfg"
    };
    
    for (const QString& path : tryPaths) {
        if (QFile::exists(path)) {
            return path;
        }
    }
    
    qDebug() << "Warning: No config file found, using built-in defaults";
    return "none";
}

job_t* SolverWorker::createJobFromStars(const QVector<DetectedStar>& stars, const SolveOptions& options)
{
    // Create a job_t structure directly from the star data (like your starlist solver)
    
    // Allocate job structure
    job_t* job = (job_t*)calloc(1, sizeof(job_t));
    if (!job) {
        return nullptr;
    }
    
    // Initialize data structures
    job->scales = dl_new(8);
    job->depths = il_new(8);
    job->include_default_scales = TRUE;
    job->use_radec_center = FALSE;
    
    // Set scale constraints
    dl_append(job->scales, options.minScale);
    dl_append(job->scales, options.maxScale);
    
    // Set depths
    for (int depth : options.depths) {
        il_append(job->depths, depth);
    }
    
    // Set field center guess if provided
    if (options.hasGuess) {
        job->use_radec_center = TRUE;
        job->ra_center = options.raGuess;
        job->dec_center = options.decGuess;
        job->search_radius = options.searchRadius;
    }
    
    // Initialize the onefield_t structure (bp = blind plate)
    onefield_init(&job->bp);
    
    // Create in-memory xylist from our stars
    if (!createInMemoryXylist(job, stars, options)) {
        job_free(job);
        return nullptr;
    }
    
    // Set up the solver within the onefield structure
    setupOnefieldSolver(job, options);
    
    // Add indexes from the engine to the onefield
    for (int i = 0; i < (int)pl_size(m_engine->indexes); i++) {
        index_t* index = (index_t*)pl_get(m_engine->indexes, i);
        onefield_add_loaded_index(&job->bp, index);
    }
    
    // Set solver parameters
    job->bp.solver.funits_lower = options.minScale;
    job->bp.solver.funits_upper = options.maxScale;
    job->bp.cpulimit = options.cpuLimit;
    
    // Create template for WCS output
    char wcs_template[] = "wcs_template_XXXXXX";
    int template_fd = mkstemp(wcs_template);
    job->bp.wcs_template = strdup(wcs_template);
    close(template_fd);
    
    // Set log odds threshold
    solver_set_keep_logodds(&job->bp.solver, options.logOddsThreshold);
    
    return job;
}

bool SolverWorker::createInMemoryXylist(job_t* job, const QVector<DetectedStar>& stars, const SolveOptions& options)
{
    // Create a temporary file to hold the xylist data (like your starlist solver)
    char tempName[] = "/tmp/astro_stars_XXXXXX.xyls";
    int fd = mkstemps(tempName, 5);  // 5 = length of ".xyls"
    if (fd == -1) {
        qDebug() << "Failed to create temporary xylist file";
        return false;
    }
    close(fd);
    
    QString tempFilename(tempName);
    
    // Create xylist for writing
    xylist_t* ls = xylist_open_for_writing(tempFilename.toLocal8Bit().data());
    if (!ls) {
        qDebug() << "Failed to create xylist for writing";
        unlink(tempFilename.toLocal8Bit().data());
        return false;
    }
    
    // Set field dimensions
    xylist_set_include_flux(ls, TRUE);
    xylist_set_include_background(ls, FALSE);
    if (xylist_write_primary_header(ls) ||
        xylist_write_field(ls, nullptr)) {
        qDebug() << "Failed to write xylist headers";
        xylist_close(ls);
        unlink(tempFilename.toLocal8Bit().data());
        return false;
    }
    
    // Add stars to xylist
    starxy_t* sxy = starxy_new(stars.size(), TRUE, FALSE);
    for (int i = 0; i < stars.size(); ++i) {
        const DetectedStar& star = stars[i];
        starxy_set_x(sxy, i, star.x);
        starxy_set_y(sxy, i, star.y);
        starxy_set_flux(sxy, i, star.flux);
    }
    
    if (xylist_write_field(ls, sxy)) {
        qDebug() << "Failed to write stars to xylist";
        starxy_free(sxy);
        xylist_close(ls);
        unlink(tempFilename.toLocal8Bit().data());
        return false;
    }
    
    starxy_free(sxy);
    
    if (xylist_fix_primary_header(ls) || xylist_close(ls)) {
        qDebug() << "Failed to finalize xylist";
        unlink(tempFilename.toLocal8Bit().data());
        return false;
    }
    
    // Set the xylist file in the job
    job->bp.fieldfname = strdup(tempFilename.toLocal8Bit().data());
    
    return true;
}

void SolverWorker::setupOnefieldSolver(job_t* job, const SolveOptions& options)
{
    // Setup solver parameters (from your starlist solver)
  /*
    job->bp.imagew = options.imageWidth;
    job->bp.imageh = options.imageHeight;
  */
    // Initialize solver
    solver_t* solver = &job->bp.solver;
    solver_reset_counters(solver);
    solver_set_keep_logodds(solver, options.logOddsThreshold);
}

PlatesolveResult SolverWorker::extractResultFromJob(job_t* job)
{
    PlatesolveResult result;
    
    // Check if solution was found
    if (!job->bp.wcs_template || !file_exists(job->bp.wcs_template)) {
        result.solved = false;
        result.errorMessage = "No WCS solution file found";
        return result;
    }
    
    // Read WCS solution from file
    tan_t* rslt = tan_read_header_file(job->bp.wcs_template, &result.wcs);
    if (!rslt) {
        result.solved = false;
        result.errorMessage = "Failed to read WCS solution";
        return result;
    }
    
    result.solved = true;
    
    // Extract coordinates and parameters from WCS
    result.ra_center = result.wcs.crval[0];
    result.dec_center = result.wcs.crval[1];
    result.crpix1 = result.wcs.crpix[0];
    result.crpix2 = result.wcs.crpix[1];
    
    // Extract CD matrix
    result.cd11 = result.wcs.cd[0][0];
    result.cd12 = result.wcs.cd[0][1];
    result.cd21 = result.wcs.cd[1][0];
    result.cd22 = result.wcs.cd[1][1];
    
    // Calculate pixel scale from CD matrix
    double cd00 = result.cd11;
    double cd01 = result.cd12;
    result.pixscale = sqrt(cd00 * cd00 + cd01 * cd01) * 3600.0;  // Convert to arcsec/pixel
    
    // Calculate rotation from CD matrix
    result.orientation = atan2(cd01, cd00) * 180.0 / M_PI;
    if (result.orientation < 0) result.orientation += 360.0;
    
    // Calculate field size
    result.fieldWidth = result.wcs.imagew * result.pixscale / 60.0;   // Convert to arcmin
    result.fieldHeight = result.wcs.imageh * result.pixscale / 60.0;  // Convert to arcmin
    
    // Get quality metrics if available
    //    result.matched_stars = job->best_match_solved;
    
    // Clean up temporary WCS file
    if (job->bp.wcs_template) {
        unlink(job->bp.wcs_template);
    }
    
    return result;
}

// IntegratedPlateSolver Implementation

IntegratedPlateSolver::IntegratedPlateSolver(QObject* parent)
    : QObject(parent)
    , m_solving(false)
    , m_solverThread(nullptr)
    , m_solverWorker(nullptr)
{
    // Initialize default options (matching your starlist solver defaults)
    m_options.indexPath = "/opt/homebrew/share/astrometry";
    m_options.minScale = 0.1;
    m_options.maxScale = 60.0;
    m_options.imageWidth = 4000;
    m_options.imageHeight = 3000;
    m_options.maxStars = 200;
    m_options.searchRadius = 15.0;
    m_options.logOddsThreshold = 14.0;
    m_options.verbose = false;
    m_options.hasGuess = false;
    m_options.depths = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
    m_options.cpuLimit = 60.0;
    
    // Setup timeout timer
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, &IntegratedPlateSolver::onSolverTimeout);
    
    initializeSolver();
    
    qDebug() << "IntegratedPlateSolver initialized with internal starlist engine";
}

IntegratedPlateSolver::~IntegratedPlateSolver()
{
    cleanupSolver();
}

void IntegratedPlateSolver::initializeSolver()
{
    // Create solver thread and worker
    m_solverThread = new QThread(this);
    m_solverWorker = new SolverWorker();
    m_solverWorker->moveToThread(m_solverThread);
    
    // Connect worker signals
    connect(m_solverWorker, &SolverWorker::solveComplete,
            this, &IntegratedPlateSolver::onWorkerSolveComplete);
    connect(m_solverWorker, &SolverWorker::solveFailed,
            this, &IntegratedPlateSolver::onWorkerSolveFailed);
    connect(m_solverWorker, &SolverWorker::solveProgress,
            this, &IntegratedPlateSolver::onWorkerSolveProgress);
    
    // Start thread
    m_solverThread->start();
}

void IntegratedPlateSolver::cleanupSolver()
{
    if (m_solving) {
        cancelSolve();
    }
    
    if (m_solverThread) {
        m_solverThread->quit();
        m_solverThread->wait(3000);
        delete m_solverWorker;
        m_solverWorker = nullptr;
    }
}

// Configuration methods

void IntegratedPlateSolver::setIndexPath(const QString& path)
{
    m_options.indexPath = path;
    qDebug() << "Index path set to:" << path;
}

void IntegratedPlateSolver::setConfigFile(const QString& configFile)
{
    m_options.configFile = configFile;
    qDebug() << "Config file set to:" << configFile;
}

void IntegratedPlateSolver::setScaleRange(double minArcsecPerPixel, double maxArcsecPerPixel)
{
    m_options.minScale = minArcsecPerPixel;
    m_options.maxScale = maxArcsecPerPixel;
    qDebug() << "Scale range set to:" << m_options.minScale << "-" << m_options.maxScale << "arcsec/pixel";
}

void IntegratedPlateSolver::setSearchRadius(double degrees)
{
    m_options.searchRadius = degrees;
}

void IntegratedPlateSolver::setPosition(double ra, double dec)
{
    m_options.raGuess = ra;
    m_options.decGuess = dec;
    m_options.hasGuess = true;
    qDebug() << "Position guess set to RA:" << ra << "Dec:" << dec;
}

void IntegratedPlateSolver::setTimeout(int seconds)
{
    m_options.cpuLimit = static_cast<float>(seconds);
}

void IntegratedPlateSolver::setMaxStars(int count)
{
    m_options.maxStars = count;
}

void IntegratedPlateSolver::setLogOddsThreshold(double threshold)
{
    m_options.logOddsThreshold = threshold;
}

void IntegratedPlateSolver::setVerbose(bool verbose)
{
    m_options.verbose = verbose;
}

// Main solving methods

void IntegratedPlateSolver::solveFromStarMask(const QVector<QPoint>& starCenters,
                                             const QVector<float>& starFluxes,
                                             const ImageData* imageData)
{
    std::vector<StarPosition> stars;
    stars.reserve(starCenters.size());

    if (m_solving) {
        emit solveFailed("Solver is already running");
        return;
    }

    if (starCenters.isEmpty() || !imageData) {
        emit solveFailed("No stars or image data provided");
        return;
    }

    /*    */
    qDebug() << "Starting plate solve with" << starCenters.size() << "stars";
    xylist_t* xyls = xylist_open("../examples/FinalStackedMaster.axy");
    starxy_t* field = xylist_read_field_num(xyls, 1, nullptr);
    std::cout << "Read field with " << field->N << " stars" << std::endl;
    
    // Convert to our StarPosition format
    stars.reserve(field->N);
    for (int i = 0; i < field->N; i++) {
        double flux = (field->flux && field->flux[i] > 0) ? field->flux[i] : 1000.0;
	qDebug() << field->x[i] << field->y[i] << flux;
	//        stars.emplace_back(field->x[i], field->y[i], flux);
    }
    
    std::cout << "Loaded " << stars.size() << " stars from xylist file" << std::endl;
    /*    */
    
    for (int i = 0; i < starCenters.size(); ++i) {
        const QPoint& center = starCenters[i];
        float flux = (i < starFluxes.size()) ? starFluxes[i] : 1000.0f;
	qDebug() << center.x() << center.y() << flux;
        stars.emplace_back(center.x(), center.y(), flux);
    }
    
    astrometry_direct(stars);
    
    // Convert to DetectedStar format
    //    QVector<DetectedStar> stars = convertStarsFromMask(starCenters, starFluxes);
    
    // Update image dimensions
    m_options.imageWidth = imageData->width;
    m_options.imageHeight = imageData->height;
    
    // solveFromDetectedStars(stars, imageData->width, imageData->height);
}

void IntegratedPlateSolver::solveFromDetectedStars(const QVector<DetectedStar>& stars,
                                                  int imageWidth, int imageHeight)
{
    if (m_solving) {
        emit solveFailed("Solver is already running");
        return;
    }
    
    if (stars.isEmpty()) {
        emit solveFailed("No stars provided");
        return;
    }
    
    m_solving = true;
    emit solveStarted();
    
    // Update image dimensions
    m_options.imageWidth = imageWidth;
    m_options.imageHeight = imageHeight;
    
    // Limit number of stars if necessary
    QVector<DetectedStar> limitedStars = stars;
    if (limitedStars.size() > m_options.maxStars) {
        // Sort by flux (brightest first) and take top N
        std::sort(limitedStars.begin(), limitedStars.end(), 
                  [](const DetectedStar& a, const DetectedStar& b) {
                      return a.flux > b.flux;
                  });
        limitedStars.resize(m_options.maxStars);
        qDebug() << "Limited to" << m_options.maxStars << "brightest stars";
    }
    
    qDebug() << "Solving with" << limitedStars.size() << "stars, image size:" 
             << imageWidth << "x" << imageHeight;
    
    // Start timeout timer
    m_timeoutTimer->start(static_cast<int>(m_options.cpuLimit * 1000));
    
    // Trigger solve in worker thread
    QMetaObject::invokeMethod(m_solverWorker, "solvePlate", Qt::QueuedConnection,
                              Q_ARG(QVector<DetectedStar>, limitedStars),
                              Q_ARG(SolveOptions, m_options));
}

void IntegratedPlateSolver::solveWithValidation(const QVector<QPoint>& starCenters,
                                               const QVector<float>& starFluxes,
                                               const ImageData* imageData,
                                               StarCatalogValidator* validator)
{
    Q_UNUSED(validator)  // For now, just solve normally
    
    // TODO: Implement validation integration
    // This could pre-validate the star list against catalogs before solving
    
    solveFromStarMask(starCenters, starFluxes, imageData);
}

void IntegratedPlateSolver::cancelSolve()
{
    if (m_solving) {
        qDebug() << "Canceling plate solve";
        m_timeoutTimer->stop();
        m_solving = false;
        
        // Note: The actual astrometry engine doesn't have a clean way to cancel
        // mid-solve, so we just mark as not solving and let it complete
        
        emit solveFailed("Solve cancelled by user");
    }
}

// Slot implementations

void IntegratedPlateSolver::onWorkerSolveComplete(const PlatesolveResult& result)
{
    m_timeoutTimer->stop();
    m_solving = false;
    m_lastResult = result;
    
    qDebug() << "Plate solve completed successfully!";
    qDebug() << "  RA:" << result.ra_center << "degrees";
    qDebug() << "  Dec:" << result.dec_center << "degrees";
    qDebug() << "  Pixel scale:" << result.pixscale << "arcsec/pixel";
    qDebug() << "  Orientation:" << result.orientation << "degrees";
    
    emit solveComplete(result);
}

void IntegratedPlateSolver::onWorkerSolveFailed(const QString& error)
{
    m_timeoutTimer->stop();
    m_solving = false;
    
    qDebug() << "Plate solve failed:" << error;
    emit solveFailed(error);
}

void IntegratedPlateSolver::onWorkerSolveProgress(const QString& status)
{
    emit solveProgress(status);
}

void IntegratedPlateSolver::onSolverTimeout()
{
    if (m_solving) {
        qDebug() << "Plate solve timeout after" << m_options.cpuLimit << "seconds";
        m_solving = false;
        emit solveFailed("Plate solve timeout");
    }
}

// ExtractStarsWithPlateSolve Implementation (unchanged interface)

ExtractStarsWithPlateSolve::ExtractStarsWithPlateSolve(QObject* parent)
    : QObject(parent)
    , m_validator(nullptr)
    , m_autoSolve(false)
    , m_currentImageData(nullptr)
{
    m_solver = new IntegratedPlateSolver(this);
    
    connect(m_solver, &IntegratedPlateSolver::solveComplete,
            this, &ExtractStarsWithPlateSolve::onSolveComplete);
    connect(m_solver, &IntegratedPlateSolver::solveFailed,
            this, &ExtractStarsWithPlateSolve::onSolveFailed);
    
    // Forward solver signals
    connect(m_solver, &IntegratedPlateSolver::solveStarted,
            this, &ExtractStarsWithPlateSolve::platesolveStarted);
    connect(m_solver, &IntegratedPlateSolver::solveProgress,
            this, &ExtractStarsWithPlateSolve::platesolveProgress);
}

void ExtractStarsWithPlateSolve::configurePlateSolver(const QString& astrometryPath,
                                                     const QString& indexPath,
                                                     double minScale,
                                                     double maxScale)
{
    Q_UNUSED(astrometryPath)  // Not used with internal solver
    m_solver->setIndexPath(indexPath);
    m_solver->setScaleRange(minScale, maxScale);
}

void ExtractStarsWithPlateSolve::setStarCatalogValidator(StarCatalogValidator* validator)
{
    m_validator = validator;
}

void ExtractStarsWithPlateSolve::extractStarsAndSolve(const ImageData* imageData,
                                                     const QVector<QPoint>& starCenters,
                                                     const QVector<float>& starFluxes,
                                                     const QVector<float>& starRadii)
{
    Q_UNUSED(starRadii)  // Not used in current implementation
    
    m_currentImageData = imageData;
    
    qDebug() << "Extract stars and solve: found" << starCenters.size() << "stars";
    
    if (m_autoSolve && !starCenters.isEmpty() && imageData) {
        // Automatically trigger plate solving
        m_solver->solveFromStarMask(starCenters, starFluxes, imageData);
    }
}

void ExtractStarsWithPlateSolve::onSolveComplete(const PlatesolveResult& result)
{
    qDebug() << "Plate solve completed successfully!";
    qDebug() << "  RA:" << result.ra_center << "degrees";
    qDebug() << "  Dec:" << result.dec_center << "degrees";
    qDebug() << "  Pixel scale:" << result.pixscale << "arcsec/pixel";
    qDebug() << "  Orientation:" << result.orientation << "degrees";
    
    // Convert to WCS format for your existing system
    WCSData wcs = result.toWCSData(
        m_currentImageData ? m_currentImageData->width : 0,
        m_currentImageData ? m_currentImageData->height : 0
    );
    
    // Update star catalog validator if available
    if (m_validator) {
        // TODO: Set WCS data in validator
        // m_validator->setWCSData(wcs);
    }
    
    emit platesolveComplete(result, wcs);
    emit wcsDataAvailable(wcs);
}

void ExtractStarsWithPlateSolve::onSolveFailed(const QString& error)
{
    qDebug() << "Plate solve failed:" << error;
    emit platesolveFailed(error);
}

// #include "IntegratedPlateSolver.moc"
