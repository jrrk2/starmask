#ifndef STAR_CATALOG_VALIDATOR_H
#define STAR_CATALOG_VALIDATOR_H

#include <QObject>
#include <QVector>
#include <QPoint>
#include <QPointF>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <memory>

#include <pcl/WCSKeywords.h>
#include <pcl/LinearTransformation.h>
#include <pcl/AstrometricMetadata.h>
#include <curl/curl.h>
#include "ImageReader.h"

struct WCSData {
    double crval1 = 0.0;    // Reference RA (degrees)
    double crval2 = 0.0;    // Reference Dec (degrees)
    double crpix1 = 0.0;    // Reference pixel X
    double crpix2 = 0.0;    // Reference pixel Y
    double cd11 = 0.0;      // CD matrix elements
    double cd12 = 0.0;
    double cd21 = 0.0;
    double cd22 = 0.0;
    double pixscale = 0.0;  // Pixel scale (arcsec/pixel)
    double orientation = 0.0; // Position angle (degrees)
    int width = 0;          // Image width
    int height = 0;         // Image height
    bool isValid = false;
    
    // Alternative: CDELT/PC matrix format (less common but supported)
    double cdelt1 = 0.0;
    double cdelt2 = 0.0;
    double pc11 = 1.0;
    double pc12 = 0.0;
    double pc21 = 0.0;
    double pc22 = 1.0;
    
    void clear() {
        crval1 = crval2 = crpix1 = crpix2 = 0.0;
        cd11 = cd12 = cd21 = cd22 = 0.0;
        pixscale = orientation = 0.0;
        width = height = 0;
        isValid = false;
        cdelt1 = cdelt2 = 0.0;
        pc11 = pc22 = 1.0;
        pc12 = pc21 = 0.0;
    }
};

struct CatalogStar {
    QString id;             // Star identifier (e.g., HD123456, HIP67890)
    double ra = 0.0;        // Right ascension (degrees)
    double dec = 0.0;       // Declination (degrees)
    double magnitude = 0.0; // Visual magnitude
    QString spectralType;   // Spectral type (if available)
    QPointF pixelPos;       // Calculated pixel position
    bool isValid = false;
    
    CatalogStar() = default;
    CatalogStar(const QString& starId, double rightAsc, double declin, double mag)
        : id(starId), ra(rightAsc), dec(declin), magnitude(mag), isValid(true) {}
};

struct StarMatch {
    int detectedIndex = -1;     // Index in detected stars array
    int catalogIndex = -1;      // Index in catalog stars array
    double distance = 0.0;      // Distance in pixels
    double magnitudeDiff = 0.0; // Magnitude difference
    bool isGoodMatch = false;   // Whether this is considered a good match
    
    StarMatch() = default;
    StarMatch(int detIdx, int catIdx, double dist, double magDiff = 0.0)
        : detectedIndex(detIdx), catalogIndex(catIdx), distance(dist), 
          magnitudeDiff(magDiff), isGoodMatch(true) {}
};

struct ValidationResult {
    QVector<CatalogStar> catalogStars;
    QVector<StarMatch> matches;
    QVector<int> unmatchedDetected;   // Indices of unmatched detected stars
    QVector<int> unmatchedCatalog;    // Indices of unmatched catalog stars
    
    // Statistics
    int totalDetected = 0;
    int totalCatalog = 0;
    int totalMatches = 0;
    double matchPercentage = 0.0;
    double averagePositionError = 0.0;
    double rmsPositionError = 0.0;
    QString summary;
    bool isValid = false;
    
    void clear() {
        catalogStars.clear();
        matches.clear();
        unmatchedDetected.clear();
        unmatchedCatalog.clear();
        totalDetected = totalCatalog = totalMatches = 0;
        matchPercentage = averagePositionError = rmsPositionError = 0.0;
        summary.clear();
        isValid = false;
    }
};

class StarCatalogValidator : public QObject
{
    Q_OBJECT
    
public:
    enum CatalogSource {
        Hipparcos,      // Bright stars (mag < 12)
        Tycho2,         // Fainter stars (mag < 12)
        Gaia,           // Very comprehensive (millions of stars)
        Custom          // User-provided catalog
    };
    
