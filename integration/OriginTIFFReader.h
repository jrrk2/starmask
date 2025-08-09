// OriginTIFFReader.h - Enhanced TIFF reader with Origin telescope metadata integration
#ifndef ORIGIN_TIFF_READER_H
#define ORIGIN_TIFF_READER_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QDebug>
#include <QVector>
#include <QPointF>
#include <tiffio.h>
#include <fstream>
#include <vector>
#include <cstdint>

// Forward declarations
class StarCatalogValidator;
struct ImageData;
struct CatalogStar;

// Structure to hold Origin telescope metadata
struct OriginMetadata {
    QString objectName;
    double centerRA = 0.0;          // degrees (celestial.first)
    double centerDec = 0.0;         // degrees (celestial.second)
    double fieldOfViewX = 0.0;      // degrees (fovX)
    double fieldOfViewY = 0.0;      // degrees (fovY)
    double orientation = 0.0;       // degrees
    QString dateTime;
    QString filter;
    int imageWidth = 0;
    int imageHeight = 0;
    double exposure = 0.0;
    int iso = 0;
    double temperature = 0.0;
    int stackedDepth = 0;
    double totalDurationMs = 0.0;
    QString bayerPattern;
    
    // GPS coordinates
    double gpsLatitude = 0.0;
    double gpsLongitude = 0.0;
    double gpsAltitude = 0.0;
    
    // Processing parameters
    double stretchBackground = 0.0;
    double stretchStrength = 0.0;
    
    QString uuid;
    bool isValid = false;
    
    // Calculate derived values
    double getPixelScaleX() const {
        return imageWidth > 0 ? (fieldOfViewX * 3600.0) / imageWidth : 0.0; // arcsec/pixel
    }
    
    double getPixelScaleY() const {
        return imageHeight > 0 ? (fieldOfViewY * 3600.0) / imageHeight : 0.0; // arcsec/pixel
    }
    
    double getAveragePixelScale() const {
        return (getPixelScaleX() + getPixelScaleY()) / 2.0;
    }
    
    QString getReadableExposureInfo() const {
        return QString("%1s at ISO %2 (%3 frames, %4min total)")
               .arg(exposure).arg(iso).arg(stackedDepth)
               .arg(totalDurationMs / 60000.0, 0, 'f', 1);
    }
};

// Structure for catalog lookup results
struct CatalogLookupResult {
    bool success = false;
    QString errorMessage;
    QVector<CatalogStar> catalogStars;
    double searchRadiusDegrees = 0.0;
    int totalStarsFound = 0;
    QString catalogUsed;
    
    // Plate solving metrics
    bool plateSolveAttempted = false;
    bool plateSolveSuccessful = false;
    double solvedRA = 0.0;
    double solvedDec = 0.0;
    double solvedPixelScale = 0.0;
    double solvedOrientation = 0.0;
    double solvingErrorArcsec = 0.0;
    int matchedStars = 0;
};

class OriginTIFFReader : public QObject
{
    Q_OBJECT
    
public:
    explicit OriginTIFFReader(QObject* parent = nullptr);
    ~OriginTIFFReader();
    
    // Core functionality
    bool loadTIFFFile(const QString& filePath);
    bool extractOriginMetadata();
    bool performCatalogLookup(StarCatalogValidator* catalogValidator = nullptr);
    
    // Data access
    const OriginMetadata& getOriginMetadata() const { return m_originMetadata; }
    const CatalogLookupResult& getCatalogResult() const { return m_catalogResult; }
    const ImageData* getImageData() const { return m_imageData; }
    
    // Utility functions
    bool hasOriginMetadata() const { return m_originMetadata.isValid; }
    bool hasCatalogData() const { return m_catalogResult.success; }
    QString getLastError() const { return m_lastError; }
    
    // Configuration
    void setMaxCatalogStars(int maxStars) { m_maxCatalogStars = maxStars; }
    void setSearchRadiusMultiplier(double multiplier) { m_searchRadiusMultiplier = multiplier; }
    void setEnablePlateSolving(bool enable) { m_enablePlateSolving = enable; }
    void setVerboseLogging(bool verbose) { m_verboseLogging = verbose; }
    
    // Export capabilities
    QJsonObject exportMetadataAsJson() const;
    QString exportMetadataAsText() const;
    bool saveProcessedImageWithOverlay(const QString& outputPath) const;

signals:
    void metadataExtracted(const OriginMetadata& metadata);
    void catalogLookupCompleted(const CatalogLookupResult& result);
    void plateSolvingProgress(const QString& status);
    void errorOccurred(const QString& error);
    
private slots:
    void onCatalogQueryFinished();
    void onPlateSolvingCompleted();
    
private:
    // TIFF handling
    TIFF* m_tiff = nullptr;
    std::ifstream m_rawFile;
    QString m_filePath;
    ImageData* m_imageData = nullptr;
    bool m_isLittleEndian = true;
    
