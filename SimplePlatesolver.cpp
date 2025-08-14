#include <unistd.h>
#include "SimplePlatesolver.h"

extern "C" {
#include "astrometry/xylist.h"
#include "astrometry/fitstable.h"
#include "astrometry/fitsioutils.h"
#include "astrometry/sip_qfits.h"
}

// Implementation
SimplePlatesolver::SimplePlatesolver(QObject* parent)
    : QObject(parent)
    , m_solveFieldPath("/opt/homebrew/bin/solve-field")
    , m_indexPath("/opt/homebrew/share/astrometry")
    , m_minScale(0.1)
    , m_maxScale(60.0)
    , m_timeoutSeconds(300)
    , m_process(nullptr)
    , m_timeoutTimer(new QTimer(this))
    , m_tempDir(nullptr)
    , m_currentImageWidth(0)
    , m_currentImageHeight(0)
{
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, &SimplePlatesolver::onTimeout);
}

void SimplePlatesolver::configurePlateSolver(const QString& astrometryPath,
                                           const QString& indexPath,
                                           double minScale,
                                           double maxScale)
{
    m_solveFieldPath = astrometryPath;
    m_indexPath = indexPath;
    m_minScale = minScale;
    m_maxScale = maxScale;
}

void SimplePlatesolver::extractStarsAndSolve(const ImageData* imageData,
                                            const QVector<QPoint>& starCenters,
                                            const QVector<float>& starFluxes,
                                            const QVector<float>& starRadii)
{
    Q_UNUSED(starRadii)  // Not used in this implementation
    
    if (isSolving()) {
        emit platesolveFailed("Solver already running");
        return;
    }
    
    if (!imageData || starCenters.isEmpty()) {
        emit platesolveFailed("No image data or stars provided");
        return;
    }
    
    if (!QFileInfo::exists(m_solveFieldPath)) {
        emit platesolveFailed(QString("solve-field not found: %1").arg(m_solveFieldPath));
        return;
    }
    
    // Store image dimensions
    m_currentImageWidth = imageData->width;
    m_currentImageHeight = imageData->height;
    
    // Create temporary directory
    cleanup(); // Clean up any previous temp files
    m_tempDir = new QTemporaryDir();
    if (!m_tempDir->isValid()) {
        emit platesolveFailed("Failed to create temporary directory");
        cleanup();
        return;
    }
    
    // Create temporary XY file with star positions
    QString xyFilePath = createTempXYFile(starCenters, starFluxes);
    if (xyFilePath.isEmpty()) {
        emit platesolveFailed("Failed to create temporary star file");
        cleanup();
        return;
    }
    
    // Build solve-field arguments
    QStringList arguments = buildArguments(xyFilePath, imageData->width, imageData->height);
    
    qDebug() << "Starting plate solve with" << starCenters.size() << "stars";
    qDebug() << "Command:" << m_solveFieldPath << arguments.join(" ");
    
    // Setup process
    m_process = new QProcess(this);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &SimplePlatesolver::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred,
            this, &SimplePlatesolver::onProcessError);
    
    // Start timeout
    m_timeoutTimer->start(m_timeoutSeconds * 1000);
    
    // Start solving
    emit platesolveStarted();
    emit platesolveProgress("Starting plate solve...");
    
    m_process->start(m_solveFieldPath, arguments);
    
    if (!m_process->waitForStarted(5000)) {
        emit platesolveFailed("Failed to start solve-field process");
        cleanup();
    }
}

bool SimplePlatesolver::isSolving() const
{
    return m_process && m_process->state() == QProcess::Running;
}

void SimplePlatesolver::cancelSolve()
{
    if (isSolving()) {
        m_process->kill();
        m_timeoutTimer->stop();
        cleanup();
        emit platesolveFailed("Solve cancelled");
    }
}

