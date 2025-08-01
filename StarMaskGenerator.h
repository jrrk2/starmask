#ifndef STAR_MASK_GENERATOR_H
#define STAR_MASK_GENERATOR_H

#include <QVector>
#include <QPoint>
#include <QImage>
#include "ImageReader.h"

struct StarMaskResult {
    QVector<QPoint> starCenters;
    QVector<float> starRadii;
    QVector<bool> starValid;
    QImage maskImage;
};

class StarMaskGenerator
{
public:
    static StarMaskResult detectStars(const ImageData& imageData, float threshold = 0.5f);
};

#endif // STAR_MASK_GENERATOR_H