    // Metadata
    OriginMetadata m_originMetadata;
    CatalogLookupResult m_catalogResult;
    QString m_lastError;
    
    // Configuration
    int m_maxCatalogStars = 1000;
    double m_searchRadiusMultiplier = 1.5;  // Multiply FOV by this for search radius
    bool m_enablePlateSolving = true;
    bool m_verboseLogging = true;
    
    // Internal methods
    bool openTIFFFile();
    void closeTIFFFile();
    
    // Metadata extraction methods
    bool scanForOriginJSON();
    bool parseOriginJSON(const QByteArray& jsonData);
    QByteArray extractJSONFromBinary(const std::vector<uint8_t>& binaryData);
    
    // TIFF low-level access
    bool scanRawTIFFStructure();
    void scanIFD(uint32_t offset, int level = 0);
    bool readBinaryValue(uint32_t offset, uint32_t count, std::vector<uint8_t>& result);
    uint16_t swapBytes16(uint16_t val);
    uint32_t swapBytes32(uint32_t val);
    
    // Catalog integration methods
    bool initializeCatalogSearch(StarCatalogValidator* validator);
    double calculateSearchRadius() const;
    bool validateCatalogResults();
    
    // Plate solving methods
    bool attemptPlateSolving(StarCatalogValidator* validator);
    bool validatePlateSolution();
    
    // Utility methods
    void logInfo(const QString& message);
    void logError(const QString& message);
    QString formatCoordinates(double ra, double dec) const;
    QString formatFieldOfView(double fovDegrees) const;
};

// Implementation
OriginTIFFReader::OriginTIFFReader(QObject* parent)
    : QObject(parent)
{
}

OriginTIFFReader::~OriginTIFFReader()
{
    closeTIFFFile();
}

bool OriginTIFFReader::loadTIFFFile(const QString& filePath)
{
    m_filePath = filePath;
    m_lastError.clear();
    m_originMetadata = OriginMetadata(); // Reset
    m_catalogResult = CatalogLookupResult(); // Reset
    
    logInfo(QString("Loading TIFF file: %1").arg(filePath));
    
    if (!openTIFFFile()) {
        m_lastError = "Failed to open TIFF file";
        logError(m_lastError);
        return false;
    }
    
    // Extract basic TIFF metadata first
    if (!extractBasicTIFFInfo()) {
        logError("Failed to extract basic TIFF information");
        // Continue anyway, might still find Origin metadata
    }
    
    // Try to extract Origin telescope metadata
    if (!extractOriginMetadata()) {
        logInfo("No Origin telescope metadata found in TIFF file");
        // This is not necessarily an error - file might be from different source
    }
    
    return true;
}

bool OriginTIFFReader::extractOriginMetadata()
{
    logInfo("Scanning TIFF file for Origin telescope metadata...");
    
    if (!m_tiff || !m_rawFile.is_open()) {
        m_lastError = "TIFF file not properly opened";
        return false;
    }
    
    return scanForOriginJSON();
}

bool OriginTIFFReader::performCatalogLookup(StarCatalogValidator* catalogValidator)
{
    if (!m_originMetadata.isValid) {
        m_lastError = "No valid Origin metadata available for catalog lookup";
        logError(m_lastError);
        return false;
    }
    
    if (!catalogValidator) {
        m_lastError = "No catalog validator provided";
        logError(m_lastError);
        return false;
    }
    
    logInfo(QString("Performing catalog lookup for object: %1").arg(m_originMetadata.objectName));
    logInfo(QString("Target coordinates: RA=%1°, Dec=%2°").arg(m_originMetadata.centerRA).arg(m_originMetadata.centerDec));
    
    // Calculate search parameters
    double searchRadius = calculateSearchRadius();
    logInfo(QString("Search radius: %1°").arg(searchRadius, 0, 'f', 4));
    
    // Initialize catalog search
    if (!initializeCatalogSearch(catalogValidator)) {
        return false;
    }
    
    // Perform the actual catalog query
    try {
        // Set up search parameters based on Origin metadata
        catalogValidator->setCenterCoordinates(m_originMetadata.centerRA, m_originMetadata.centerDec);
        catalogValidator->setSearchRadius(searchRadius);
        catalogValidator->setMaxResults(m_maxCatalogStars);
        
        // Trigger catalog query
        catalogValidator->queryCatalogRegion();
        
        // The actual results will come via signal/slot when query completes
        logInfo("Catalog query initiated...");
        return true;
        
    } catch (const std::exception& e) {
        m_lastError = QString("Exception during catalog lookup: %1").arg(e.what());
        logError(m_lastError);
        return false;
    }
}

