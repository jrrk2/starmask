#include "BrightStarDatabase.h"
#include "StarCatalogValidator.h"
#include "Local2MASSCatalog.h"
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

// Static member definition (add to .cpp file)
QString Local2MASSCatalog::s_catalogPath;

StarCatalogValidator::StarCatalogValidator(QObject* parent)
    : QObject(parent)
    , m_validationMode(Loose)
    , m_pixelTolerance(5.0)
    , m_magnitudeTolerance(2.0)
    , m_magnitudeLimit(12.0)
    , m_wcsMatrixValid(false)
    , m_det(0.0)
    , m_curl(curl_easy_init())
{
    initializeTolerances();
    // Initialize local 2MASS catalog
    initializeLocal2MASS();
        
    // Initialize transformation matrix
    for (int i = 0; i < 4; ++i) {
        m_transformMatrix[i] = 0.0;
    }
}

StarCatalogValidator::~StarCatalogValidator()
{
    curl_easy_cleanup(m_curl);
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

// Callback to collect HTTP response
static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// Update the queryCatalog method to handle Local2MASS
void StarCatalogValidator::queryCatalog(double centerRA, double centerDec, double radiusDegrees)
{
    if (!m_wcsData.isValid) {
        emit errorSignal("No valid WCS data available for catalog query");
        return;
    }
    
    queryLocal2MASS(centerRA, centerDec, radiusDegrees);
}

void StarCatalogValidator::queryGaiaCatalog(double centerRA, double centerDec, double radiusDegrees)
{
    if (!m_wcsData.isValid) {
        emit errorSignal("No valid WCS data available for catalog query");
        return;
    }
    
    QString baseUrl;
    QString query;

    std::string adqlQuery =
        "SELECT TOP 10000 source_id,ra,dec,phot_g_mean_mag "
        "FROM gaiaedr3.gaia_source "
        "WHERE CONTAINS(POINT('ICRS', ra, dec), "
        "CIRCLE('ICRS', " + std::to_string(centerRA) + ", " + std::to_string(centerDec) + ", " + std::to_string(radiusDegrees) + "))=1 ";

    if (!m_curl) {
      qDebug() << "Curl init failed!\n";
      return;
    }

    std::string postFields = "REQUEST=doQuery"
                             "&LANG=ADQL"
                             "&FORMAT=JSON"
      "&QUERY=" + std::string(curl_easy_escape(m_curl, adqlQuery.c_str(), adqlQuery.size()));
    
    std::string readBuffer;

    curl_easy_setopt(m_curl, CURLOPT_URL, "https://gea.esac.esa.int/tap-server/tap/sync");
    curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, postFields.c_str());
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &readBuffer);

    qDebug() << "Querying catalog:" << postFields;
    
    emit catalogQueryStarted();
    
    CURLcode res = curl_easy_perform(m_curl);
    m_catData = readBuffer.c_str();
    
    if (res != CURLE_OK)
      qDebug() << "Curl error: " << curl_easy_strerror(res) << "\n";
    else
      {
      parseCatalogResponse(m_catData);
      emit catalogQueryFinished(true, QString("Retrieved %1 catalog stars").arg(m_catalogStars.size()));
      }
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
		    qDebug() << sourceId << ra << dec << magnitude;
                    m_catalogStars.append(star);
                }
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