    enum ValidationMode {
        Strict,         // Require very close matches
        Normal,         // Standard matching tolerance
        Loose          // More permissive matching
    };
    
    explicit StarCatalogValidator(QObject* parent = nullptr);
    ~StarCatalogValidator();
    
    // Configuration
    void setCatalogSource(CatalogSource source);
    void setValidationMode(ValidationMode mode);
    void setMatchingTolerance(double pixelTolerance, double magnitudeTolerance = 2.0);
    void setMagnitudeLimit(double faintestMagnitude);
    
    // WCS handling
    bool setWCSData(const WCSData& wcs);
    void setWCSFromMetadata(const QStringList& metadata);
    WCSData getWCSData() const;
    bool hasValidWCS() const;
    
    // Coordinate transformations
    bool setWCSFromPCLKeywords(const pcl::FITSKeywordArray& keywords);
    bool setWCSFromImageMetadata(const ImageData& imageData);  // NEW: Direct from ImageData
    
    // PCL-based coordinate transformations
    QPointF skyToPixel(double ra, double dec) const;
    QPointF pixelToSky(double x, double y) const;
    
    // Catalog operations
    void queryCatalog(double centerRA, double centerDec, double radiusDegrees);
    void loadCustomCatalog(const QString& filePath);
    void loadCustomCatalog(const QVector<CatalogStar>& stars);
  //    void identifyBrightStars() const;
    void addBrightStarsFromDatabase(double centerRA, double centerDec, double radiusDegrees);
  
    // Validation
    ValidationResult validateStars(const QVector<QPoint>& detectedStars, 
                                   const QVector<float>& starRadii = QVector<float>());
    
    // Results access
    ValidationResult getLastValidation() const { return m_lastValidation; }
    QVector<CatalogStar> getCatalogStars() const { return m_catalogStars; }
    void showCatalogStats() const;
    
    // Utility
    QString getValidationSummary() const;
    QJsonObject exportValidationResults() const;
    void clearResults();
    
signals:
    void catalogQueryStarted();
    void catalogQueryFinished(bool success, const QString& message);
    void validationCompleted(const ValidationResult& result);
    void errorSignal(const QString& message);
    
private slots:
  //    void onCatalogQueryFinished();
    void onNetworkError(QNetworkReply::NetworkError error);
    
private:
    // Replace complex WCS handling with PCL's high-level class
    pcl::AstrometricMetadata m_astrometricMetadata;
    bool m_hasAstrometricData = false;
    
    // Helper methods
    void testAstrometricMetadata() const;
    // Configuration
    ValidationMode m_validationMode;
    double m_pixelTolerance;
    double m_magnitudeTolerance;
    double m_magnitudeLimit;
    
    // Data
    WCSData m_wcsData;
    QVector<CatalogStar> m_catalogStars;
    ValidationResult m_lastValidation;
    
    // Network
    CURL* m_curl;
    QByteArray m_catData;
    
    // Internal methods
    void initializeTolerances();
    QString buildCatalogQuery(double centerRA, double centerDec, double radiusDegrees);
    void parseCatalogResponse(const QByteArray& data);
    void parseHipparcosData(const QJsonArray& stars);
    void parseGaiaData(const QJsonArray& stars);
    double parseCoordinate(const QString& coordStr, bool isRA) const;
    
    ValidationResult performMatching(const QVector<QPoint>& detectedStars, 
                                     const QVector<float>& starRadii);
    double calculateDistance(const QPoint& detected, const QPointF& catalog) const;
    bool isGoodMatch(double distance, double magnitudeDiff) const;
    void calculateStatistics(ValidationResult& result) const;
    void queryGaiaCatalog(double centerRA, double centerDec, double radiusDegrees);
    void initializeLocal2MASS();
    void queryLocal2MASS(double centerRA, double centerDec, double radiusDegrees);
  
    // WCS transformation helpers
    void updateWCSMatrix();
    void testPCLWCS() const;
    bool m_wcsMatrixValid;
    double m_transformMatrix[4]; // 2x2 transformation matrix
    double m_det; // Determinant for inverse transformation
    // Use PCL's native WCS objects
};

#endif // STAR_CATALOG_VALIDATOR_H