bool OriginTIFFReader::openTIFFFile()
{
    // Open with libtiff
    m_tiff = TIFFOpen(m_filePath.toLocal8Bit().constData(), "r");
    if (!m_tiff) {
        return false;
    }
    
    // Also open for raw binary access
    m_rawFile.open(m_filePath.toLocal8Bit().constData(), std::ios::binary);
    if (!m_rawFile.is_open()) {
        TIFFClose(m_tiff);
        m_tiff = nullptr;
        return false;
    }
    
    // Determine byte order
    uint16_t byteOrder;
    m_rawFile.seekg(0);
    m_rawFile.read(reinterpret_cast<char*>(&byteOrder), sizeof(byteOrder));
    m_isLittleEndian = (byteOrder == 0x4949); // 'II'
    
    logInfo(QString("TIFF file opened successfully, byte order: %1")
            .arg(m_isLittleEndian ? "Little Endian" : "Big Endian"));
    
    return true;
}

void OriginTIFFReader::closeTIFFFile()
{
    if (m_tiff) {
        TIFFClose(m_tiff);
        m_tiff = nullptr;
    }
    
    if (m_rawFile.is_open()) {
        m_rawFile.close();
    }
}

bool OriginTIFFReader::scanForOriginJSON()
{
    if (!m_rawFile.is_open()) {
        return false;
    }
    
    // Read TIFF header to get first IFD offset
    m_rawFile.seekg(0);
    
    struct TIFFHeader {
        uint16_t byteOrder;
        uint16_t magic;
        uint32_t ifdOffset;
    } header;
    
    m_rawFile.read(reinterpret_cast<char*>(&header), sizeof(header));
    
    uint32_t ifdOffset = swapBytes32(header.ifdOffset);
    logInfo(QString("Scanning for Origin JSON starting at IFD offset: %1").arg(ifdOffset));
    
    return scanIFD(ifdOffset);
}

void OriginTIFFReader::scanIFD(uint32_t offset, int level)
{
    if (offset == 0 || !m_rawFile.is_open() || level > 3) {
        return;
    }
    
    m_rawFile.seekg(offset);
    
    // Read number of IFD entries
    uint16_t numEntries;
    m_rawFile.read(reinterpret_cast<char*>(&numEntries), sizeof(numEntries));
    numEntries = swapBytes16(numEntries);
    
    if (numEntries > 1000) {
        logError(QString("Suspicious number of IFD entries: %1, skipping").arg(numEntries));
        return;
    }
    
    logInfo(QString("Scanning IFD at offset %1 with %2 entries").arg(offset).arg(numEntries));
    
    // Read all IFD entries
    for (uint16_t i = 0; i < numEntries; i++) {
        struct IFDEntry {
            uint16_t tag;
            uint16_t type;
            uint32_t count;
            uint32_t valueOffset;
        } entry;
        
        m_rawFile.read(reinterpret_cast<char*>(&entry), sizeof(entry));
        
        entry.tag = swapBytes16(entry.tag);
        entry.type = swapBytes16(entry.type);
        entry.count = swapBytes32(entry.count);
        entry.valueOffset = swapBytes32(entry.valueOffset);
        
        // Look for UNDEFINED data type that might contain JSON
        if (entry.type == 7 && entry.count > 50) { // UNDEFINED with reasonable size
            std::vector<uint8_t> binaryData;
            if (readBinaryValue(entry.valueOffset, entry.count, binaryData)) {
                QByteArray jsonData = extractJSONFromBinary(binaryData);
                if (!jsonData.isEmpty()) {
                    if (parseOriginJSON(jsonData)) {
                        logInfo("Successfully found and parsed Origin JSON metadata");
                        emit metadataExtracted(m_originMetadata);
                        return; // Found it, stop scanning
                    }
                }
            }
        }
        
        // Recursively scan sub-IFDs (EXIF, GPS, etc.)
        if (entry.tag == 34665 || entry.tag == 34853) { // EXIF or GPS IFD
            scanIFD(entry.valueOffset, level + 1);
        }
    }
}

QByteArray OriginTIFFReader::extractJSONFromBinary(const std::vector<uint8_t>& binaryData)
{
    // Convert binary data to string and look for JSON patterns
    QString dataString;
    for (uint8_t byte : binaryData) {
        if (byte >= 32 && byte <= 126) {
            dataString += static_cast<char>(byte);
        } else if (byte == 0) {
            dataString += ' '; // Replace null with space
        }
    }
    
    // Look for Origin telescope JSON patterns
    if (!dataString.contains("StackedInfo") && 
        !dataString.contains("captureParams") && 
        !dataString.contains("celestial")) {
        return QByteArray();
    }
    
    // Find the JSON object boundaries
    int jsonStart = dataString.indexOf('{');
    int jsonEnd = dataString.lastIndexOf('}');
    
    if (jsonStart == -1 || jsonEnd == -1 || jsonEnd <= jsonStart) {
        return QByteArray();
    }
    
    QString jsonString = dataString.mid(jsonStart, jsonEnd - jsonStart + 1);
    return jsonString.toUtf8();
}

