// OriginMetadataExtractor.h - Standalone metadata extractor for Origin telescope TIFF files
// This is a pure C++ class that doesn't depend on Qt or PCL, avoiding conflicts
#ifndef ORIGIN_METADATA_EXTRACTOR_H
#define ORIGIN_METADATA_EXTRACTOR_H

#include <string>
#include <vector>
#include <cstdint>
#include <fstream>
#include <memory>

// Pure C++ structure for Origin metadata (no Qt dependencies)
struct OriginTelescopeMetadata {
    // Core astronomical data
    std::string objectName;
    double centerRA = 0.0;          // degrees (celestial.first)
    double centerDec = 0.0;         // degrees (celestial.second)
    double fieldOfViewX = 0.0;      // degrees (fovX)
    double fieldOfViewY = 0.0;      // degrees (fovY)
    double orientation = 0.0;       // degrees
    
    // Image properties
    int imageWidth = 0;
    int imageHeight = 0;
    std::string bayerPattern;
    
    // Capture settings
    double exposure = 0.0;          // seconds
    int iso = 0;
    double temperature = 0.0;       // celsius
    bool autoExposure = false;
    int binning = 1;
    
    // Stacking information
    int stackedDepth = 0;           // number of frames
    double totalDurationMs = 0.0;   // total exposure time
    
    // Processing parameters
    double stretchBackground = 0.0;
    double stretchStrength = 0.0;
    std::string filter;
    
    // Location and timing
    std::string dateTime;
    double gpsLatitude = 0.0;
    double gpsLongitude = 0.0;
    double gpsAltitude = 0.0;
    
    // Unique identifier
    std::string uuid;
    
    // Validation
    bool isValid = false;
    std::string extractionSource; // "TIFF_IFD", "EXIF", etc.
    
    // Calculated properties
    double getPixelScaleX() const {
        return imageWidth > 0 ? (fieldOfViewX * 3600.0 * 60.0) / imageWidth : 0.0; // arcsec/pixel
    }
    
    double getPixelScaleY() const {
        return imageHeight > 0 ? (fieldOfViewY * 3600.0 * 60.0) / imageHeight : 0.0; // arcsec/pixel
    }
    
    double getAveragePixelScale() const {
        return (getPixelScaleX() + getPixelScaleY()) / 2.0;
    }
    
    double getTotalExposureSeconds() const {
        return totalDurationMs / 1000.0;
    }
    
    double getExposurePerFrame() const {
        return stackedDepth > 0 ? getTotalExposureSeconds() / stackedDepth : exposure;
    }
};

// Standalone extractor class (no Qt/PCL dependencies)
class OriginMetadataExtractor {
public:
    OriginMetadataExtractor();
    ~OriginMetadataExtractor();
    
    // Main extraction method - call this after your existing TIFF loading
    bool extractFromTIFFFile(const std::string& filePath);
    
    // Alternative: extract from already-loaded TIFF data
    bool extractFromTIFFData(const uint8_t* tiffData, size_t dataSize);
    
    // Access results
    const OriginTelescopeMetadata& getMetadata() const { return m_metadata; }
    bool hasValidMetadata() const { return m_metadata.isValid; }
    
    // Error handling
    std::string getLastError() const { return m_lastError; }
    
    // Configuration
    void setVerboseLogging(bool verbose) { m_verboseLogging = verbose; }
    void setMaxSearchDepth(int depth) { m_maxIFDDepth = depth; }
    
    // Utility methods
    std::string formatCoordinates() const;
    std::string formatFieldOfView() const;
    std::string formatExposureInfo() const;
    std::string exportAsText() const;
    std::string exportAsJSON() const;
    
    // Static utility for quick check
    static bool containsOriginMetadata(const std::string& filePath);

private:
    // TIFF parsing structures
    #pragma pack(push, 1)
    struct TIFFHeader {
        uint16_t byteOrder;     // 'II' or 'MM'
        uint16_t magic;         // Always 42
        uint32_t ifdOffset;     // Offset to first IFD
    };
    
    struct IFDEntry {
        uint16_t tag;           // Tag identifier
        uint16_t type;          // Data type
        uint32_t count;         // Number of values
        uint32_t valueOffset;   // Value or offset to value
    };
    #pragma pack(pop)
    
    // Internal methods
    bool openTIFFFile(const std::string& filePath);
    bool parseTIFFData(const uint8_t* data, size_t size);
    void closeTIFFFile();
    
    // TIFF parsing
    bool scanForOriginJSON();
    bool scanIFD(uint32_t offset, int depth = 0);
    bool readBinaryValue(uint32_t offset, uint32_t count, std::vector<uint8_t>& result);
    
    // JSON processing
    std::vector<uint8_t> extractJSONFromBinary(const std::vector<uint8_t>& binaryData);
    bool parseOriginJSON(const std::vector<uint8_t>& jsonData);
    
    // Byte order handling
    uint16_t swapBytes16(uint16_t val) const;
    uint32_t swapBytes32(uint32_t val) const;
    bool isValidTIFFData(const uint8_t* data, size_t size) const;
    
    // Utility
    void logInfo(const std::string& message) const;
    void logError(const std::string& message);
    
    // Data members
    OriginTelescopeMetadata m_metadata;
    std::string m_lastError;
    bool m_verboseLogging = false;
    int m_maxIFDDepth = 3;
    
    // File handling
    std::unique_ptr<std::ifstream> m_file;
    bool m_isLittleEndian = true;
    std::string m_currentFilePath;
    
    // Memory-based parsing
    const uint8_t* m_tiffData = nullptr;
    size_t m_tiffDataSize = 0;
    size_t m_currentOffset = 0;
};

#endif // ORIGIN_METADATA_EXTRACTOR_H
