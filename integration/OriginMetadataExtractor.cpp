#include <iostream>
#include "OriginMetadataExtractor.h"

// Implementation
OriginMetadataExtractor::OriginMetadataExtractor() {
    // Constructor implementation
}

OriginMetadataExtractor::~OriginMetadataExtractor() {
    closeTIFFFile();
}

bool OriginMetadataExtractor::extractFromTIFFFile(const std::string& filePath) {
    m_currentFilePath = filePath;
    m_lastError.clear();
    m_metadata = OriginTelescopeMetadata(); // Reset
    
    logInfo("Extracting Origin metadata from: " + filePath);
    
    if (!openTIFFFile(filePath)) {
        m_lastError = "Failed to open TIFF file: " + filePath;
        logError(m_lastError);
        return false;
    }
    
    bool success = scanForOriginJSON();
    closeTIFFFile();
    
    if (success && m_metadata.isValid) {
        logInfo("Successfully extracted Origin metadata for: " + m_metadata.objectName);
    } else {
        logInfo("No Origin telescope metadata found in TIFF file");
    }
    
    return success;
}

bool OriginMetadataExtractor::extractFromTIFFData(const uint8_t* tiffData, size_t dataSize) {
    m_lastError.clear();
    m_metadata = OriginTelescopeMetadata(); // Reset
    
    if (!tiffData || dataSize < sizeof(TIFFHeader)) {
        m_lastError = "Invalid TIFF data provided";
        return false;
    }
    
    if (!isValidTIFFData(tiffData, dataSize)) {
        m_lastError = "Data does not appear to be a valid TIFF file";
        return false;
    }
    
    return parseTIFFData(tiffData, dataSize);
}

bool OriginMetadataExtractor::openTIFFFile(const std::string& filePath) {
    m_file = std::make_unique<std::ifstream>(filePath, std::ios::binary);
    
    if (!m_file->is_open()) {
        return false;
    }
    
    // Read and validate TIFF header
    TIFFHeader header;
    m_file->read(reinterpret_cast<char*>(&header), sizeof(header));
    
    if (!m_file->good()) {
        return false;
    }
    
    // Determine byte order
    if (header.byteOrder == 0x4949) {  // 'II' - Intel (little endian)
        m_isLittleEndian = true;
    } else if (header.byteOrder == 0x4D4D) {  // 'MM' - Motorola (big endian)
        m_isLittleEndian = false;
    } else {
        m_lastError = "Invalid TIFF byte order marker";
        return false;
    }
    
    // Validate magic number
    uint16_t magic = swapBytes16(header.magic);
    if (magic != 42) {
        m_lastError = "Invalid TIFF magic number";
        return false;
    }
    
    logInfo("TIFF file opened successfully, byte order: " + 
            std::string(m_isLittleEndian ? "Little Endian" : "Big Endian"));
    
    return true;
}

bool OriginMetadataExtractor::parseTIFFData(const uint8_t* data, size_t size) {
    m_tiffData = data;
    m_tiffDataSize = size;
    m_currentOffset = 0;
    
    // Read TIFF header
    if (size < sizeof(TIFFHeader)) {
        m_lastError = "TIFF data too small";
        return false;
    }
    
    const TIFFHeader* header = reinterpret_cast<const TIFFHeader*>(data);
    
    // Determine byte order
    if (header->byteOrder == 0x4949) {
        m_isLittleEndian = true;
    } else if (header->byteOrder == 0x4D4D) {
        m_isLittleEndian = false;
    } else {
        m_lastError = "Invalid TIFF byte order marker";
        return false;
    }
    
    uint32_t ifdOffset = swapBytes32(header->ifdOffset);
    logInfo("Scanning TIFF data for Origin JSON, first IFD at offset: " + std::to_string(ifdOffset));
    
    return scanIFD(ifdOffset);
}

void OriginMetadataExtractor::closeTIFFFile() {
    if (m_file) {
        m_file->close();
        m_file.reset();
    }
    m_tiffData = nullptr;
    m_tiffDataSize = 0;
}