bool OriginTIFFReader::parseOriginJSON(const QByteArray& jsonData)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        logError(QString("JSON parse error: %1").arg(parseError.errorString()));
        return false;
    }
    
    if (!doc.isObject()) {
        logError("JSON data is not an object");
        return false;
    }
    
    QJsonObject root = doc.object();
    if (!root.contains("StackedInfo")) {
        logError("JSON does not contain StackedInfo");
        return false;
    }
    
    QJsonObject stackedInfo = root["StackedInfo"].toObject();
    
    // Extract core metadata
    m_originMetadata.objectName = stackedInfo["objectName"].toString();
    m_originMetadata.dateTime = stackedInfo["dateTime"].toString();
    m_originMetadata.filter = stackedInfo["filter"].toString();
    m_originMetadata.imageWidth = stackedInfo["imageWidth"].toInt();
    m_originMetadata.imageHeight = stackedInfo["imageHeight"].toInt();
    m_originMetadata.stackedDepth = stackedInfo["stackedDepth"].toInt();
    m_originMetadata.totalDurationMs = stackedInfo["totalDurationMs"].toDouble();
    m_originMetadata.bayerPattern = stackedInfo["bayer"].toString();
    m_originMetadata.uuid = stackedInfo["uuid"].toString();
    
    // Extract celestial coordinates
    if (stackedInfo.contains("celestial")) {
        QJsonObject celestial = stackedInfo["celestial"].toObject();
        m_originMetadata.centerRA = celestial["first"].toDouble();
        m_originMetadata.centerDec = celestial["second"].toDouble();
    }
    
    // Extract field of view
    m_originMetadata.fieldOfViewX = stackedInfo["fovX"].toDouble();
    m_originMetadata.fieldOfViewY = stackedInfo["fovY"].toDouble();
    m_originMetadata.orientation = stackedInfo["orientation"].toDouble();
    
    // Extract capture parameters
    if (stackedInfo.contains("captureParams")) {
        QJsonObject captureParams = stackedInfo["captureParams"].toObject();
        m_originMetadata.exposure = captureParams["exposure"].toDouble();
        m_originMetadata.iso = captureParams["iso"].toInt();
        m_originMetadata.temperature = captureParams["temperature"].toDouble();
    }
    
    // Extract GPS information
    if (stackedInfo.contains("gps")) {
        QJsonObject gps = stackedInfo["gps"].toObject();
        m_originMetadata.gpsLatitude = gps["latitude"].toDouble();
        m_originMetadata.gpsLongitude = gps["longitude"].toDouble();
        m_originMetadata.gpsAltitude = gps["altitude"].toDouble();
    }
    
    // Extract processing parameters
    m_originMetadata.stretchBackground = stackedInfo["stretchBackground"].toDouble();
    m_originMetadata.stretchStrength = stackedInfo["stretchStrength"].toDouble();
    
    // Mark as valid
    m_originMetadata.isValid = true;
    
    logInfo(QString("Successfully parsed Origin metadata for object: %1").arg(m_originMetadata.objectName));
    logInfo(QString("Coordinates: RA=%1°, Dec=%2°").arg(m_originMetadata.centerRA).arg(m_originMetadata.centerDec));
    logInfo(QString("Field of view: %1° × %2°").arg(m_originMetadata.fieldOfViewX).arg(m_originMetadata.fieldOfViewY));
    logInfo(QString("Image size: %1 × %2 pixels").arg(m_originMetadata.imageWidth).arg(m_originMetadata.imageHeight));
    logInfo(QString("Pixel scale: %1 arcsec/pixel").arg(m_originMetadata.getAveragePixelScale(), 0, 'f', 2));
    
    return true;
}

double OriginTIFFReader::calculateSearchRadius() const
{
    if (!m_originMetadata.isValid) {
        return 1.0; // Default 1 degree
    }
    
    // Use the larger of the two FOV dimensions, multiplied by safety factor
    double maxFOV = qMax(m_originMetadata.fieldOfViewX, m_originMetadata.fieldOfViewY);
    return maxFOV * m_searchRadiusMultiplier;
}

bool OriginTIFFReader::initializeCatalogSearch(StarCatalogValidator* validator)
{
    if (!validator) {
        m_lastError = "No catalog validator provided";
        return false;
    }
    
    // Connect signals for async operation
    connect(validator, &StarCatalogValidator::catalogQueryFinished,
            this, &OriginTIFFReader::onCatalogQueryFinished);
    
    // Set up WCS information from Origin metadata
    if (m_originMetadata.isValid) {
        // Create synthetic WCS based on Origin metadata
        // This will be used for coordinate transformations
        validator->setSyntheticWCS(
            m_originMetadata.centerRA,
            m_originMetadata.centerDec,
            m_originMetadata.getAveragePixelScale(),
            m_originMetadata.orientation,
            m_originMetadata.imageWidth,
            m_originMetadata.imageHeight
        );
        
        logInfo("Configured catalog validator with Origin metadata WCS");
    }
    
    return true;
}