QString SimplePlatesolver::createTempXYFile(const QVector<QPoint>& starCenters, 
                                          const QVector<float>& starFluxes)
{
  QString filepath;
        // Create a temporary file to hold the xylist data
        char tempName[] = "/tmp/astro_stars_XXXXXX.xyls";
        int fd = mkstemps(tempName, 5);  // 5 = length of ".xyls"
        if (fd == -1) {
            qDebug() << "Failed to create temporary xylist file";
            return filepath;
        }
        close(fd);
        
        std::string tempFilename(tempName);
        
        // Create xylist for writing
        xylist_t* ls = xylist_open_for_writing(tempFilename.c_str());
        if (!ls) {
            qDebug() << "Failed to create xylist for writing";
            unlink(tempFilename.c_str());
            return filepath;
        }
        
        // Write primary header with image dimensions
        qfits_header* primary_hdr = xylist_get_primary_header(ls);
        if (primary_hdr) {
            char width_str[32], height_str[32];
            snprintf(width_str, sizeof(width_str), "%d", m_currentImageWidth);
            snprintf(height_str, sizeof(height_str), "%d", m_currentImageHeight);
            qfits_header_add(primary_hdr, "IMAGEW", width_str, "Image width in pixels", NULL);
            qfits_header_add(primary_hdr, "IMAGEH", height_str, "Image height in pixels", NULL);
        }
        
        if (xylist_write_primary_header(ls)) {
            qDebug() << "Failed to write primary header";
            xylist_close(ls);
            unlink(tempFilename.c_str());
            return filepath;
        }
        
        // Start a new field (extension)
       if (xylist_next_field(ls)) {
            qDebug() << "Failed to create new field";
            xylist_close(ls);
            unlink(tempFilename.c_str());
            return filepath;
        }
        
        // Set up the field header with image dimensions
        qfits_header* field_hdr = xylist_get_header(ls);
        if (field_hdr) {
            char width_str[32], height_str[32];
            snprintf(width_str, sizeof(width_str), "%d", m_currentImageWidth);
            snprintf(height_str, sizeof(height_str), "%d", m_currentImageHeight);
            qfits_header_add(field_hdr, "IMAGEW", width_str, "Image width in pixels", NULL);
            qfits_header_add(field_hdr, "IMAGEH", height_str, "Image height in pixels", NULL);
        }
        
        // Write the field header
        if (xylist_write_header(ls)) {
            qDebug() << "Failed to write field header";
            xylist_close(ls);
            unlink(tempFilename.c_str());
            return filepath;
        }
        
        // Write all star data
	for (int i = 0; i < starCenters.size(); ++i) {
	  const QPoint& center = starCenters[i];
	  float flux = (i < starFluxes.size()) ? starFluxes[i] : 1000.0f;
	  if (xylist_write_one_row_data(ls, center.x()+1, center.y()+1, flux, 0.0)) {
                qDebug() << "Failed to write star data";
                xylist_close(ls);
                unlink(tempFilename.c_str());
                return filepath;
            }
        }
        
        // Fix the header (update NAXIS2 with actual row count)
        if (xylist_fix_header(ls)) {
            qDebug() << "Failed to fix field header";
            xylist_close(ls);
            unlink(tempFilename.c_str());
            return filepath;
        }
        
        // Fix the primary header
        if (xylist_fix_primary_header(ls)) {
            qDebug() << "Failed to fix primary header";
            xylist_close(ls);
            unlink(tempFilename.c_str());
            return filepath;
        }
        
        // Close the writing xylist
        if (xylist_close(ls)) {
            qDebug() << "Failed to close xylist after writing";
            unlink(tempFilename.c_str());
            return filepath;
        }
  
	filepath = QString(tempFilename.c_str());
        
    qDebug() << "Created XY file with" << starCenters.size() << "stars:" << filepath;
    return filepath;
}

QStringList SimplePlatesolver::buildArguments(const QString& xyFilePath, int imageWidth, int imageHeight)
{
    QStringList args;
    
    // Input XY list file
    args << xyFilePath;
    
    // Image dimensions (required when using XY list without image)
    args << "--width" << QString::number(imageWidth);
    args << "--height" << QString::number(imageHeight);
    
    // Output directory
    args << "--dir" << m_tempDir->path();
    
    // Scale constraints
    args << "--scale-low" << QString::number(m_minScale);
    args << "--scale-high" << QString::number(m_maxScale);
    args << "--scale-units" << "arcsecperpix";
    
    // Performance and output options
    args << "--downsample" << "2";         // Downsample for speed
    args << "--no-plots";                  // Don't generate plots
    args << "--overwrite";                 // Overwrite existing files
    args << "--no-verify";                 // Skip verification for speed
    args << "--crpix-center";              // Set reference pixel to center
    //    args << "--timeout" << QString::number(m_timeoutSeconds);
    
    // Index path
    if (!m_indexPath.isEmpty() && QFileInfo::exists(m_indexPath)) {
        args << "--index-dir" << m_indexPath;
    }
    
    // Limit number of field attempts for faster solving
    args << "--depth" << "10,20,30,40,50";
    
    return args;
}