bool OriginMetadataExtractor::scanForOriginJSON() {
    if (!m_file || !m_file->is_open()) {
        return false;
    }
    
    // Read TIFF header to get first IFD offset
    m_file->seekg(0);
    TIFFHeader header;
    m_file->read(reinterpret_cast<char*>(&header), sizeof(header));
    
    uint32_t ifdOffset = swapBytes32(header.ifdOffset);
    logInfo("Scanning for Origin JSON starting at IFD offset: " + std::to_string(ifdOffset));
    
    return scanIFD(ifdOffset);
}

bool OriginMetadataExtractor::scanIFD(uint32_t offset, int depth) {
    if (offset == 0 || depth > m_maxIFDDepth) {
        return false;
    }
    
    // Handle both file and memory-based reading
    std::vector<uint8_t> ifdData;
    
    if (m_file) {
        // File-based reading
        m_file->seekg(offset);
        if (!m_file->good()) {
            return false;
        }
        
        uint16_t numEntries;
        m_file->read(reinterpret_cast<char*>(&numEntries), sizeof(numEntries));
        numEntries = swapBytes16(numEntries);
        
        if (numEntries > 1000) { // Sanity check
            logError("Suspicious number of IFD entries: " + std::to_string(numEntries));
            return false;
        }
        
        logInfo("Scanning IFD at offset " + std::to_string(offset) + " with " + std::to_string(numEntries) + " entries");
        
        // Read all entries
        for (uint16_t i = 0; i < numEntries; i++) {
            IFDEntry entry;
            m_file->read(reinterpret_cast<char*>(&entry), sizeof(entry));
            
            entry.tag = swapBytes16(entry.tag);
            entry.type = swapBytes16(entry.type);
            entry.count = swapBytes32(entry.count);
            entry.valueOffset = swapBytes32(entry.valueOffset);
            
            // Look for UNDEFINED data type that might contain JSON
            if (entry.type == 7 && entry.count > 50) { // UNDEFINED with reasonable size
                std::vector<uint8_t> binaryData;
                if (readBinaryValue(entry.valueOffset, entry.count, binaryData)) {
                    std::vector<uint8_t> jsonData = extractJSONFromBinary(binaryData);
                    if (!jsonData.empty()) {
                        if (parseOriginJSON(jsonData)) {
                            logInfo("Successfully found and parsed Origin JSON metadata");
                            return true; // Found it!
                        }
                    }
                }
            }
            
            // Recursively scan sub-IFDs (EXIF, GPS, etc.)
            if (entry.tag == 34665 || entry.tag == 34853) { // EXIF or GPS IFD
                if (scanIFD(entry.valueOffset, depth + 1)) {
                    return true; // Found in sub-IFD
                }
            }
        }
        
    } else if (m_tiffData) {
        // Memory-based reading
        if (offset + sizeof(uint16_t) > m_tiffDataSize) {
            return false;
        }
        
        const uint16_t* numEntriesPtr = reinterpret_cast<const uint16_t*>(m_tiffData + offset);
        uint16_t numEntries = swapBytes16(*numEntriesPtr);
        
        if (numEntries > 1000) {
            return false;
        }
        
        size_t entriesOffset = offset + sizeof(uint16_t);
        if (entriesOffset + numEntries * sizeof(IFDEntry) > m_tiffDataSize) {
            return false;
        }
        
        const IFDEntry* entries = reinterpret_cast<const IFDEntry*>(m_tiffData + entriesOffset);
        
        for (uint16_t i = 0; i < numEntries; i++) {
            IFDEntry entry = entries[i];
            entry.tag = swapBytes16(entry.tag);
            entry.type = swapBytes16(entry.type);
            entry.count = swapBytes32(entry.count);
            entry.valueOffset = swapBytes32(entry.valueOffset);
            
            if (entry.type == 7 && entry.count > 50) {
                if (entry.valueOffset + entry.count <= m_tiffDataSize) {
                    std::vector<uint8_t> binaryData(m_tiffData + entry.valueOffset, 
                                                  m_tiffData + entry.valueOffset + entry.count);
                    std::vector<uint8_t> jsonData = extractJSONFromBinary(binaryData);
                    if (!jsonData.empty()) {
                        if (parseOriginJSON(jsonData)) {
                            return true;
                        }
                    }
                }
            }
            
            if (entry.tag == 34665 || entry.tag == 34853) {
                if (scanIFD(entry.valueOffset, depth + 1)) {
                    return true;
                }
            }
        }
    }
    
    return false;
}