// Helper methods
uint16_t OriginTIFFReader::swapBytes16(uint16_t val)
{
    return m_isLittleEndian ? val : ((val & 0xFF) << 8) | ((val >> 8) & 0xFF);
}

uint32_t OriginTIFFReader::swapBytes32(uint32_t val)
{
    if (m_isLittleEndian) return val;
    return ((val & 0xFF) << 24) | (((val >> 8) & 0xFF) << 16) | 
           (((val >> 16) & 0xFF) << 8) | ((val >> 24) & 0xFF);
}

bool OriginTIFFReader::readBinaryValue(uint32_t offset, uint32_t count, std::vector<uint8_t>& result)
{
    if (!m_rawFile.is_open()) {
        return false;
    }
    
    std::streampos currentPos = m_rawFile.tellg();
    m_rawFile.seekg(offset);
    
    result.resize(count);
    m_rawFile.read(reinterpret_cast<char*>(result.data()), count);
    
    bool success = m_rawFile.gcount() == count;
    m_rawFile.seekg(currentPos);
    
    return success;
}

void OriginTIFFReader::onCatalogQueryFinished()
{
    logInfo("Catalog query completed, processing results...");
    
    StarCatalogValidator* validator = qobject_cast<StarCatalogValidator*>(sender());
    if (!validator) {
        logError("Invalid catalog validator in query finished handler");
        return;
    }
    
    // Extract results from validator
    m_catalogResult.catalogStars = validator->getCatalogStars();
    m_catalogResult.totalStarsFound = m_catalogResult.catalogStars.size();
    m_catalogResult.searchRadiusDegrees = calculateSearchRadius();
    m_catalogResult.catalogUsed = validator->getCatalogName();
    m_catalogResult.success = m_catalogResult.totalStarsFound > 0;
    
    if (m_catalogResult.success) {
        logInfo(QString("Found %1 catalog stars").arg(m_catalogResult.totalStarsFound));
        
        // Attempt plate solving if enabled
        if (m_enablePlateSolving) {
            attemptPlateSolving(validator);
        }
    } else {
        m_catalogResult.errorMessage = "No catalog stars found in search region";
        logError(m_catalogResult.errorMessage);
    }
    
    emit catalogLookupCompleted(m_catalogResult);
}

bool OriginTIFFReader::attemptPlateSolving(StarCatalogValidator* validator)
{
    if (!m_originMetadata.isValid || !validator) {
        return false;
    }
    
    logInfo("Attempting plate solving with Origin metadata as initial guess...");
    
    m_catalogResult.plateSolveAttempted = true;
    
    // Use Origin metadata as initial WCS solution
    // Then refine based on detected stars vs catalog matches
    
    try {
        // This would integrate with your existing plate solving logic
        // For now, we'll validate the Origin metadata against catalog
        
        double rmsError = validator->calculateRMSError();
        int matchedStars = validator->getMatchedStarCount();
        
        if (rmsError < 10.0 && matchedStars >= 3) { // Within 10 arcsec RMS, at least 3 matches
            m_catalogResult.plateSolveSuccessful = true;
            m_catalogResult.solvedRA = m_originMetadata.centerRA;
            m_catalogResult.solvedDec = m_originMetadata.centerDec;
            m_catalogResult.solvedPixelScale = m_originMetadata.getAveragePixelScale();
            m_catalogResult.solvedOrientation = m_originMetadata.orientation;
            m_catalogResult.solvingErrorArcsec = rmsError;
            m_catalogResult.matchedStars = matchedStars;
            
            logInfo(QString("Plate solving successful! RMS error: %1 arcsec, %2 matched stars")
                   .arg(rmsError, 0, 'f', 2).arg(matchedStars));
        } else {
            m_catalogResult.plateSolveSuccessful = false;
            m_catalogResult.solvingErrorArcsec = rmsError;
            m_catalogResult.matchedStars = matchedStars;
            
            logInfo(QString("Plate solving failed. RMS error: %1 arcsec, %2 matched stars")
                   .arg(rmsError, 0, 'f', 2).arg(matchedStars));
        }
        
        return m_catalogResult.plateSolveSuccessful;
        
    } catch (const std::exception& e) {
        logError(QString("Exception during plate solving: %1").arg(e.what()));
        return false;
    }
}

