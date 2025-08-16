// RGBPhotometryAnalyzer.h
#ifndef RGBPHOTOMETRYANALYZER_H
#define RGBPHOTOMETRYANALYZER_H

#include <QObject>
#include <QVector>
#include <QPointF>
#include <QColor>
#include <QDebug>
#include "ImageReader.h" // Your existing ImageData structure
#include "StarCatalogValidator.h"

struct StarColorData {
    int starIndex;
    QPointF position;
    
    // Raw channel values (average within aperture)
    double redValue;
    double greenValue; 
    double blueValue;
    
    // Calculated color indices
    double bv_index;    // B-V color index
    double vr_index;    // V-R color index
    double gr_index;    // G-R color index (instrumental)
    
    // Catalog comparison data
    double catalogBV;
    double catalogVR;
    QString spectralType;
    double magnitude;
    
    // Analysis results
    double bv_difference;  // Observed - Catalog
    double colorError;     // RMS color error
    bool hasValidCatalogColor;
    
    StarColorData() : starIndex(-1), redValue(0), greenValue(0), blueValue(0),
                     bv_index(0), vr_index(0), gr_index(0), catalogBV(0), catalogVR(0),
                     magnitude(0), bv_difference(0), colorError(0), hasValidCatalogColor(false) {}
};

struct ColorCalibrationResult {
    // Color transformation matrix (3x3)
    double colorMatrix[3][3];
    
    // Linear color corrections
    double redScale, greenScale, blueScale;
    double redOffset, greenOffset, blueOffset;
    
    // Quality metrics
    double rmsColorError;
    double systematicBVError;
    double systematicVRError;
    int starsUsed;
    
    // Recommendations
    QString calibrationQuality;  // "Excellent", "Good", "Fair", "Poor"
    QStringList recommendations;
    
    ColorCalibrationResult() {
        // Initialize to identity matrix
        for(int i = 0; i < 3; i++) {
            for(int j = 0; j < 3; j++) {
                colorMatrix[i][j] = (i == j) ? 1.0 : 0.0;
            }
        }
        redScale = greenScale = blueScale = 1.0;
        redOffset = greenOffset = blueOffset = 0.0;
        rmsColorError = 0.0;
        systematicBVError = systematicVRError = 0.0;
        starsUsed = 0;
    }
};

class RGBPhotometryAnalyzer : public QObject
{
    Q_OBJECT
    
public:
    explicit RGBPhotometryAnalyzer(QObject *parent = nullptr);
    
    // Main analysis functions
    bool analyzeStarColors(const ImageData* imageData, 
                          const QVector<QPoint>& starCenters,
                          const QVector<float>& starRadii);
    
    bool setStarCatalogData(const QVector<CatalogStar>& catalogStars);
    
    ColorCalibrationResult calculateColorCalibration();
    
    // Configuration
    void setApertureRadius(double radius) { m_apertureRadius = radius; }
    void setBackgroundAnnulus(double inner, double outer) { 
        m_bgInnerRadius = inner; 
        m_bgOuterRadius = outer; 
    }
    void setColorIndexType(const QString& type) { m_colorIndexType = type; }
    
    // Results access
    QVector<StarColorData> getStarColorData() const { return m_starColors; }
    ColorCalibrationResult getLastCalibration() const { return m_lastCalibration; }
    
    // Utility functions
    static double spectralTypeToColorIndex(const QString& spectralType);
    static QColor colorIndexToRGB(double bv_index);
    
signals:
    void colorAnalysisCompleted(int starsAnalyzed);
    void calibrationCompleted(const ColorCalibrationResult& result);
    
private:
    // Core photometry functions
    bool extractPhotometry(const ImageData* imageData, 
                          const QPoint& center, 
                          double radius,
                          double& red, double& green, double& blue);
    
    bool calculateBackgroundLevel(const ImageData* imageData,
                                 const QPoint& center,
                                 double innerRadius, 
                                 double outerRadius,
                                 double& bgRed, double& bgGreen, double& bgBlue);
    
    void calculateColorIndices(StarColorData& star);
    bool matchWithCatalog(StarColorData& star, const QVector<CatalogStar>& catalog);
    
    // Calibration analysis
    void analyzeSystematicErrors();
    void generateRecommendations();
    
    // Member variables
    QVector<StarColorData> m_starColors;
    ColorCalibrationResult m_lastCalibration;
    
    // Analysis parameters
    double m_apertureRadius;
    double m_bgInnerRadius;
    double m_bgOuterRadius;
    QString m_colorIndexType;
    
    // Catalog data
    QVector<CatalogStar> m_catalogStars;
};

#endif // RGBPHOTOMETRYANALYZER_H