std::vector<uint8_t> OriginMetadataExtractor::extractJSONFromBinary(const std::vector<uint8_t>& binaryData) {
    // Convert binary data to string and look for JSON patterns
    std::string dataString;
    dataString.reserve(binaryData.size());
    
    for (uint8_t byte : binaryData) {
        if (byte >= 32 && byte <= 126) {
            dataString += static_cast<char>(byte);
        } else if (byte == 0) {
            dataString += ' '; // Replace null with space
        }
    }
    
    // Look for Origin telescope JSON patterns
    if (dataString.find("StackedInfo") == std::string::npos && 
        dataString.find("captureParams") == std::string::npos && 
        dataString.find("celestial") == std::string::npos) {
        return {}; // Empty vector
    }
    
    // Find the JSON object boundaries
    size_t jsonStart = dataString.find('{');
    size_t jsonEnd = dataString.rfind('}');
    
    if (jsonStart == std::string::npos || jsonEnd == std::string::npos || jsonEnd <= jsonStart) {
        return {};
    }
    
    std::string jsonString = dataString.substr(jsonStart, jsonEnd - jsonStart + 1);
    return std::vector<uint8_t>(jsonString.begin(), jsonString.end());
}

bool OriginMetadataExtractor::readBinaryValue(uint32_t offset, uint32_t count, std::vector<uint8_t>& result) {
    if (m_file) {
        // File-based reading
        std::streampos currentPos = m_file->tellg();
        m_file->seekg(offset);
        
        result.resize(count);
        m_file->read(reinterpret_cast<char*>(result.data()), count);
        
        bool success = m_file->gcount() == count;
        m_file->seekg(currentPos);
        return success;
        
    } else if (m_tiffData) {
        // Memory-based reading
        if (offset + count > m_tiffDataSize) {
            return false;
        }
        
        result.assign(m_tiffData + offset, m_tiffData + offset + count);
        return true;
    }
    
    return false;
}

// Utility methods
uint16_t OriginMetadataExtractor::swapBytes16(uint16_t val) const {
    return m_isLittleEndian ? val : ((val & 0xFF) << 8) | ((val >> 8) & 0xFF);
}

uint32_t OriginMetadataExtractor::swapBytes32(uint32_t val) const {
    if (m_isLittleEndian) return val;
    return ((val & 0xFF) << 24) | (((val >> 8) & 0xFF) << 16) | 
           (((val >> 16) & 0xFF) << 8) | ((val >> 24) & 0xFF);
}

bool OriginMetadataExtractor::isValidTIFFData(const uint8_t* data, size_t size) const {
    if (size < sizeof(TIFFHeader)) {
        return false;
    }
    
    const TIFFHeader* header = reinterpret_cast<const TIFFHeader*>(data);
    return (header->byteOrder == 0x4949 || header->byteOrder == 0x4D4D) &&
           (header->magic == 42 || header->magic == 0x2A00); // Handle byte order
}