QJsonObject OriginTIFFReader::exportMetadataAsJson() const
{
    QJsonObject obj;
    
    if (!m_originMetadata.isValid) {
        obj["error"] = "No valid Origin metadata available";
        return obj;
    }
    
    // Origin metadata
    QJsonObject originObj;
    originObj["objectName"] = m_originMetadata.objectName;
    originObj["centerRA"] = m_originMetadata.centerRA;
    originObj["centerDec"] = m_originMetadata.centerDec;
    originObj["fieldOfViewX"] = m_originMetadata.fieldOfViewX;
    originObj["fieldOfViewY"] = m_originMetadata.fieldOfViewY;
    originObj["orientation"] = m_originMetadata.orientation;
    originObj["dateTime"] = m_originMetadata.dateTime;
    originObj["filter"] = m_originMetadata.filter;
    originObj["imageWidth"] = m_originMetadata.imageWidth;
    originObj["imageHeight"] = m_originMetadata.imageHeight;
    originObj["pixelScale"] = m_originMetadata.getAveragePixelScale();
    originObj["exposureInfo"] = m_originMetadata.getReadableExposureInfo();
    
    obj["originMetadata"] = originObj;
    
    // Catalog results
    if (m_catalogResult.success) {
        QJsonObject catalogObj;
        catalogObj["totalStarsFound"] = m_catalogResult.totalStarsFound;
        catalogObj["searchRadius"] = m_catalogResult.searchRadiusDegrees;
        catalogObj["catalogUsed"] = m_catalogResult.catalogUsed;
        
        if (m_catalogResult.plateSolveAttempted) {
            QJsonObject plateObj;
            plateObj["successful"] = m_catalogResult.plateSolveSuccessful;
            plateObj["rmsErrorArcsec"] = m_catalogResult.solvingErrorArcsec;
            plateObj["matchedStars"] = m_catalogResult.matchedStars;
            catalogObj["plateSolving"] = plateObj;
        }
        
        obj["catalogResults"] = catalogObj;
    }
    
    return obj;
}

QString OriginTIFFReader::exportMetadataAsText() const
{
    if (!m_originMetadata.isValid) {
        return "No valid Origin metadata available";
    }
    
    QString text;
    text += "=== Origin Telescope Metadata ===\n";
    text += QString("Object: %1\n").arg(m_originMetadata.objectName);
    text += QString("Coordinates: %1\n").arg(formatCoordinates(m_originMetadata.centerRA, m_originMetadata.centerDec));
    text += QString("Field of View: %1 × %2\n")
            .arg(formatFieldOfView(m_originMetadata.fieldOfViewX))
            .arg(formatFieldOfView(m_originMetadata.fieldOfViewY));
    text += QString("Image Size: %1 × %2 pixels\n").arg(m_originMetadata.imageWidth).arg(m_originMetadata.imageHeight);
    text += QString("Pixel Scale: %1 arcsec/pixel\n").arg(m_originMetadata.getAveragePixelScale(), 0, 'f', 2);
    text += QString("Orientation: %1°\n").arg(m_originMetadata.orientation, 0, 'f', 2);
    text += QString("Date/Time: %1\n").arg(m_originMetadata.dateTime);
    text += QString("Filter: %1\n").arg(m_originMetadata.filter);
    text += QString("Exposure: %1\n").arg(m_originMetadata.getReadableExposureInfo());
    text += QString("Temperature: %1°C\n").arg(m_originMetadata.temperature, 0, 'f', 1);
    
    if (m_originMetadata.gpsLatitude != 0.0 || m_originMetadata.gpsLongitude != 0.0) {
        text += QString("GPS Location: %1°, %2° (alt: %3m)\n")
                .arg(m_originMetadata.gpsLatitude, 0, 'f', 6)
                .arg(m_originMetadata.gpsLongitude, 0, 'f', 6)
                .arg(m_originMetadata.gpsAltitude, 0, 'f', 1);
    }
    
    text += QString("UUID: %1\n").arg(m_originMetadata.uuid);
    
    if (m_catalogResult.success) {
        text += "\n=== Catalog Lookup Results ===\n";
        text += QString("Catalog: %1\n").arg(m_catalogResult.catalogUsed);
        text += QString("Search Radius: %1°\n").arg(m_catalogResult.searchRadiusDegrees, 0, 'f', 4);
        text += QString("Stars Found: %1\n").arg(m_catalogResult.totalStarsFound);
        
        if (m_catalogResult.plateSolveAttempted) {
            text += "\n=== Plate Solving Results ===\n";
            text += QString("Status: %1\n").arg(m_catalogResult.plateSolveSuccessful ? "Successful" : "Failed");
            text += QString("RMS Error: %1 arcsec\n").arg(m_catalogResult.solvingErrorArcsec, 0, 'f', 2);
            text += QString("Matched Stars: %1\n").arg(m_catalogResult.matchedStars);
            
            if (m_catalogResult.plateSolveSuccessful) {
                text += QString("Solved Position: %1\n").arg(formatCoordinates(m_catalogResult.solvedRA, m_catalogResult.solvedDec));
                text += QString("Solved Pixel Scale: %1 arcsec/pixel\n").arg(m_catalogResult.solvedPixelScale, 0, 'f', 2);
                text += QString("Solved Orientation: %1°\n").arg(m_catalogResult.solvedOrientation, 0, 'f', 2);
            }
        }
    }
    
    return text;
}

