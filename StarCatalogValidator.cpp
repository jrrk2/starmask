#include "StarCatalogValidator.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QUrlQuery>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QRegularExpression>
#include <QEventLoop>
#include <QTimer>
#include <cmath>
#include <algorithm>

StarCatalogValidator::StarCatalogValidator(QObject* parent)
    : QObject(parent)
    , m_catalogSource(Hipparcos)
    , m_validationMode(Loose)
    , m_pixelTolerance(5.0)
    , m_magnitudeTolerance(2.0)
    , m_magnitudeLimit(12.0)
    , m_networkManager(std::make_unique<QNetworkAccessManager>(this))
    , m_currentReply(nullptr)
    , m_wcsMatrixValid(false)
    , m_det(0.0)
{
    initializeTolerances();
    
    // Initialize transformation matrix
    for (int i = 0; i < 4; ++i) {
        m_transformMatrix[i] = 0.0;
    }
}

StarCatalogValidator::~StarCatalogValidator()
{
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
    }
}

void StarCatalogValidator::initializeTolerances()
{
    switch (m_validationMode) {
        case Strict:
            m_pixelTolerance = 2.0;
            m_magnitudeTolerance = 1.0;
            break;
        case Normal:
            m_pixelTolerance = 5.0;
            m_magnitudeTolerance = 2.0;
            break;
        case Loose:
            m_pixelTolerance = 10.0;
            m_magnitudeTolerance = 3.0;
            break;
    }
}

void StarCatalogValidator::setCatalogSource(CatalogSource source)
{
    m_catalogSource = source;
    qDebug() << "Catalog source set to:" << static_cast<int>(source);
}

void StarCatalogValidator::setValidationMode(ValidationMode mode)
{
    m_validationMode = mode;
    initializeTolerances();
    qDebug() << "Validation mode set to:" << static_cast<int>(mode);
}

void StarCatalogValidator::setMatchingTolerance(double pixelTolerance, double magnitudeTolerance)
{
    m_pixelTolerance = pixelTolerance;
    m_magnitudeTolerance = magnitudeTolerance;
    qDebug() << "Tolerances set - Pixel:" << pixelTolerance << "Magnitude:" << magnitudeTolerance;
}

void StarCatalogValidator::setMagnitudeLimit(double faintestMagnitude)
{
    m_magnitudeLimit = faintestMagnitude;
    qDebug() << "Magnitude limit set to:" << faintestMagnitude;
}

bool StarCatalogValidator::setWCSData(const WCSData& wcs)
{
    m_wcsData = wcs;
    
    if (wcs.isValid) {
        updateWCSMatrix();
        qDebug() << "WCS data set successfully";
        qDebug() << "  Center RA/Dec:" << wcs.crval1 << wcs.crval2;
        qDebug() << "  Reference pixel:" << wcs.crpix1 << wcs.crpix2;
        qDebug() << "  Pixel scale:" << wcs.pixscale << "arcsec/pixel";
        return true;
    } else {
        qDebug() << "Invalid WCS data provided";
        return false;
    }
}

