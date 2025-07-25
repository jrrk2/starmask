// Stub file for structuredefinitions.h  
// Replace this with your actual header or remove if not needed

#ifndef STRUCTURE_DEFINITIONS_H
#define STRUCTURE_DEFINITIONS_H

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