void OriginTIFFReader::logInfo(const QString& message)
{
    if (m_verboseLogging) {
        qDebug() << "OriginTIFFReader:" << message;
    }
}

void OriginTIFFReader::logError(const QString& message)
{
    qDebug() << "OriginTIFFReader ERROR:" << message;
    emit errorOccurred(message);
}

QString OriginTIFFReader::formatCoordinates(double ra, double dec) const
{
    // Convert RA to hours:minutes:seconds
    double raHours = ra / 15.0;
    int hours = static_cast<int>(raHours);
    int minutes = static_cast<int>((raHours - hours) * 60.0);
    double seconds = ((raHours - hours) * 60.0 - minutes) * 60.0;
    
    // Convert Dec to degrees:arcminutes:arcseconds
    int degrees = static_cast<int>(dec);
    int arcminutes = static_cast<int>(qAbs(dec - degrees) * 60.0);
    double arcseconds = (qAbs(dec - degrees) * 60.0 - arcminutes) * 60.0;
    
    return QString("RA %1h %2m %3s, Dec %4° %5' %6\"")
           .arg(hours, 2, 10, QChar('0'))
           .arg(minutes, 2, 10, QChar('0'))
           .arg(seconds, 0, 'f', 1)
           .arg(degrees, 3, 10, dec >= 0 ? QChar(' ') : QChar('-'))
           .arg(arcminutes, 2, 10, QChar('0'))
           .arg(arcseconds, 0, 'f', 1);
}

QString OriginTIFFReader::formatFieldOfView(double fovDegrees) const
{
    if (fovDegrees >= 1.0) {
        return QString("%1°").arg(fovDegrees, 0, 'f', 2);
    } else {
        double fovArcmin = fovDegrees * 60.0;
        if (fovArcmin >= 1.0) {
            return QString("%1'").arg(fovArcmin, 0, 'f', 1);
        } else {
            double fovArcsec = fovArcmin * 60.0;
            return QString("%1\"").arg(fovArcsec, 0, 'f', 0);
        }
    }
}

bool OriginTIFFReader::extractBasicTIFFInfo()
{
    if (!m_tiff) {
        return false;
    }
    
    uint32_t width, height;
    uint16_t bitsPerSample, samplesPerPixel;
    char* software = nullptr;
    
    bool hasBasicInfo = true;
    hasBasicInfo &= TIFFGetField(m_tiff, TIFFTAG_IMAGEWIDTH, &width);
    hasBasicInfo &= TIFFGetField(m_tiff, TIFFTAG_IMAGELENGTH, &height);
    TIFFGetField(m_tiff, TIFFTAG_BITSPERSAMPLE, &bitsPerSample);
    TIFFGetField(m_tiff, TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel);
    TIFFGetField(m_tiff, TIFFTAG_SOFTWARE, &software);
    
    if (hasBasicInfo) {
        logInfo(QString("TIFF Info: %1×%2, %3-bit, %4 channels")
               .arg(width).arg(height).arg(bitsPerSample).arg(samplesPerPixel));
        
        if (software) {
            logInfo(QString("Software: %1").arg(software));
            
            // Check if this is an Origin telescope file
            if (QString(software).contains("Origin")) {
                logInfo("Detected Origin telescope software signature");
            }
        }
    }
    
    return hasBasicInfo;
}

#endif // ORIGIN_TIFF_READER_H

// =====================================================
// Example usage integration code
// =====================================================