void StarCatalogValidator::setWCSFromMetadata(const QStringList& metadata)
{
    WCSData wcs;
    
    qDebug() << "=== Extracting WCS from metadata ===";
    qDebug() << "Total metadata lines:" << metadata.size();
    
    // Debug: Show first few metadata lines to understand the format
    qDebug() << "=== First 10 metadata lines ===";
    for (int i = 0; i < qMin(10, metadata.size()); ++i) {
        qDebug() << "Line" << i << ":" << metadata[i];
    }
    
    // Look for specific WCS keywords to debug
    for (int i = 0; i < metadata.size(); ++i) {
        const QString& line = metadata[i];
        if (line.contains("CRVAL1", Qt::CaseInsensitive) || 
            line.contains("CRVAL2", Qt::CaseInsensitive) ||
            line.contains("CD1_1", Qt::CaseInsensitive) ||
            line.contains("CD2_2", Qt::CaseInsensitive)) {
            qDebug() << "WCS keyword line" << i << ":" << line;
        }
    }
    
    // Parse common WCS keywords from metadata
    for (const QString& line : metadata) {
        if (line.contains("CRVAL1", Qt::CaseInsensitive)) {
            QRegularExpression re(R"(CRVAL1:\s*([+-]?\d*\.?\d*(?:[eE][+-]?\d+)?))");
            auto match = re.match(line);
            if (match.hasMatch()) {
                wcs.crval1 = match.captured(1).toDouble();
                qDebug() << "Found CRVAL1:" << wcs.crval1;
            }
        } else if (line.contains("CRVAL2", Qt::CaseInsensitive)) {
            QRegularExpression re(R"(CRVAL2:\s*([+-]?\d*\.?\d*(?:[eE][+-]?\d+)?))");
            auto match = re.match(line);
            if (match.hasMatch()) {
                wcs.crval2 = match.captured(1).toDouble();
                qDebug() << "Found CRVAL2:" << wcs.crval2;
            }
        } else if (line.contains("CRPIX1", Qt::CaseInsensitive)) {
            QRegularExpression re(R"(CRPIX1:\s*([+-]?\d*\.?\d*(?:[eE][+-]?\d+)?))");
            auto match = re.match(line);
            if (match.hasMatch()) {
                wcs.crpix1 = match.captured(1).toDouble();
                qDebug() << "Found CRPIX1:" << wcs.crpix1;
            }
        } else if (line.contains("CRPIX2", Qt::CaseInsensitive)) {
            QRegularExpression re(R"(CRPIX2:\s*([+-]?\d*\.?\d*(?:[eE][+-]?\d+)?))");
            auto match = re.match(line);
            if (match.hasMatch()) {
                wcs.crpix2 = match.captured(1).toDouble();
                qDebug() << "Found CRPIX2:" << wcs.crpix2;
            }
        } else if (line.contains("CD1_1", Qt::CaseInsensitive)) {
            QRegularExpression re(R"(CD1_1:\s*([+-]?\d*\.?\d*(?:[eE][+-]?\d+)?))");
            auto match = re.match(line);
            if (match.hasMatch()) {
                wcs.cd11 = match.captured(1).toDouble();
                qDebug() << "Found CD1_1:" << wcs.cd11;
            }
        } else if (line.contains("CD1_2", Qt::CaseInsensitive)) {
            QRegularExpression re(R"(CD1_2:\s*([+-]?\d*\.?\d*(?:[eE][+-]?\d+)?))");
            auto match = re.match(line);
            if (match.hasMatch()) {
                wcs.cd12 = match.captured(1).toDouble();
                qDebug() << "Found CD1_2:" << wcs.cd12;
            }
        } else if (line.contains("CD2_1", Qt::CaseInsensitive)) {
            QRegularExpression re(R"(CD2_1:\s*([+-]?\d*\.?\d*(?:[eE][+-]?\d+)?))");
            auto match = re.match(line);
            if (match.hasMatch()) {
                wcs.cd21 = match.captured(1).toDouble();
                qDebug() << "Found CD2_1:" << wcs.cd21;
            }
        } else if (line.contains("CD2_2", Qt::CaseInsensitive)) {
            QRegularExpression re(R"(CD2_2:\s*([+-]?\d*\.?\d*(?:[eE][+-]?\d+)?))");
            auto match = re.match(line);
            if (match.hasMatch()) {
                wcs.cd22 = match.captured(1).toDouble();
                qDebug() << "Found CD2_2:" << wcs.cd22;
            }
        } else if (line.contains("PIXSCALE", Qt::CaseInsensitive)) {
            QRegularExpression re(R"(PIXSCALE:\s*([+-]?\d*\.?\d*(?:[eE][+-]?\d+)?))");
            auto match = re.match(line);
            if (match.hasMatch()) {
                wcs.pixscale = match.captured(1).toDouble();
                qDebug() << "Found PIXSCALE:" << wcs.pixscale;
            }
        } else if (line.contains("ORIENTAT", Qt::CaseInsensitive) || line.contains("ROTATION", Qt::CaseInsensitive)) {
            QRegularExpression re(R"((?:ORIENTAT|ROTATION):\s*([+-]?\d*\.?\d*(?:[eE][+-]?\d+)?))");
            auto match = re.match(line);
            if (match.hasMatch()) {
                wcs.orientation = match.captured(1).toDouble();
                qDebug() << "Found ORIENTAT/ROTATION:" << wcs.orientation;
            }
        } else if (line.contains("NAXIS1", Qt::CaseInsensitive)) {
            QRegularExpression re(R"(NAXIS1:\s*(\d+))");
            auto match = re.match(line);
            if (match.hasMatch()) {
                wcs.width = match.captured(1).toInt();
                qDebug() << "Found NAXIS1 (width):" << wcs.width;
            }
        } else if (line.contains("NAXIS2", Qt::CaseInsensitive)) {
            QRegularExpression re(R"(NAXIS2:\s*(\d+))");
            auto match = re.match(line);
            if (match.hasMatch()) {
                wcs.height = match.captured(1).toInt();
                qDebug() << "Found NAXIS2 (height):" << wcs.height;
            }
        } else if (line.contains("Dimensions", Qt::CaseInsensitive)) {
            QRegularExpression re(R"((\d+)\s*[×x]\s*(\d+))");
            auto match = re.match(line);
            if (match.hasMatch()) {
                wcs.width = match.captured(1).toInt();
                wcs.height = match.captured(2).toInt();
                qDebug() << "Found dimensions:" << wcs.width << "x" << wcs.height;
            }
        }
    }
    
    // Calculate pixel scale from CD matrix if available but pixscale not directly provided
    if (wcs.pixscale == 0.0 && (wcs.cd11 != 0.0 || wcs.cd12 != 0.0 || wcs.cd21 != 0.0 || wcs.cd22 != 0.0)) {
        // Calculate pixel scale from CD matrix
        // Pixel scale = sqrt(CD1_1^2 + CD2_1^2) converted from degrees to arcseconds
        double cd11_arcsec = wcs.cd11 * 3600.0; // Convert from degrees to arcseconds
        double cd21_arcsec = wcs.cd21 * 3600.0;
        wcs.pixscale = sqrt(cd11_arcsec * cd11_arcsec + cd21_arcsec * cd21_arcsec);
        
        qDebug() << "Calculated pixel scale from CD matrix:" << wcs.pixscale << "arcsec/pixel";
        qDebug() << "CD matrix elements: CD11=" << wcs.cd11 << "CD21=" << wcs.cd21;
    }
    
    // Check if we have minimum required WCS data
    bool hasBasicWCS = (wcs.crval1 != 0.0 || wcs.crval2 != 0.0) && 
                       (wcs.cd11 != 0.0 || wcs.cd22 != 0.0 || wcs.pixscale != 0.0);
    
    if (hasBasicWCS) {
        // If we have pixscale but no CD matrix, create simple CD matrix
        if (wcs.cd11 == 0.0 && wcs.cd22 == 0.0 && wcs.pixscale != 0.0) {
            double pixscaleRad = wcs.pixscale / 3600.0 * M_PI / 180.0; // Convert to radians
            double cosRot = cos(wcs.orientation * M_PI / 180.0);
            double sinRot = sin(wcs.orientation * M_PI / 180.0);
            
            wcs.cd11 = -pixscaleRad * cosRot;
            wcs.cd12 = pixscaleRad * sinRot;
            wcs.cd21 = pixscaleRad * sinRot;
            wcs.cd22 = pixscaleRad * cosRot;
            
            qDebug() << "Created CD matrix from pixel scale:" << wcs.pixscale;
        }
        
        // Set default reference pixel if not provided
        if (wcs.crpix1 == 0.0 && wcs.crpix2 == 0.0 && wcs.width > 0 && wcs.height > 0) {
            wcs.crpix1 = wcs.width / 2.0 + 1.0;  // FITS uses 1-based indexing
            wcs.crpix2 = wcs.height / 2.0 + 1.0;
            qDebug() << "Set default reference pixel:" << wcs.crpix1 << wcs.crpix2;
        }
        
        wcs.isValid = true;
        setWCSData(wcs);
    } else {
        qDebug() << "Insufficient WCS data found in metadata";
        emit errorSignal("No valid WCS information found in image metadata");
    }
}

