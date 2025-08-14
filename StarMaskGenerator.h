#ifndef STAR_MASK_GENERATOR_H
#define STAR_MASK_GENERATOR_H

#include <QVector>
#include <QPoint>
#include <QImage>
#include "ImageReader.h"
#include "StarCatalogValidator.h"

struct StarMaskResult {
    QVector<QPoint> starCenters;
    QVector<float> starRadii;
    QVector<float> starFluxes;    // Add this missing member
    QVector<bool> starValid;
    QImage maskImage;
};

class StarMaskGenerator
{
public:
    // Main star detection method using PCL StarDetector
    static StarMaskResult detectStars(const ImageData& imageData, float threshold = 0.5f);
    
    // Advanced star detection with custom parameters
    static StarMaskResult detectStarsAdvanced(const ImageData& imageData, 
                                             float sensitivity = 0.5f,
                                             int structureLayers = 5,
                                             int noiseLayers = 1,
                                             float peakResponse = 0.5f,
                                             float maxDistortion = 0.8f,
                                             bool enablePSFFitting = true);
    static void dumpcat(QVector<CatalogStar> &catalogStars);
    static void validateStarDetection();

private:
    // Fallback simple detection method
    static StarMaskResult detectStarsSimple(const ImageData& imageData, float threshold = 0.5f);
};

#endif // STAR_MASK_GENERATOR_H