bool StarCatalogValidator::setWCSFromImageMetadata(const ImageData& imageData)
{
    qDebug() << "=== Using PCL AstrometricMetadata (Complete) ===";
    
    try {
        
        // Convert ImageData metadata to PCL FITSKeywordArray
        pcl::FITSKeywordArray keywords;
        
        // Parse metadata lines and convert to FITS keywords
        for (const QString& line : imageData.metadata) {
            QString trimmedLine = line.trimmed();
            
            // Extract key-value pairs in the format "KEY: value (comment)"
            QRegularExpression kvRegex(R"(([A-Z0-9_-]+):\s*([^(]+)(?:\s*\(([^)]*)\))?)", 
                                     QRegularExpression::CaseInsensitiveOption);
            auto match = kvRegex.match(trimmedLine);
            
            if (match.hasMatch()) {
                QString key = match.captured(1).toUpper();
                QString valueStr = match.captured(2).trimmed();
                QString comment = match.captured(3).trimmed();
                
                // Handle quoted string values
                if (valueStr.startsWith("'") && valueStr.endsWith("'")) {
                    valueStr = valueStr.mid(1, valueStr.length() - 2);
                }
                
                // Only add WCS-related keywords that PCL recognizes
                QStringList wcsKeys = {
                    "CRVAL1", "CRVAL2", "CRPIX1", "CRPIX2",
                    "CD1_1", "CD1_2", "CD2_1", "CD2_2",
                    "CDELT1", "CDELT2", "CROTA1", "CROTA2",
                    "CTYPE1", "CTYPE2", "CUNIT1", "CUNIT2",
                    "PV1_1", "PV1_2", "LONPOLE", "LATPOLE",
                    "EQUINOX", "RADESYS", "NAXIS1", "NAXIS2",
                    // Add some basic metadata
                    "DATE-OBS", "FOCALLEN", "XPIXSZ", "YPIXSZ"
                };
                
                if (wcsKeys.contains(key)) {
                    // Try to parse as number first
                    bool isNumber;
                    double numValue = valueStr.toDouble(&isNumber);
                    
                    if (isNumber) {
                        keywords.Add(pcl::FITSHeaderKeyword(
                            pcl::IsoString(key.toUtf8().constData()), 
                            numValue, 
                            pcl::IsoString(comment.toUtf8().constData())));
                        qDebug() << "Added numeric keyword:" << key << "=" << numValue;
                    } else {
                        // Handle as string (for CTYPE, RADESYS, etc.)
                        keywords.Add(pcl::FITSHeaderKeyword(
                            pcl::IsoString(key.toUtf8().constData()), 
                            pcl::IsoString(valueStr.toUtf8().constData()), 
                            pcl::IsoString(comment.toUtf8().constData())));
                        qDebug() << "Added string keyword:" << key << "=" << valueStr;
                    }
                }
            }
        }
        
        // Add essential default keywords if not present
        bool hasCTYPE1 = false, hasCTYPE2 = false, hasRADESYS = false;
        for (const auto& kw : keywords) {
            if (kw.name == "CTYPE1") hasCTYPE1 = true;
            if (kw.name == "CTYPE2") hasCTYPE2 = true;
            if (kw.name == "RADESYS") hasRADESYS = true;
        }
        
        if (!hasCTYPE1) {
            keywords.Add(pcl::FITSHeaderKeyword("CTYPE1", 
                pcl::IsoString("'RA---TAN'"), "Coordinate type"));
        }
        if (!hasCTYPE2) {
            keywords.Add(pcl::FITSHeaderKeyword("CTYPE2", 
                pcl::IsoString("'DEC--TAN'"), "Coordinate type"));
        }
        if (!hasRADESYS) {
            keywords.Add(pcl::FITSHeaderKeyword("RADESYS", 
                pcl::IsoString("'ICRS'"), "Coordinate reference system"));
        }
        
        qDebug() << "Created" << keywords.Length() << "FITS keywords for PCL";
        
        // Use PCL's AstrometricMetadata.Build() method
        // This is the high-level interface that handles everything!
        pcl::PropertyArray emptyProperties; // We're using FITS keywords only
        
        try {
            m_astrometricMetadata.Build(emptyProperties, keywords, 
                                       imageData.width, imageData.height);
            
            // Check if the astrometric solution is valid
            m_hasAstrometricData = m_astrometricMetadata.IsValid();
            
            if (m_hasAstrometricData) {
                // Update our WCSData for compatibility with existing code
                pcl::DPoint centerWorld;
                if (m_astrometricMetadata.ImageToCelestial(centerWorld, 
                    pcl::DPoint(imageData.width * 0.5, imageData.height * 0.5))) {
                    
                    m_wcsData.crval1 = centerWorld.x;
                    m_wcsData.crval2 = centerWorld.y;
                    m_wcsData.width = imageData.width;
                    m_wcsData.height = imageData.height;
                    m_wcsData.isValid = true;
                    
                    // Get resolution from PCL
                    m_wcsData.pixscale = m_astrometricMetadata.Resolution() * 3600.0; // arcsec/pixel
                    
                    qDebug() << "✅ PCL AstrometricMetadata setup successful!";
                    qDebug() << "  Image center: RA=" << centerWorld.x << "° Dec=" << centerWorld.y << "°";
                    qDebug() << "  Resolution:" << m_wcsData.pixscale << "arcsec/pixel";
                    qDebug() << "  Image size:" << imageData.width << "x" << imageData.height;
                    
                    // Test the coordinate transformations
                    testAstrometricMetadata();
                    
                    return true;
                    
                } else {
                    qDebug() << "❌ ImageToCelestial failed for image center";
                    m_hasAstrometricData = false;
                }
            } else {
                qDebug() << "❌ PCL AstrometricMetadata.IsValid() returned false";
                
                // Try to get more information about why it failed
                try {
                    m_astrometricMetadata.Validate(0.1); // Test with 0.1 pixel tolerance
                } catch (const pcl::Error& e) {
                    qDebug() << "  Validation error:" << e.Message().c_str();
                }
            }
            
        } catch (const pcl::Error& e) {
            qDebug() << "❌ PCL AstrometricMetadata.Build() failed:" << e.Message().c_str();
            m_hasAstrometricData = false;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        qDebug() << "❌ Standard exception in PCL AstrometricMetadata setup:" << e.what();
        return false;
    } catch (...) {
        qDebug() << "❌ Unknown exception in PCL AstrometricMetadata setup";
        return false;
    }
}

void StarCatalogValidator::testAstrometricMetadata() const
{
    if (!m_hasAstrometricData) return;
    
    qDebug() << "\n=== Testing PCL AstrometricMetadata ===";
    
    try {
        // Test 1: Image center
        pcl::DPoint centerImage(m_wcsData.width * 0.5, m_wcsData.height * 0.5);
        pcl::DPoint centerWorld;
        
        if (m_astrometricMetadata.ImageToCelestial(centerWorld, centerImage)) {
            qDebug() << QString("✅ Image center: (%1, %2) -> RA=%3° Dec=%4°")
                        .arg(centerImage.x, 0, 'f', 1).arg(centerImage.y, 0, 'f', 1)
                        .arg(centerWorld.x, 0, 'f', 6).arg(centerWorld.y, 0, 'f', 6);
        } else {
            qDebug() << "❌ Failed to transform image center";
        }
        
        // Test 2: Image corners
        struct TestPoint {
            pcl::DPoint image;
            QString name;
        };
        
        TestPoint corners[] = {
            {pcl::DPoint(0, 0), "Top-Left"},
            {pcl::DPoint(m_wcsData.width, 0), "Top-Right"},
            {pcl::DPoint(0, m_wcsData.height), "Bottom-Left"},
            {pcl::DPoint(m_wcsData.width, m_wcsData.height), "Bottom-Right"}
        };
        
        qDebug() << "\nImage corner transformations:";
        for (const auto& corner : corners) {
            pcl::DPoint world;
            if (m_astrometricMetadata.ImageToCelestial(world, corner.image)) {
                qDebug() << QString("  %1: (%2, %3) -> RA=%4° Dec=%5°")
                            .arg(corner.name).arg(corner.image.x, 0, 'f', 0).arg(corner.image.y, 0, 'f', 0)
                            .arg(world.x, 0, 'f', 4).arg(world.y, 0, 'f', 4);
            } else {
                qDebug() << QString("  %1: Transformation failed").arg(corner.name);
            }
        }
        
        // Test 3: Catalog stars (your Gaia data)
        struct TestStar {
            double ra, dec;
            QString name;
        };
        
        TestStar testStars[] = {
            {11.195908490324188, 41.89174375779233, "Gaia_387310087645581056"},
            {11.245222869774707, 41.818567129781435, "Gaia_387309567953910528"},
            {11.25642400913238, 41.84593075636271, "Gaia_387309705392866432"},
            {centerWorld.x, centerWorld.y, "Image_Center_Roundtrip"}
        };
        
        qDebug() << "\nCatalog star transformations:";
        int inBoundsCount = 0;
        
        for (const auto& star : testStars) {
            pcl::DPoint worldCoord(star.ra, star.dec);
            pcl::DPoint imageCoord;
            
            if (m_astrometricMetadata.CelestialToImage(imageCoord, worldCoord)) {
                bool inBounds = (imageCoord.x >= 0 && imageCoord.x < m_wcsData.width && 
                               imageCoord.y >= 0 && imageCoord.y < m_wcsData.height);
                if (inBounds) inBoundsCount++;
                
                qDebug() << QString("  %1: RA=%2° Dec=%3° -> pixel=(%4,%5) %6")
                            .arg(star.name).arg(star.ra, 0, 'f', 6).arg(star.dec, 0, 'f', 6)
                            .arg(imageCoord.x, 0, 'f', 1).arg(imageCoord.y, 0, 'f', 1)
                            .arg(inBounds ? "✅" : "❌");
            } else {
                qDebug() << QString("  %1: CelestialToImage failed").arg(star.name);
            }
        }
        
        qDebug() << QString("\n📊 Results: %1/%2 stars in bounds").arg(inBoundsCount).arg(4);
        
        if (inBoundsCount > 0) {
            qDebug() << "🎉 SUCCESS: PCL AstrometricMetadata is working correctly!";
            qDebug() << "    Your catalog stars should now appear on the image.";
        } else {
            qDebug() << "⚠️  WARNING: No stars are landing within image bounds.";
            qDebug() << "    Check if the WCS keywords in your FITS file are correct.";
        }
        
        // Test 4: Round-trip accuracy test
        qDebug() << "\nRound-trip accuracy test:";
        pcl::DPoint testPixel(m_wcsData.width * 0.25, m_wcsData.height * 0.75);
        pcl::DPoint worldCoord, backToPixel;
        
        if (m_astrometricMetadata.ImageToCelestial(worldCoord, testPixel) &&
            m_astrometricMetadata.CelestialToImage(backToPixel, worldCoord)) {
            
            double errorX = testPixel.x - backToPixel.x;
            double errorY = testPixel.y - backToPixel.y;
            double totalError = sqrt(errorX*errorX + errorY*errorY);
            
            qDebug() << QString("  Original: (%1, %2)")
                        .arg(testPixel.x, 0, 'f', 3).arg(testPixel.y, 0, 'f', 3);
            qDebug() << QString("  Round-trip: (%1, %2)")
                        .arg(backToPixel.x, 0, 'f', 3).arg(backToPixel.y, 0, 'f', 3);
            qDebug() << QString("  Error: %.6f pixels %1")
                        .arg(totalError).arg(totalError < 0.1 ? "✅" : "⚠️");
        }
        
        // Test 5: Print PCL's diagnostic info
        qDebug() << "\nPCL Internal Diagnostics:";
        try {
            // Test the validation
            m_astrometricMetadata.Validate(0.01);
            qDebug() << "  ✅ PCL internal validation passed (0.01 px tolerance)";
        } catch (const pcl::Error& e) {
            qDebug() << "  ❌ PCL validation failed:" << e.Message().c_str();
        }
        
        // Get search radius for catalog queries
        try {
            double searchRadius = m_astrometricMetadata.SearchRadius();
            qDebug() << QString("  📡 Search radius: %1° (for catalog queries)")
                        .arg(searchRadius, 0, 'f', 2);
        } catch (const pcl::Error& e) {
            qDebug() << "  ❌ SearchRadius failed:" << e.Message().c_str();
        }
        
    } catch (const pcl::Error& e) {
        qDebug() << "❌ PCL error during testing:" << e.Message().c_str();
    } catch (...) {
        qDebug() << "❌ Unknown error during testing";
    }
}

// Simplified coordinate transformation methods using PCL's high-level API
QPointF StarCatalogValidator::skyToPixel(double ra, double dec) const
{
    if (!m_hasAstrometricData) {
        return QPointF(-1, -1);
    }
    
    try {
        pcl::DPoint worldCoord(ra, dec);
        pcl::DPoint imageCoord;
        
        if (m_astrometricMetadata.CelestialToImage(imageCoord, worldCoord)) {
            return QPointF(imageCoord.x, imageCoord.y);
        }
        
    } catch (const pcl::Error& e) {
        qDebug() << "skyToPixel error:" << e.Message().c_str();
    }
    
    return QPointF(-1, -1);
}

QPointF StarCatalogValidator::pixelToSky(double x, double y) const
{
    if (!m_hasAstrometricData) {
        return QPointF(-1, -1);
    }
    
    try {
        pcl::DPoint imageCoord(x, y);
        pcl::DPoint worldCoord;
        
        if (m_astrometricMetadata.ImageToCelestial(worldCoord, imageCoord)) {
            return QPointF(worldCoord.x, worldCoord.y);
        }
        
    } catch (const pcl::Error& e) {
        qDebug() << "pixelToSky error:" << e.Message().c_str();
    }
    
    return QPointF(-1, -1);
}

// Update your other methods
bool StarCatalogValidator::hasValidWCS() const 
{ 
    return m_hasAstrometricData && m_astrometricMetadata.IsValid(); 
}

WCSData StarCatalogValidator::getWCSData() const 
{ 
    return m_wcsData; 
}

// Update StarCatalogValidator to use the local bright star database
void StarCatalogValidator::addBrightStarsFromDatabase(double centerRA, double centerDec, double radiusDegrees)
{
    qDebug() << "\n=== ADDING BRIGHT STARS FROM LOCAL DATABASE ===";
    
    // Get bright stars in field (magnitude < 3.0 to ensure visibility)
    auto brightStars = BrightStarDatabase::getStarsInField(centerRA, centerDec, radiusDegrees, 3.0);
    
    if (brightStars.isEmpty()) {
        qDebug() << "No bright stars (mag < 3.0) found in this field";
        return;
    }
    
    qDebug() << QString("Found %1 bright stars in field:").arg(brightStars.size());
    
    int addedCount = 0;
    int replacedCount = 0;
    
    for (const auto& brightStar : brightStars) {
        // Check if magnitude is within our limit
        if (brightStar.magnitude > m_magnitudeLimit) {
            qDebug() << QString("  Skipping %1 (mag %.2f > limit %.2f)")
                        .arg(brightStar.name).arg(brightStar.magnitude).arg(m_magnitudeLimit);
            continue;
        }
        
        // Create catalog star
        CatalogStar catalogStar(brightStar.id, brightStar.ra, brightStar.dec, brightStar.magnitude);
        catalogStar.spectralType = brightStar.spectralType;
        
        // Calculate pixel position
        catalogStar.pixelPos = skyToPixel(brightStar.ra, brightStar.dec);
        
        // Check if in image bounds
        catalogStar.isValid = (catalogStar.pixelPos.x() >= 0 && catalogStar.pixelPos.x() < m_wcsData.width && 
                              catalogStar.pixelPos.y() >= 0 && catalogStar.pixelPos.y() < m_wcsData.height);
        
        // Check if we already have a star at this position (replace if ours is better)
        bool replaced = false;
        for (int i = 0; i < m_catalogStars.size(); ++i) {
            auto& existing = m_catalogStars[i];
            double ra_diff = abs(existing.ra - brightStar.ra);
            double dec_diff = abs(existing.dec - brightStar.dec);
            
            if (ra_diff < 0.01 && dec_diff < 0.01) { // Same star (within 0.01 degrees)
                // Replace if our magnitude is more reasonable (bright stars should have mag < 6)
                if (existing.magnitude > 6.0 && brightStar.magnitude < 6.0) {
                    qDebug() << QString("  🔄 Replacing %1: bad mag %.2f -> correct mag %.2f")
                                .arg(brightStar.name).arg(existing.magnitude).arg(brightStar.magnitude);
                    m_catalogStars[i] = catalogStar;
                    replaced = true;
                    replacedCount++;
                    break;
                }
            }
        }
        
        if (!replaced) {
            m_catalogStars.append(catalogStar);
            addedCount++;
            
            qDebug() << QString("  ✅ Added %1 (%2): mag %.2f -> pixel (%.0f, %.0f) %3")
                        .arg(brightStar.name).arg(brightStar.constellation).arg(brightStar.magnitude)
                        .arg(catalogStar.pixelPos.x()).arg(catalogStar.pixelPos.y())
                        .arg(catalogStar.isValid ? "✅" : "❌");
        }
    }
    
    qDebug() << QString("Added %1 new bright stars, replaced %2 existing entries")
                .arg(addedCount).arg(replacedCount);
    
    if (addedCount > 0 || replacedCount > 0) {
        // Sort catalog by magnitude (brightest first)
        std::sort(m_catalogStars.begin(), m_catalogStars.end(), 
                  [](const CatalogStar& a, const CatalogStar& b) {
                      return a.magnitude < b.magnitude;
                  });
        
        qDebug() << QString("Total catalog now contains %1 stars").arg(m_catalogStars.size());
    }
}

// Update StarCatalogValidator to use local 2MASS catalog
void StarCatalogValidator::initializeLocal2MASS()
{
    // Set the path to your 2MASS catalog
    QString catalogPath = "/Volumes/X10Pro/allsky_2mass/allsky.mag";
    Local2MASSCatalog::setCatalogPath(catalogPath);
    
    if (Local2MASSCatalog::isAvailable()) {
        qDebug() << "✅" << Local2MASSCatalog::getCatalogInfo();
    } else {
        qDebug() << "❌ 2MASS catalog not found at:" << catalogPath;
        qDebug() << "   Network catalogs will be used as fallback";
    }
}

void StarCatalogValidator::queryLocal2MASS(double centerRA, double centerDec, double radiusDegrees)
{
    qDebug() << "\n=== QUERYING LOCAL 2MASS CATALOG ===";
    
    if (!Local2MASSCatalog::isAvailable()) {
        qDebug() << "❌ Local 2MASS not available, falling back to network";
        // Fall back to original network query
        queryCatalog(centerRA, centerDec, radiusDegrees);
        return;
    }
    
    emit catalogQueryStarted();
    
    // Query local catalog
    auto stars2mass = Local2MASSCatalog::queryRegion(centerRA, centerDec, radiusDegrees, m_magnitudeLimit);
    
    // Convert to our CatalogStar format
    m_catalogStars.clear();
    for (const auto& star : stars2mass) {
        CatalogStar catalogStar(star.id, star.ra, star.dec, star.magnitude);
        
        // Calculate pixel position
        catalogStar.pixelPos = skyToPixel(star.ra, star.dec);
        
        // Check if in image bounds
        catalogStar.isValid = (catalogStar.pixelPos.x() >= 0 && catalogStar.pixelPos.x() < m_wcsData.width && 
                              catalogStar.pixelPos.y() >= 0 && catalogStar.pixelPos.y() < m_wcsData.height);
        
        m_catalogStars.append(catalogStar);
    }
    
    // Add bright stars from local database (for stars like Betelgeuse that might be missing/wrong in 2MASS)
    addBrightStarsFromDatabase(centerRA, centerDec, radiusDegrees);
    
    QString message = QString("Retrieved %1 stars from local 2MASS catalog").arg(m_catalogStars.size());
    qDebug() << "✅" << message;
    
    emit catalogQueryFinished(true, message);
}

// Add a method to show catalog statistics in the UI
void StarCatalogValidator::showCatalogStats() const
{
    if (m_catalogStars.isEmpty()) {
        qDebug() << "No catalog stars loaded";
        return;
    }
    
    // Magnitude distribution
    QMap<int, int> magHistogram;
    int validStars = 0;
    double brightestMag = 99.0;
    double faintestMag = -99.0;
    
    for (const auto& star : m_catalogStars) {
        if (star.isValid) validStars++;
        
        int magBin = static_cast<int>(std::floor(star.magnitude));
        magHistogram[magBin]++;
        
        brightestMag = std::min(brightestMag, star.magnitude);
        faintestMag = std::max(faintestMag, star.magnitude);
    }
    
    qDebug() << "\n=== CATALOG STATISTICS ===";
    qDebug() << QString("Total stars: %1").arg(m_catalogStars.size());
    qDebug() << QString("Stars in image bounds: %1").arg(validStars);
    qDebug() << QString("Magnitude range: %.1f to %.1f").arg(brightestMag).arg(faintestMag);
    qDebug() << "Magnitude distribution:";
    
    for (auto it = magHistogram.begin(); it != magHistogram.end(); ++it) {
        qDebug() << QString("  mag %1-%2: %3 stars")
                    .arg(it.key()).arg(it.key() + 1).arg(it.value());
    }
}