void StarCatalogValidator::updateWCSMatrix()
{
    if (!m_wcsData.isValid) {
        m_wcsMatrixValid = false;
        return;
    }
    
    // Set up transformation matrix for WCS calculations
    m_transformMatrix[0] = m_wcsData.cd11; // CD1_1
    m_transformMatrix[1] = m_wcsData.cd12; // CD1_2
    m_transformMatrix[2] = m_wcsData.cd21; // CD2_1
    m_transformMatrix[3] = m_wcsData.cd22; // CD2_2
    
    // Calculate determinant for inverse transformations
    m_det = m_transformMatrix[0] * m_transformMatrix[3] - m_transformMatrix[1] * m_transformMatrix[2];
    
    m_wcsMatrixValid = (std::abs(m_det) > 1e-15);
    
    if (!m_wcsMatrixValid) {
        qDebug() << "Warning: WCS transformation matrix is singular";
    }
}

QPointF StarCatalogValidator::skyToPixel(double ra, double dec) const
{
    if (!m_wcsData.isValid || !m_wcsMatrixValid) {
        return QPointF(-1, -1);
    }
    
    // Convert RA/Dec to standard coordinates (simplified tangent plane projection)
    double raRad = ra * M_PI / 180.0;
    double decRad = dec * M_PI / 180.0;
    double ra0Rad = m_wcsData.crval1 * M_PI / 180.0;
    double dec0Rad = m_wcsData.crval2 * M_PI / 180.0;
    
    double cosDec = cos(decRad);
    double cosDec0 = cos(dec0Rad);
    double sinDec = sin(decRad);
    double sinDec0 = sin(dec0Rad);
    double cosRaDiff = cos(raRad - ra0Rad);
    
    double denom = sinDec * sinDec0 + cosDec * cosDec0 * cosRaDiff;
    
    if (denom <= 0) {
        return QPointF(-1, -1); // Point is behind the plane
    }
    
    double xi = cosDec * sin(raRad - ra0Rad) / denom;
    double eta = (sinDec * cosDec0 - cosDec * sinDec0 * cosRaDiff) / denom;
    
    // Transform from standard coordinates to pixel coordinates
    // Using inverse CD matrix
    if (std::abs(m_det) < 1e-15) {
        return QPointF(-1, -1);
    }
    
    double invDet = 1.0 / m_det;
    double pixelX = (m_transformMatrix[3] * xi - m_transformMatrix[1] * eta) * invDet + m_wcsData.crpix1;
    double pixelY = (-m_transformMatrix[2] * xi + m_transformMatrix[0] * eta) * invDet + m_wcsData.crpix2;
    
    return QPointF(pixelX, pixelY);
}

