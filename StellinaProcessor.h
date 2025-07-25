// Stub file for StellinaProcessor.h
// Replace this with your actual header or remove if not needed

#ifndef STELLINA_PROCESSOR_H
#define STELLINA_PROCESSOR_H

#include <QString>

// Minimal stub for StellinaImageData
struct StellinaImageData {
    double altitude = 45.0;
    double azimuth = 180.0;
    int exposureSeconds = 30;
    int temperatureKelvin = 273;
    QString binning = "1x1";
    QString bayerPattern = "RGGB";
    bool hasCalculatedCoords = false;
    double calculatedRA = 0.0;
    double calculatedDec = 0.0;
};

#endif // STELLINA_PROCESSOR_H