void SimplePlatesolver::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_timeoutTimer->stop();
    
    if (exitStatus == QProcess::CrashExit) {
        emit platesolveFailed("solve-field process crashed");
        cleanup();
        return;
    }
    
    if (exitCode != 0) {
        // Read any error output
        QString errorOutput;
        if (m_process) {
            errorOutput = QString::fromUtf8(m_process->readAllStandardError());
        }
        
        QString errorMsg = QString("Plate solving failed (exit code %1)").arg(exitCode);
        if (!errorOutput.isEmpty()) {
            errorMsg += "\nError: " + errorOutput.trimmed();
        }
        
        emit platesolveFailed(errorMsg);
        cleanup();
        return;
    }
    
    // Success! Look for the WCS file
    emit platesolveProgress("Parsing results...");
    
    QString wcsPath = m_tempDir->path() + "/" + QFileInfo(m_tempDir->path() + "/stars.xyls").baseName() + ".wcs";
    
    // Try alternative WCS file locations
    if (!QFileInfo::exists(wcsPath)) {
        QDir tempDir(m_tempDir->path());
        QStringList wcsFiles = tempDir.entryList(QStringList() << "*.wcs", QDir::Files);
        if (!wcsFiles.isEmpty()) {
            wcsPath = m_tempDir->path() + "/" + wcsFiles.first();
        }
    }
    
    if (!QFileInfo::exists(wcsPath)) {
        emit platesolveFailed("No WCS solution found - image may not have been solved");
        cleanup();
        return;
    }
    
    // Parse the WCS solution
    PlatesolveResult result = parseWCSOutput(wcsPath);
    
    if (result.solved) {
        // Convert to WCS data for compatibility
        WCSData wcs;
        wcs.crval1 = result.ra_center;
        wcs.crval2 = result.dec_center;
        wcs.crpix1 = result.crpix1;
        wcs.crpix2 = result.crpix2;
        wcs.cd11 = result.cd11;
        wcs.cd12 = result.cd12;
        wcs.cd21 = result.cd21;
        wcs.cd22 = result.cd22;
        wcs.pixscale = result.pixscale;
	/*
	  wcs.rotation = result.orientation;
	  wcs.imageWidth = m_currentImageWidth;
	  wcs.imageHeight = m_currentImageHeight;
	*/
        wcs.isValid = true;
        
        emit platesolveComplete(result, wcs);
        emit wcsDataAvailable(wcs);
    } else {
        emit platesolveFailed("Failed to parse WCS solution");
    }
    
    cleanup();
}

void SimplePlatesolver::onProcessError(QProcess::ProcessError error)
{
    m_timeoutTimer->stop();
    
    QString errorMsg;
    switch (error) {
    case QProcess::FailedToStart:
        errorMsg = "Failed to start solve-field. Check path: " + m_solveFieldPath;
        break;
    case QProcess::Crashed:
        errorMsg = "solve-field process crashed unexpectedly";
        break;
    case QProcess::Timedout:
        errorMsg = "solve-field process timed out";
        break;
    case QProcess::WriteError:
        errorMsg = "Write error with solve-field process";
        break;
    case QProcess::ReadError:
        errorMsg = "Read error with solve-field process";
        break;
    default:
        errorMsg = "Unknown error with solve-field process";
    }
    
    emit platesolveFailed(errorMsg);
    cleanup();
}

void SimplePlatesolver::onTimeout()
{
    if (isSolving()) {
        m_process->kill();
        emit platesolveFailed(QString("Plate solve timed out after %1 seconds").arg(m_timeoutSeconds));
        cleanup();
    }
}