QPointF StarCatalogValidator::pixelToSky(double x, double y) const
{
    if (!m_wcsData.isValid || !m_wcsMatrixValid) {
        return QPointF(-1, -1);
    }
    
    // Convert pixel coordinates to standard coordinates
    double dx = x - m_wcsData.crpix1;
    double dy = y - m_wcsData.crpix2;
    
    double xi = m_transformMatrix[0] * dx + m_transformMatrix[1] * dy;
    double eta = m_transformMatrix[2] * dx + m_transformMatrix[3] * dy;
    
    // Convert standard coordinates to RA/Dec
    double ra0Rad = m_wcsData.crval1 * M_PI / 180.0;
    double dec0Rad = m_wcsData.crval2 * M_PI / 180.0;
    
    double cosDec0 = cos(dec0Rad);
    double sinDec0 = sin(dec0Rad);
    
    double rho = sqrt(xi * xi + eta * eta);
    double c = atan(rho);
    double cosc = cos(c);
    double sinc = sin(c);
    
    double decRad, raRad;
    
    if (rho < 1e-8) {
        // Very close to reference point
        decRad = dec0Rad;
        raRad = ra0Rad;
    } else {
        decRad = asin(cosc * sinDec0 + (eta * sinc * cosDec0) / rho);
        raRad = ra0Rad + atan2(xi * sinc, rho * cosDec0 * cosc - eta * sinDec0 * sinc);
    }
    
    double ra = raRad * 180.0 / M_PI;
    double dec = decRad * 180.0 / M_PI;
    
    // Normalize RA to 0-360 range
    while (ra < 0) ra += 360.0;
    while (ra >= 360) ra -= 360.0;
    
    return QPointF(ra, dec);
}

QString StarCatalogValidator::buildCatalogQuery(double centerRA, double centerDec, double radiusDegrees)
{
    QString baseUrl;
    QString query;
    
    switch (m_catalogSource) {
        case Hipparcos:
            // Use VizieR for Hipparcos catalog
            baseUrl = "https://vizier.cds.unistra.fr/viz-bin/votable";
            query = QString("?-source=I/239/hip_main&-out.max=1000&-c=%1+%2&-c.rs=%3&-out=HIP,RAhms,DEdms,Vmag,SpType")
                   .arg(centerRA, 0, 'f', 6)
                   .arg(centerDec, 0, 'f', 6)
                   .arg(radiusDegrees, 0, 'f', 4);
            break;
            
        case Tycho2:
            baseUrl = "https://vizier.cds.unistra.fr/viz-bin/votable";
            query = QString("?-source=I/259/tyc2&-out.max=2000&-c=%1+%2&-c.rs=%3&-out=TYC1,TYC2,TYC3,RAmdeg,DEmdeg,VTmag")
                   .arg(centerRA, 0, 'f', 6)
                   .arg(centerDec, 0, 'f', 6)
                   .arg(radiusDegrees, 0, 'f', 4);
            break;
            
        case Gaia:
            // Use ESA Gaia archive (TAP service)
            baseUrl = "https://gea.esac.esa.int/tap-server/tap/sync";
	    query = QString("?REQUEST=doQuery&LANG=ADQL&FORMAT=json&QUERY="
			   "SELECT TOP %1 source_id,ra,dec,phot_g_mean_mag "
			   "FROM gaiadr3.gaia_source "
			   "WHERE CONTAINS(POINT('ICRS',ra,dec),CIRCLE('ICRS',%2,%3,%4))=1 "
			   "ORDER BY phot_g_mean_mag")
		   .arg(2000)  // Max results
		   .arg(centerRA, 0, 'f', 6)
		   .arg(centerDec, 0, 'f', 6)
		   .arg(radiusDegrees, 0, 'f', 4);
            break;
            
        default:
            return QString();
    }
    
    return baseUrl + query;
}

