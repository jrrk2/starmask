#ifndef STARCORRELATOR_H
#define STARCORRELATOR_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QtMath>
#include <QRegularExpression>

struct DetectedStar {
    int id;
    double x, y;
    double flux;
    double area;
    double radius;
    double snr;
    bool matched = false;
    
    DetectedStar() = default;
    DetectedStar(int id, double x, double y, double flux, double area, double radius, double snr)
        : id(id), x(x), y(y), flux(flux), area(area), radius(radius), snr(snr) {}
};

struct CorrCatalogStar {
    QString gaia_id;
    double x, y;
    double magnitude;
    bool matched = false;
    
    CorrCatalogStar() = default;
    CorrCatalogStar(const QString& id, double x, double y, double mag)
        : gaia_id(id), x(x), y(y), magnitude(mag) {}
};

struct CorrStarMatch {
    int detected_id;
    QString catalog_id;
    double distance;
    double detected_x, detected_y;
    double catalog_x, catalog_y;
    double magnitude;
    double flux;
    double predicted_flux;
    double flux_ratio;
    double mag_diff;
};

class StarCorrelator : public QObject
{
  //    Q_OBJECT

private:
    QVector<DetectedStar> m_detectedStars;
    QVector<CorrCatalogStar> m_catalogStars;
    QVector<CorrStarMatch> m_matches;
    double m_matchThreshold = 5.0;
    double m_imageWidth = 3056.0;
    double m_imageHeight = 2048.0;
    double m_zeroPoint = 25.0;
    bool m_autoCalibrate = true;

public:
    explicit StarCorrelator(QObject *parent = nullptr);
    
    // Configuration
    void setMatchThreshold(double threshold) { m_matchThreshold = threshold; }
    void setImageDimensions(double width, double height) { m_imageWidth = width; m_imageHeight = height; }
    void setZeroPoint(double zp) { m_zeroPoint = zp; }
    void setAutoCalibrate(bool auto_cal) { m_autoCalibrate = auto_cal; }
    
    // Data loading
    void loadDetectedStarsFromLog(const QString& filename);
    void loadCatalogStarsFromLog(const QString& filename);
    void addDetectedStar(int id, double x, double y, double flux, double area, double radius, double snr);
    void addCatalogStar(const QString& gaia_id, double x, double y, double magnitude);
    
    // Analysis methods
    void correlateStars();
    void calibrateZeroPoint();
    void updateMatchFluxData();
    
    // Utility functions
    double magnitudeToFlux(double magnitude) const;
    double fluxToMagnitude(double flux) const;
    
    // Reporting methods
    void printDetailedStatistics() const;
    void printMatchDetails() const;
    void analyzePhotometricAccuracy() const;
    void analyzeMissingStars() const;
    void generateRecommendations() const;
    void exportMatches(const QString& filename) const;
    
    // Getters for integration with your existing code
    const QVector<CorrStarMatch>& getMatches() const { return m_matches; }
    int getMatchCount() const { return m_matches.size(); }
    double getAverageError() const;
    double getMatchRate() const;

private:
    // Parsing helpers
    void parseDetectedStar(const QString& line);
    void parseCatalogStar(const QString& line);
    bool parseStarData(const QString& line, double& x, double& y, double& flux, double& area, double& radius, double& snr);
    QString extractValue(const QString& line, int start) const;
};

#endif // STARCORRELATOR_H