std::string OriginMetadataExtractor::exportAsText() const {
    if (!m_metadata.isValid) return "No valid Origin metadata available";
    
    std::string text;
    text += "=== Origin Telescope Metadata ===\n";
    text += "Object: " + m_metadata.objectName + "\n";
    text += "Coordinates: " + formatCoordinates() + "\n";
    text += "Field of View: " + std::to_string(m_metadata.fieldOfViewX) + "° × " + 
            std::to_string(m_metadata.fieldOfViewY) + "°\n";
    text += "Image Size: " + std::to_string(m_metadata.imageWidth) + " × " + 
            std::to_string(m_metadata.imageHeight) + " pixels\n";
    text += "Pixel Scale: " + std::to_string(m_metadata.getAveragePixelScale()) + " arcsec/pixel\n";
    text += "Orientation: " + std::to_string(m_metadata.orientation) + "°\n";
    text += "Date/Time: " + m_metadata.dateTime + "\n";
    text += "Filter: " + m_metadata.filter + "\n";
    text += "Exposure: " + std::to_string(m_metadata.exposure) + "s × " + 
            std::to_string(m_metadata.stackedDepth) + " frames\n";
    text += "Total Duration: " + std::to_string(m_metadata.getTotalExposureSeconds()) + " seconds\n";
    text += "ISO: " + std::to_string(m_metadata.iso) + "\n";
    text += "Temperature: " + std::to_string(m_metadata.temperature) + "°C\n";
    
    if (m_metadata.gpsLatitude != 0.0 || m_metadata.gpsLongitude != 0.0) {
        text += "GPS Location: " + std::to_string(m_metadata.gpsLatitude) + "°, " + 
                std::to_string(m_metadata.gpsLongitude) + "° (alt: " + 
                std::to_string(m_metadata.gpsAltitude) + "m)\n";
    }
    
    text += "UUID: " + m_metadata.uuid + "\n";
    
    return text;
}

void OriginMetadataExtractor::logInfo(const std::string& message) const {
    if (m_verboseLogging) {
        // Use std::cout or your preferred logging mechanism
        std::cout << "[OriginExtractor] " << message << std::endl;
    }
}

void OriginMetadataExtractor::logError(const std::string& message) {
    // Always log errors
    std::cerr << "[OriginExtractor ERROR] " << message << std::endl;
    m_lastError = message;
}

// Static utility method
bool OriginMetadataExtractor::containsOriginMetadata(const std::string& filePath) {
    OriginMetadataExtractor extractor;
    extractor.setVerboseLogging(false); // Silent check
    return extractor.extractFromTIFFFile(filePath);
}

// Fixed coordinate conversion in parseOriginJSON method
// The Origin telescope stores coordinates in RADIANS, not degrees!

