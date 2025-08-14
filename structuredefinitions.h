// Stub file for structuredefinitions.h  
// Replace this with your actual header or remove if not needed

#ifndef STRUCTURE_DEFINITIONS_H
#define STRUCTURE_DEFINITIONS_H

#include "StarCatalogValidator.h"

extern "C" {
#include "astrometry/sip.h"
}

// Plate solve result structure  
struct PlatesolveResult {
    // Core data
    bool solved = false;
    QString errorMessage;
    QString indexUsed;
    
    // Position and scale (extracted from tan_t)
    double ra_center = 0.0;      // Center RA (degrees)
    double dec_center = 0.0;     // Center Dec (degrees)
    double pixscale = 1.0;       // Pixel scale (arcsec/pixel)
    double orientation = 0.0;    // Position angle (degrees)
    double fieldWidth = 0.0;     // Field width (arcmin)
    double fieldHeight = 0.0;    // Field height (arcmin)
    
    // CD matrix for precise transformations
    double cd11 = 0.0, cd12 = 0.0;
    double cd21 = 0.0, cd22 = 0.0;
    
    // Reference pixel (typically image center)
    double crpix1 = 0.0, crpix2 = 0.0;
    
    // Image parity (normal vs flipped)
    bool parity_positive = true;
    
    // Solution quality metrics
    double ra_error = 0.0;       // RA error (arcsec)
    double dec_error = 0.0;      // Dec error (arcsec)
    int matched_stars = 0;       // Number of matched catalog stars
    double solve_time = 0.0;     // Time to solve (seconds)
    
    // Internal WCS structure (from astrometry.net)
    tan_t wcs;
    
    // Convert to WCSData for your existing system
    WCSData toWCSData(int imageWidth = 0, int imageHeight = 0) const;
};

namespace FITSImage {
    enum Parity {
        POSITIVE = 1,
        NEGATIVE = -1
    };

    // Minimal stub for FITS Solution
    struct Solution {
        double ra = 180.0;          // Right Ascension (degrees)
        double dec = 45.0;          // Declination (degrees)
        double pixscale = 1.0;      // Pixel scale (arcsec/pixel)
        double orientation = 0.0;   // Position angle (degrees)
        double fieldWidth = 60.0;   // Field width (arcmin)
        double fieldHeight = 40.0;  // Field height (arcmin)
        Parity parity = POSITIVE;   // Image parity
        double raError = 0.0;       // RA error (arcsec)
        double decError = 0.0;      // Dec error (arcsec)
    };
}

#endif // STRUCTURE_DEFINITIONS_H