PlatesolveResult SimplePlatesolver::parseWCSOutput(const QString& outputPath)
{
    PlatesolveResult result;
    result.solved = false;
    
    // Simple FITS header parsing - you can enhance this with CFITSIO if needed
    QFile file(outputPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Cannot open WCS file:" << outputPath;
        return result;
    }
    
    QTextStream in(&file);
    QString content = in.readAll();
    file.close();
    
    QRegularExpression crval1Regex(R"(CRVAL1\s*=\s*([\d\.-]+))");
    QRegularExpression crval2Regex(R"(CRVAL2\s*=\s*([\d\.-]+))");
    QRegularExpression crpix1Regex(R"(CRPIX1\s*=\s*([\d\.-]+))");
    QRegularExpression crpix2Regex(R"(CRPIX2\s*=\s*([\d\.-]+))");
    QRegularExpression cd11Regex(R"(CD1_1\s*=\s*([\d\.-]+[eE]?[\+\-]?\d*))");
    QRegularExpression cd12Regex(R"(CD1_2\s*=\s*([\d\.-]+[eE]?[\+\-]?\d*))");
    QRegularExpression cd21Regex(R"(CD2_1\s*=\s*([\d\.-]+[eE]?[\+\-]?\d*))");
    QRegularExpression cd22Regex(R"(CD2_2\s*=\s*([\d\.-]+[eE]?[\+\-]?\d*))");
    
    QRegularExpressionMatch match;
    
    match = crval1Regex.match(content);
    if (match.hasMatch()) {
        result.ra_center = match.captured(1).toDouble();
    }
    
    match = crval2Regex.match(content);
    if (match.hasMatch()) {
        result.dec_center = match.captured(1).toDouble();
    }
    
    match = crpix1Regex.match(content);
    if (match.hasMatch()) {
        result.crpix1 = match.captured(1).toDouble();
    }
    
    match = crpix2Regex.match(content);
    if (match.hasMatch()) {
        result.crpix2 = match.captured(1).toDouble();
    }
    
    match = cd11Regex.match(content);
    if (match.hasMatch()) {
        result.cd11 = match.captured(1).toDouble();
    }
    
    match = cd12Regex.match(content);
    if (match.hasMatch()) {
        result.cd12 = match.captured(1).toDouble();
    }
    
    match = cd21Regex.match(content);
    if (match.hasMatch()) {
        result.cd21 = match.captured(1).toDouble();
    }
    
    match = cd22Regex.match(content);
    if (match.hasMatch()) {
        result.cd22 = match.captured(1).toDouble();
    }
    
    // Calculate derived values
    if (result.cd11 != 0.0 || result.cd12 != 0.0) {
        // Pixel scale from CD matrix (convert to arcsec/pixel)
        result.pixscale = sqrt(result.cd11 * result.cd11 + result.cd12 * result.cd12) * 3600.0;
        
        // Orientation from CD matrix
        result.orientation = atan2(result.cd12, result.cd11) * 180.0 / M_PI;
        if (result.orientation < 0) result.orientation += 360.0;
        
        // Field size estimates (approximate)
        result.fieldWidth = m_currentImageWidth * result.pixscale / 60.0;   // arcmin
        result.fieldHeight = m_currentImageHeight * result.pixscale / 60.0; // arcmin
        
        result.solved = true;
        result.errorMessage = "";
        
        qDebug() << "Parsed WCS solution:";
        qDebug() << "  RA:" << result.ra_center << "degrees";
        qDebug() << "  Dec:" << result.dec_center << "degrees";
        qDebug() << "  Pixel scale:" << result.pixscale << "arcsec/px";
        qDebug() << "  Orientation:" << result.orientation << "degrees";
    } else {
        result.errorMessage = "Invalid or missing CD matrix in WCS file";
        qDebug() << "Failed to parse WCS: no valid CD matrix found";
    }
    
    return result;
}

void SimplePlatesolver::cleanup()
{
    if (m_process) {
        m_process->disconnect();
        m_process->deleteLater();
        m_process = nullptr;
    }
    
    if (m_tempDir) {
        delete m_tempDir;
        m_tempDir = nullptr;
    }
}

// Usage example - replace your existing plate solver initialization:
/*
void MainWindow::initializePlatesolveIntegration()
{
    // Replace your complex IntegratedPlateSolver with this simple version
    m_platesolveIntegration = new SimplePlatesolver(this);
    
    // Configure with your existing settings
    m_platesolveIntegration->configurePlateSolver(
        m_astrometryPath,    // "/opt/homebrew/bin/solve-field"
        m_indexPath,         // "/opt/homebrew/share/astrometry"
        m_minScale,          // 0.1
        m_maxScale           // 60.0
    );
    
    m_platesolveIntegration->setTimeout(m_platesolveTimeout);
    
    // Connect signals (same as before)
    connect(m_platesolveIntegration, &SimplePlatesolver::platesolveStarted,
            this, &MainWindow::onPlatesolveStarted);
    connect(m_platesolveIntegration, &SimplePlatesolver::platesolveProgress,
            this, &MainWindow::onPlatesolveProgress);
    connect(m_platesolveIntegration, &SimplePlatesolver::platesolveComplete,
            this, &MainWindow::onPlatesolveComplete);
    connect(m_platesolveIntegration, &SimplePlatesolver::platesolveFailed,
            this, &MainWindow::onPlatesolveFailed);
    connect(m_platesolveIntegration, &SimplePlatesolver::wcsDataAvailable,
            this, &MainWindow::onWCSDataReceived);
}

// Your existing test button will work unchanged:
void MainWindow::onTestPlatesolveWithStarExtraction()
{
    if (!m_imageData) {
        QMessageBox::information(this, "Test Plate Solve", "No image loaded.");
        return;
    }
    
    StarMaskResult starMask = performStarExtraction();
    
    if (starMask.starCenters.isEmpty()) {
        QMessageBox::information(this, "Test Plate Solve", "No stars detected.");
        return;
    }
    
    // This will now use subprocess solve-field instead of C library
    m_platesolveIntegration->extractStarsAndSolve(
        m_imageData,
        starMask.starCenters,
        starMask.starFluxes,
        starMask.starRadii
    );
}
*/