void StarCatalogValidator::queryCatalog(double centerRA, double centerDec, double radiusDegrees)
{
    if (!m_wcsData.isValid) {
        emit errorSignal("No valid WCS data available for catalog query");
        return;
    }
    
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }
    
    QString queryUrl = buildCatalogQuery(centerRA, centerDec, radiusDegrees);
    if (queryUrl.isEmpty()) {
        emit errorSignal("Unable to build catalog query URL");
        return;
    }
    
    qDebug() << "Querying catalog:" << queryUrl;
    
    emit catalogQueryStarted();
    
    auto q = QUrl(queryUrl);
    QNetworkRequest request(q);
    request.setHeader(QNetworkRequest::UserAgentHeader, "StarCatalogValidator/1.0");
    request.setRawHeader("Accept", "application/json,application/x-votable+xml,text/plain");
    
    m_currentReply = m_networkManager->get(request);
    connect(m_currentReply, &QNetworkReply::finished, this, &StarCatalogValidator::onCatalogQueryFinished);
    connect(m_currentReply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &StarCatalogValidator::onNetworkError);
    
    // Set a timeout
    QTimer::singleShot(30000, this, [this]() {
        if (m_currentReply && m_currentReply->isRunning()) {
	  //            m_currentReply->abort();
            emit errorSignal("Catalog query timed out");
        }
    });
}

void StarCatalogValidator::onCatalogQueryFinished()
{
    if (!m_currentReply) return;
    
    if (m_currentReply->error() == QNetworkReply::NoError) {
        QByteArray data = m_currentReply->readAll();
        parseCatalogResponse(data);
        emit catalogQueryFinished(true, QString("Retrieved %1 catalog stars").arg(m_catalogStars.size()));
    } else {
        QString errorMsg = QString("Catalog query failed: %1").arg(m_currentReply->errorString());
        qDebug() << errorMsg;
        emit catalogQueryFinished(false, errorMsg);
        emit errorSignal(errorMsg);
    }
    
    m_currentReply->deleteLater();
    m_currentReply = nullptr;
}

void StarCatalogValidator::onNetworkError(QNetworkReply::NetworkError error)
{
    QString errorMsg = QString("Network error during catalog query: %1").arg(static_cast<int>(error));
    qDebug() << errorMsg;
    emit errorSignal(errorMsg);
}

void StarCatalogValidator::parseCatalogResponse(const QByteArray& data)
{
    m_catalogStars.clear();
    
    // Try to parse as JSON first (for Gaia)
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    
    if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
        QJsonObject obj = doc.object();
        if (obj.contains("data")) {
            parseGaiaData(obj["data"].toArray());
        }
    } else {
        // Try to parse as VOTable (XML format from VizieR)
        QString xmlData = QString::fromUtf8(data);
        parseVOTableData(xmlData);
    }
    
    // Calculate pixel positions for all catalog stars
    for (auto& star : m_catalogStars) {
        star.pixelPos = skyToPixel(star.ra, star.dec);
        // Mark as invalid if outside image bounds
        if (star.pixelPos.x() < 0 || star.pixelPos.y() < 0 ||
            star.pixelPos.x() >= m_wcsData.width || star.pixelPos.y() >= m_wcsData.height) {
            star.isValid = false;
        }
    }
    
    qDebug() << "Parsed" << m_catalogStars.size() << "catalog stars";
}