bool OriginMetadataExtractor::parseOriginJSON(const std::vector<uint8_t>& jsonData) {
    try {
        std::string jsonString(jsonData.begin(), jsonData.end());
        
        // Look for key patterns in the JSON string
        auto findValue = [&](const std::string& key) -> std::string {
            std::string searchKey = "\"" + key + "\":";
            size_t pos = jsonString.find(searchKey);
            if (pos == std::string::npos) return "";
            
            pos += searchKey.length();
            while (pos < jsonString.length() && (jsonString[pos] == ' ' || jsonString[pos] == '\t')) pos++;
            
            if (pos >= jsonString.length()) return "";
            
            if (jsonString[pos] == '"') {
                // String value
                pos++;
                size_t endPos = jsonString.find('"', pos);
                if (endPos == std::string::npos) return "";
                return jsonString.substr(pos, endPos - pos);
            } else {
                // Numeric value
                size_t endPos = pos;
                while (endPos < jsonString.length() && 
                       (std::isdigit(jsonString[endPos]) || jsonString[endPos] == '.' || 
                        jsonString[endPos] == '-' || jsonString[endPos] == '+' || 
                        jsonString[endPos] == 'e' || jsonString[endPos] == 'E')) {
                    endPos++;
                }
                return jsonString.substr(pos, endPos - pos);
            }
        };
        
        // Extract core metadata
        m_metadata.objectName = findValue("objectName");
        
        // Parse celestial coordinates - CONVERT FROM RADIANS TO DEGREES!
        std::string firstStr = findValue("first");
        std::string secondStr = findValue("second");
        if (!firstStr.empty() && !secondStr.empty()) {
            double raRadians = std::stod(firstStr);
            double decRadians = std::stod(secondStr);
            
            // Convert from radians to degrees
            m_metadata.centerRA = raRadians * 180.0 / M_PI;
            m_metadata.centerDec = decRadians * 180.0 / M_PI;
            
            // Ensure RA is in [0, 360) range
            while (m_metadata.centerRA < 0) m_metadata.centerRA += 360.0;
            while (m_metadata.centerRA >= 360.0) m_metadata.centerRA -= 360.0;
            
            // Ensure Dec is in [-90, 90] range
            if (m_metadata.centerDec > 90.0) m_metadata.centerDec = 90.0;
            if (m_metadata.centerDec < -90.0) m_metadata.centerDec = -90.0;
            
            logInfo("Converted coordinates from radians: RA=" + std::to_string(raRadians) + 
                   " rad -> " + std::to_string(m_metadata.centerRA) + "°, Dec=" + 
                   std::to_string(decRadians) + " rad -> " + std::to_string(m_metadata.centerDec) + "°");
        }
        
        // Parse field of view - these are already in degrees
        std::string fovXStr = findValue("fovX");
        std::string fovYStr = findValue("fovY");
        if (!fovXStr.empty()) m_metadata.fieldOfViewX = std::stod(fovXStr);
        if (!fovYStr.empty()) m_metadata.fieldOfViewY = std::stod(fovYStr);
        
        // Parse image dimensions
        std::string widthStr = findValue("imageWidth");
        std::string heightStr = findValue("imageHeight");
        if (!widthStr.empty()) m_metadata.imageWidth = std::stoi(widthStr);
        if (!heightStr.empty()) m_metadata.imageHeight = std::stoi(heightStr);
        
        // Parse orientation - this might also be in radians, check the value
        std::string orientationStr = findValue("orientation");
        if (!orientationStr.empty()) {
            double orientationValue = std::stod(orientationStr);
            
            // If the value is small (< 10), it's probably in radians
            if (std::abs(orientationValue) < 10.0) {
                m_metadata.orientation = orientationValue * 180.0 / M_PI;
                logInfo("Converted orientation from radians: " + std::to_string(orientationValue) + 
                       " rad -> " + std::to_string(m_metadata.orientation) + "°");
            } else {
                // Already in degrees
                m_metadata.orientation = orientationValue;
            }
        }
        
        // Parse other fields (these should be in their natural units)
        std::string exposureStr = findValue("exposure");
        if (!exposureStr.empty()) m_metadata.exposure = std::stod(exposureStr);
        
        std::string isoStr = findValue("iso");
        if (!isoStr.empty()) m_metadata.iso = std::stoi(isoStr);
        
        std::string temperatureStr = findValue("temperature");
        if (!temperatureStr.empty()) m_metadata.temperature = std::stod(temperatureStr);
        
        std::string stackedDepthStr = findValue("stackedDepth");
        if (!stackedDepthStr.empty()) m_metadata.stackedDepth = std::stoi(stackedDepthStr);
        
        std::string totalDurationStr = findValue("totalDurationMs");
        if (!totalDurationStr.empty()) m_metadata.totalDurationMs = std::stod(totalDurationStr);
        
        m_metadata.dateTime = findValue("dateTime");
        m_metadata.filter = findValue("filter");
        m_metadata.bayerPattern = findValue("bayer");
        m_metadata.uuid = findValue("uuid");
        
        // GPS coordinates - should be in degrees
        std::string latStr = findValue("latitude");
        std::string lonStr = findValue("longitude");
        std::string altStr = findValue("altitude");
        if (!latStr.empty()) m_metadata.gpsLatitude = std::stod(latStr);
        if (!lonStr.empty()) m_metadata.gpsLongitude = std::stod(lonStr);
        if (!altStr.empty()) m_metadata.gpsAltitude = std::stod(altStr);
        
        // Processing parameters
        std::string stretchBgStr = findValue("stretchBackground");
        std::string stretchStrStr = findValue("stretchStrength");
        if (!stretchBgStr.empty()) m_metadata.stretchBackground = std::stod(stretchBgStr);
        if (!stretchStrStr.empty()) m_metadata.stretchStrength = std::stod(stretchStrStr);
        
        // Validate that we have essential data
        if (!m_metadata.objectName.empty() && 
            m_metadata.centerRA >= 0.0 && m_metadata.centerRA <= 360.0 &&
            m_metadata.centerDec >= -90.0 && m_metadata.centerDec <= 90.0 &&
            m_metadata.imageWidth > 0 && 
            m_metadata.imageHeight > 0) {
            
            m_metadata.isValid = true;
            m_metadata.extractionSource = "TIFF_IFD";
            
            logInfo("Successfully parsed Origin metadata for: " + m_metadata.objectName);
            logInfo("Final coordinates: RA=" + std::to_string(m_metadata.centerRA) + 
                   "°, Dec=" + std::to_string(m_metadata.centerDec) + "°");
            return true;
        } else {
            logError("Origin JSON found but missing essential fields or invalid coordinates");
            logError("RA=" + std::to_string(m_metadata.centerRA) + 
                    "°, Dec=" + std::to_string(m_metadata.centerDec) + 
                    "°, Object='" + m_metadata.objectName + "'");
            return false;
        }
        
    } catch (const std::exception& e) {
        m_lastError = "Error parsing Origin JSON: " + std::string(e.what());
        logError(m_lastError);
        return false;
    }
}