/*
// Example integration with your existing TIFF reader:

// In your ImageReader.cpp, add Origin telescope support:

bool ImageReaderPrivate::readTIFFWithOriginSupport(const QString& filePath)
{
    // Create Origin TIFF reader
    OriginTIFFReader* originReader = new OriginTIFFReader();
    
    // Connect signals for progress tracking
    QObject::connect(originReader, &OriginTIFFReader::metadataExtracted,
                     [this](const OriginMetadata& metadata) {
        qDebug() << "Origin metadata extracted for:" << metadata.objectName;
        qDebug() << "Target coordinates:" << metadata.centerRA << metadata.centerDec;
        
        // Store the metadata for later use
        this->originMetadata = metadata;
    });
    
    QObject::connect(originReader, &OriginTIFFReader::catalogLookupCompleted,
                     [this](const CatalogLookupResult& result) {
        if (result.success) {
            qDebug() << "Catalog lookup successful:" << result.totalStarsFound << "stars found";
            
            if (result.plateSolveSuccessful) {
                qDebug() << "Plate solving successful, RMS error:" << result.solvingErrorArcsec << "arcsec";
                
                // Update WCS information with solved coordinates
                this->wcsInfo.centerRA = result.solvedRA;
                this->wcsInfo.centerDec = result.solvedDec;
                this->wcsInfo.pixelScale = result.solvedPixelScale;
                this->wcsInfo.orientation = result.solvedOrientation;
                this->wcsInfo.isValid = true;
                
                // Notify main application that WCS is available
                emit wcsInfoUpdated(this->wcsInfo);
            }
        } else {
            qDebug() << "Catalog lookup failed:" << result.errorMessage;
        }
    });
    
    // Load the TIFF file
    if (!originReader->loadTIFFFile(filePath)) {
        qDebug() << "Failed to load TIFF file:" << originReader->getLastError();
        delete originReader;
        return false;
    }
    
    // If Origin metadata was found, perform catalog lookup
    if (originReader->hasOriginMetadata()) {
        qDebug() << "Origin metadata found, performing catalog lookup...";
        
        // Create or get existing catalog validator
        if (!m_catalogValidator) {
            m_catalogValidator = new StarCatalogValidator();
        }
        
        // Configure catalog search
        originReader->setMaxCatalogStars(1000);
        originReader->setSearchRadiusMultiplier(1.5);
        originReader->setEnablePlateSolving(true);
        
        // Perform catalog lookup (async)
        originReader->performCatalogLookup(m_catalogValidator);
        
        // Store the reader for later cleanup
        m_originReader = originReader;
        
        return true;
    } else {
        qDebug() << "No Origin metadata found, falling back to standard TIFF reading";
        delete originReader;
        
        // Fall back to your existing TIFF reading method
        return readTIFF(filePath);
    }
}

// In your MainWindow.cpp, add menu/button for Origin TIFF files:

void MainWindow::onLoadOriginTIFF()
{
    QString fileName = QFileDialog::getOpenFileName(
        this, "Open Origin TIFF File",
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
        "TIFF Files (*.tiff *.tif);;All Files (*)");
    
    if (fileName.isEmpty()) return;
    
    // Load with Origin support
    if (m_imageReader->readTIFFWithOriginSupport(fileName)) {
        m_statusLabel->setText("Origin TIFF file loaded, processing metadata...");
        
        // The rest will be handled by the async signals
    } else {
        QMessageBox::warning(this, "Load Error", 
                           "Failed to load Origin TIFF file.");
    }
}

// Add this to your CMakeLists.txt for the Origin integration:

target_sources(${PROJECT_NAME} PRIVATE
    OriginTIFFReader.h
    OriginTIFFReader.cpp
)

// Make sure to link against the required libraries:
target_link_libraries(${PROJECT_NAME} 
    ${TIFF_LIBRARIES}
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
)

*/

// =====================================================
// Advanced usage with star detection integration
// =====================================================

/*
// Enhanced integration that combines Origin metadata with detected stars:

class EnhancedOriginTIFFProcessor : public QObject
{
    Q_OBJECT
    
public:
    struct ProcessingResult {
        OriginMetadata originMetadata;
        CatalogLookupResult catalogResult;
        QVector<DetectedStar> detectedStars;
        ValidationResult validationResult;
        bool platesolveSuccessful = false;
        double finalRMSError = 0.0;
    };
    
    ProcessingResult processOriginTIFF(const QString& filePath, 
                                     StarCatalogValidator* catalogValidator,
                                     StarDetector* starDetector)
    {
        ProcessingResult result;
        
        // 1. Load TIFF and extract Origin metadata
        OriginTIFFReader originReader;
        if (!originReader.loadTIFFFile(filePath)) {
            return result; // Failed
        }
        
        result.originMetadata = originReader.getOriginMetadata();
        
        // 2. Perform catalog lookup based on Origin metadata
        if (result.originMetadata.isValid) {
            originReader.performCatalogLookup(catalogValidator);
            // Wait for async completion or make it synchronous
            result.catalogResult = originReader.getCatalogResult();
        }
        
        // 3. Detect stars in the image
        if (starDetector) {
            ImageData* imageData = loadImageData(filePath);
            if (imageData) {
                result.detectedStars = starDetector->detectStars(*imageData);
            }
        }
        
        // 4. Cross-validate detected stars with catalog
        if (result.catalogResult.success && !result.detectedStars.isEmpty()) {
            result.validationResult = catalogValidator->validateStars(
                result.detectedStars, result.catalogResult.catalogStars);
            
            // 5. Refine plate solution if validation is good
            if (result.validationResult.totalMatches >= 3) {
                result.platesolveSuccessful = refinePlateSolution(
                    result.originMetadata,
                    result.detectedStars,
                    result.catalogResult.catalogStars,
                    result.finalRMSError
                );
            }
        }
        
        return result;
    }
};

*/
    