void StarCatalogValidator::parseGaiaData(const QJsonArray& stars)
{
    for (const QJsonValue& value : stars) {
        if (value.isArray()) {
            QJsonArray starData = value.toArray();
            if (starData.size() >= 4) {
                QString sourceId = QString::number(starData[0].toDouble(), 'f', 0);
                double ra = starData[1].toDouble();
                double dec = starData[2].toDouble();
                double magnitude = starData[3].toDouble();
                
                if (magnitude <= m_magnitudeLimit) {
                    CatalogStar star(QString("Gaia_%1").arg(sourceId), ra, dec, magnitude);
                    m_catalogStars.append(star);
                }
            }
        }
    }
}

void StarCatalogValidator::parseVOTableData(const QString& xmlData)
{
    // Simple XML parsing for VOTable format
    // This is a basic implementation - in production you'd use QXmlStreamReader
    QRegularExpression rowRegex(R"(<TR>.*?</TR>)", QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression tdRegex(R"(<TD[^>]*>(.*?)</TD>)");
    
    auto rowMatches = rowRegex.globalMatch(xmlData);
    
    while (rowMatches.hasNext()) {
        auto rowMatch = rowMatches.next();
        QString row = rowMatch.captured(0);
        
        auto cellMatches = tdRegex.globalMatch(row);
        QStringList cells;
        
        while (cellMatches.hasNext()) {
            auto cellMatch = cellMatches.next();
            cells.append(cellMatch.captured(1).trimmed());
        }
        
        // Parse based on catalog source
        if (m_catalogSource == Hipparcos && cells.size() >= 5) {
            QString hipId = cells[0];
            QString raStr = cells[1];
            QString decStr = cells[2];
            double magnitude = cells[3].toDouble();
            QString spectralType = cells[4];
            
            // Convert RA/Dec from sexagesimal to decimal degrees
            double ra = parseCoordinate(raStr, true);
            double dec = parseCoordinate(decStr, false);
            
            if (ra >= 0 && dec >= -90 && magnitude <= m_magnitudeLimit) {
                CatalogStar star(QString("HIP_%1").arg(hipId), ra, dec, magnitude);
                star.spectralType = spectralType;
                m_catalogStars.append(star);
            }
        } else if (m_catalogSource == Tycho2 && cells.size() >= 6) {
            QString tycId = QString("%1-%2-%3").arg(cells[0], cells[1], cells[2]);
            double ra = cells[3].toDouble();
            double dec = cells[4].toDouble();
            double magnitude = cells[5].toDouble();
            
            if (magnitude <= m_magnitudeLimit) {
                CatalogStar star(QString("TYC_%1").arg(tycId), ra, dec, magnitude);
                m_catalogStars.append(star);
            }
        }
    }
}

double StarCatalogValidator::parseCoordinate(const QString& coordStr, bool isRA) const
{
    // Parse coordinates in various formats (HH:MM:SS.S or DD:MM:SS.S)
    QRegularExpression sexagesimalRegex(R"(([+-]?\d+)[:\s]+(\d+)[:\s]+([+-]?\d+\.?\d*))");
    auto match = sexagesimalRegex.match(coordStr);
    
    if (match.hasMatch()) {
        double deg = match.captured(1).toDouble();
        double min = match.captured(2).toDouble();
        double sec = match.captured(3).toDouble();
        
        double decimal = std::abs(deg) + min/60.0 + sec/3600.0;
        if (deg < 0) decimal = -decimal;
        
        // Convert RA from hours to degrees
        if (isRA) decimal *= 15.0;
        
        return decimal;
    }
    
    // Try direct decimal parsing
    bool ok;
    double value = coordStr.toDouble(&ok);
    if (ok) {
        if (isRA && value < 24.0) value *= 15.0; // Convert hours to degrees
        return value;
    }
    
    return -999.0; // Invalid coordinate
}

void StarCatalogValidator::loadCustomCatalog(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit errorSignal(QString("Cannot open catalog file: %1").arg(filePath));
        return;
    }
    
    QTextStream in(&file);
    QVector<CatalogStar> customStars;
    
    // Skip header line if present
    QString line = in.readLine();
    if (!line.contains(QRegularExpression(R"(\d+\.?\d*)"))) {
        line = in.readLine(); // Skip header
    }
    
    int lineNumber = 1;
    do {
        QStringList parts = line.split(QRegularExpression(R"([,\t\s]+)"), Qt::SkipEmptyParts);
        
        if (parts.size() >= 3) {
            QString id = parts.size() > 3 ? parts[0] : QString("Star_%1").arg(lineNumber);
            int idxOffset = parts.size() > 3 ? 1 : 0;
            
            bool raOk, decOk, magOk = true;
            double ra = parts[idxOffset].toDouble(&raOk);
            double dec = parts[idxOffset + 1].toDouble(&decOk);
            double mag = parts.size() > idxOffset + 2 ? parts[idxOffset + 2].toDouble(&magOk) : 10.0;
            
            if (raOk && decOk && magOk && mag <= m_magnitudeLimit) {
                CatalogStar star(id, ra, dec, mag);
                if (parts.size() > idxOffset + 3) {
                    star.spectralType = parts[idxOffset + 3];
                }
                customStars.append(star);
            }
        }
        
        line = in.readLine();
        lineNumber++;
    } while (!line.isNull());
    
    loadCustomCatalog(customStars);
    qDebug() << "Loaded" << customStars.size() << "stars from custom catalog";
}