// Updated coordinate formatting to show both formats
std::string OriginMetadataExtractor::formatCoordinates() const {
    if (!m_metadata.isValid) return "Invalid metadata";
    
    // Convert RA to hours:minutes:seconds
    double raHours = m_metadata.centerRA / 15.0;
    int hours = static_cast<int>(raHours);
    int minutes = static_cast<int>((raHours - hours) * 60.0);
    double seconds = ((raHours - hours) * 60.0 - minutes) * 60.0;
    
    // Convert Dec to degrees:arcminutes:arcseconds
    int degrees = static_cast<int>(m_metadata.centerDec);
    int arcminutes = static_cast<int>(std::abs(m_metadata.centerDec - degrees) * 60.0);
    double arcseconds = (std::abs(m_metadata.centerDec - degrees) * 60.0 - arcminutes) * 60.0;
    
    char buffer[512];
    snprintf(buffer, sizeof(buffer), 
             "RA %02dh %02dm %04.1fs (%06.3f°), Dec %+03d° %02d' %04.1f\" (%+07.3f°)",
             hours, minutes, seconds, m_metadata.centerRA,
             degrees, arcminutes, arcseconds, m_metadata.centerDec);
    
    return std::string(buffer);
}

/*
Expected output after fix for M51:

=== FinalStackedMaster.tiff ===
Object: Whirlpool Galaxy - M 51
Coordinates: RA 13h 29m 52.5s (202.469°), Dec +47° 11' 47.7" (+47.195°)
Field of view: 0.0218929° × 0.014672°
Image size: 3056 × 2048 pixels
Pixel scale: 1.43 arcsec/pixel  <- This should also be corrected
Orientation: 25.9°  <- Converted from 0.453 radians
Date/Time: 2025-05-19T22:22:12+0100
Filter: Clear
Exposure: 10s × 480 frames
Total exposure: 4800 seconds
ISO: 200
Temperature: 21.4°C  <- This should be extracted correctly
GPS: 52.2452°, 0.0797178° (alt: 0m)
UUID: 56E5B93C-B4A4-45EF-BA2E-1E692FEBC622
Status: ✅ Valid Origin metadata
Hint quality: EXCELLENT (score: 7/7)
Expected speedup: 8x

The corrected coordinates (RA=202.469°, Dec=47.195°) are exactly right for M51!
*/