void StarCatalogValidator::loadCustomCatalog(const QVector<CatalogStar>& stars)
{
    m_catalogStars = stars;
    m_catalogSource = Custom;
    
    // Calculate pixel positions
    for (auto& star : m_catalogStars) {
        star.pixelPos = skyToPixel(star.ra, star.dec);
        if (star.pixelPos.x() < 0 || star.pixelPos.y() < 0 ||
            star.pixelPos.x() >= m_wcsData.width || star.pixelPos.y() >= m_wcsData.height) {
            star.isValid = false;
        }
    }
    
    emit catalogQueryFinished(true, QString("Loaded %1 custom catalog stars").arg(stars.size()));
}

ValidationResult StarCatalogValidator::validateStars(const QVector<QPoint>& detectedStars, 
                                                   const QVector<float>& starRadii)
{
    if (!m_wcsData.isValid) {
        ValidationResult result;
        result.summary = "No valid WCS data available";
        emit errorSignal(result.summary);
        return result;
    }
    
    if (m_catalogStars.isEmpty()) {
        ValidationResult result;
        result.summary = "No catalog stars available for validation";
        emit errorSignal(result.summary);
        return result;
    }
    
    m_lastValidation = performMatching(detectedStars, starRadii);
    emit validationCompleted(m_lastValidation);
    
    return m_lastValidation;
}

ValidationResult StarCatalogValidator::performMatching(const QVector<QPoint>& detectedStars, 
                                                     const QVector<float>& starRadii)
{
    ValidationResult result;
    result.catalogStars = m_catalogStars;
    result.totalDetected = detectedStars.size();
    result.totalCatalog = m_catalogStars.size();
    
    // Create lists to track unmatched stars
    QVector<bool> detectedMatched(detectedStars.size(), false);
    QVector<bool> catalogMatched(m_catalogStars.size(), false);
    
    // Find matches using nearest neighbor approach
    for (int i = 0; i < detectedStars.size(); ++i) {
        QPoint detected = detectedStars[i];
        double bestDistance = m_pixelTolerance;
        int bestCatalogIndex = -1;
        
        for (int j = 0; j < m_catalogStars.size(); ++j) {
            if (!m_catalogStars[j].isValid || catalogMatched[j]) continue;
            
            double distance = calculateDistance(detected, m_catalogStars[j].pixelPos);
            
            if (distance < bestDistance) {
                bestDistance = distance;
                bestCatalogIndex = j;
            }
        }
        
        if (bestCatalogIndex >= 0) {
            // Calculate magnitude difference if we have radius information
            double magnitudeDiff = 0.0;
            if (!starRadii.isEmpty() && i < starRadii.size()) {
                // Rough magnitude estimation from star radius
                // This is very approximate - real photometry would be needed
                float detectedRadius = starRadii[i];
                double estimatedMag = 10.0 - 2.5 * log10(detectedRadius * detectedRadius);
                magnitudeDiff = std::abs(estimatedMag - m_catalogStars[bestCatalogIndex].magnitude);
            }
            
            bool isGood = isGoodMatch(bestDistance, magnitudeDiff);
            
            StarMatch match(i, bestCatalogIndex, bestDistance, magnitudeDiff);
            match.isGoodMatch = isGood;
            result.matches.append(match);
            
            if (isGood) {
                detectedMatched[i] = true;
                catalogMatched[bestCatalogIndex] = true;
                result.totalMatches++;
            }
        }
    }
    
    // Collect unmatched stars
    for (int i = 0; i < detectedStars.size(); ++i) {
        if (!detectedMatched[i]) {
            result.unmatchedDetected.append(i);
        }
    }
    
    for (int i = 0; i < m_catalogStars.size(); ++i) {
        if (m_catalogStars[i].isValid && !catalogMatched[i]) {
            result.unmatchedCatalog.append(i);
        }
    }
    
    calculateStatistics(result);
    result.isValid = true;
    
    return result;
}

double StarCatalogValidator::calculateDistance(const QPoint& detected, const QPointF& catalog) const
{
    double dx = detected.x() - catalog.x();
    double dy = detected.y() - catalog.y();
    return sqrt(dx * dx + dy * dy);
}

bool StarCatalogValidator::isGoodMatch(double distance, double magnitudeDiff) const
{
    if (distance > m_pixelTolerance) return false;
    if (magnitudeDiff > m_magnitudeTolerance) return false;
    
    // Additional quality checks based on validation mode
    switch (m_validationMode) {
        case Strict:
            return distance < m_pixelTolerance * 0.5;
        case Normal:
            return true; // Already passed basic checks
        case Loose:
            return distance < m_pixelTolerance * 1.5;
    }
    
    return true;
}

void StarCatalogValidator::calculateStatistics(ValidationResult& result) const
{
    if (result.totalDetected == 0) {
        result.matchPercentage = 0.0;
        result.summary = "No detected stars to validate";
        return;
    }
    
    result.matchPercentage = (100.0 * result.totalMatches) / result.totalDetected;
    
    // Calculate position error statistics
    if (!result.matches.isEmpty()) {
        double sumError = 0.0;
        double sumSquaredError = 0.0;
        int goodMatches = 0;
        
        for (const auto& match : result.matches) {
            if (match.isGoodMatch) {
                sumError += match.distance;
                sumSquaredError += match.distance * match.distance;
                goodMatches++;
            }
        }
        
        if (goodMatches > 0) {
            result.averagePositionError = sumError / goodMatches;
            result.rmsPositionError = sqrt(sumSquaredError / goodMatches);
        }
    }
    
    // Generate summary
    result.summary = QString("Validation Results:\n"
                           "Detected: %1, Catalog: %2, Matches: %3 (%.1f%%)\n"
                           "Average position error: %.2f pixels\n"
                           "RMS position error: %.2f pixels\n"
                           "Unmatched detections: %4\n"
                           "Unmatched catalog stars: %5")
                   .arg(result.totalDetected)
                   .arg(result.totalCatalog)
                   .arg(result.totalMatches)
                   .arg(result.matchPercentage)
                   .arg(result.averagePositionError, 0, 'f', 2)
                   .arg(result.rmsPositionError, 0, 'f', 2)
                   .arg(result.unmatchedDetected.size())
                   .arg(result.unmatchedCatalog.size());
}

QString StarCatalogValidator::getValidationSummary() const
{
    return m_lastValidation.summary;
}

QJsonObject StarCatalogValidator::exportValidationResults() const
{
    const ValidationResult& result = m_lastValidation;
    
    QJsonObject obj;
    obj["totalDetected"] = result.totalDetected;
    obj["totalCatalog"] = result.totalCatalog;
    obj["totalMatches"] = result.totalMatches;
    obj["matchPercentage"] = result.matchPercentage;
    obj["averagePositionError"] = result.averagePositionError;
    obj["rmsPositionError"] = result.rmsPositionError;
    obj["summary"] = result.summary;
    
    QJsonArray matchesArray;
    for (const auto& match : result.matches) {
        QJsonObject matchObj;
        matchObj["detectedIndex"] = match.detectedIndex;
        matchObj["catalogIndex"] = match.catalogIndex;
        matchObj["distance"] = match.distance;
        matchObj["magnitudeDiff"] = match.magnitudeDiff;
        matchObj["isGoodMatch"] = match.isGoodMatch;
        matchesArray.append(matchObj);
    }
    obj["matches"] = matchesArray;
    
    QJsonArray catalogArray;
    for (const auto& star : result.catalogStars) {
        QJsonObject starObj;
        starObj["id"] = star.id;
        starObj["ra"] = star.ra;
        starObj["dec"] = star.dec;
        starObj["magnitude"] = star.magnitude;
        starObj["spectralType"] = star.spectralType;
        starObj["pixelX"] = star.pixelPos.x();
        starObj["pixelY"] = star.pixelPos.y();
        starObj["isValid"] = star.isValid;
        catalogArray.append(starObj);
    }
    obj["catalogStars"] = catalogArray;
    
    return obj;
}

void StarCatalogValidator::clearResults()
{
    m_catalogStars.clear();
    m_lastValidation.clear();
    qDebug() << "Validation results cleared";
}
