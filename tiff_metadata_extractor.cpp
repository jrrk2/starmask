#include <iostream>
#include <string>
#include <iomanip>
#include <vector>
#include <cstdint>
#include <fstream>
#include <cstring>
#include <tiffio.h>

#pragma pack(push, 1)
struct TIFFHeader {
    uint16_t byteOrder;     // 'II' (little endian) or 'MM' (big endian)
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

class TIFFMetadataExtractor {
private:
    TIFF* tiff;
    std::string filename;
    bool isLittleEndian;
    std::ifstream file;

public:
    TIFFMetadataExtractor(const std::string& file) : filename(file), tiff(nullptr), isLittleEndian(true) {}
    
    ~TIFFMetadataExtractor() {
        if (tiff) {
            TIFFClose(tiff);
        }
        if (file.is_open()) {
            file.close();
        }
    }
    
    bool open() {
        tiff = TIFFOpen(filename.c_str(), "r");
        if (!tiff) return false;
        
        // Also open for raw access
        file.open(filename, std::ios::binary);
        return file.is_open();
    }
    
    uint16_t swapBytes16(uint16_t val) {
        if (isLittleEndian) return val;
        return ((val & 0xFF) << 8) | ((val >> 8) & 0xFF);
    }
    
    uint32_t swapBytes32(uint32_t val) {
        if (isLittleEndian) return val;
        return ((val & 0xFF) << 24) | (((val >> 8) & 0xFF) << 16) | 
               (((val >> 16) & 0xFF) << 8) | ((val >> 24) & 0xFF);
    }
    
    std::string getTypeName(uint16_t type) {
        switch (type) {
            case 1: return "BYTE";
            case 2: return "ASCII";
            case 3: return "SHORT";
            case 4: return "LONG";
            case 5: return "RATIONAL";
            case 6: return "SBYTE";
            case 7: return "UNDEFINED";
            case 8: return "SSHORT";
            case 9: return "SLONG";
            case 10: return "SRATIONAL";
            case 11: return "FLOAT";
            case 12: return "DOUBLE";
            default: return "UNKNOWN";
        }
    }
    
    std::string getTagName(uint16_t tag) {
        // Common TIFF tag names
        switch (tag) {
            case 256: return "ImageWidth";
            case 257: return "ImageLength";
            case 258: return "BitsPerSample";
            case 259: return "Compression";
            case 262: return "PhotometricInterpretation";
            case 273: return "StripOffsets";
            case 274: return "Orientation";
            case 277: return "SamplesPerPixel";
            case 278: return "RowsPerStrip";
            case 279: return "StripByteCounts";
            case 284: return "PlanarConfiguration";
            case 305: return "Software";
            case 306: return "DateTime";
            case 315: return "Artist";
            case 320: return "ColorMap";
            case 33432: return "Copyright";
            case 34665: return "ExifIFD";
            case 34853: return "GPSIFD";
            case 700: return "XMP";
            default: return "Unknown";
        }
    }
    
    void readBinaryValue(uint32_t offset, uint32_t count, std::vector<uint8_t>& result) {
        if (!file.is_open()) return;
        
        std::streampos currentPos = file.tellg();
        file.seekg(offset);
        
        result.resize(count);
        file.read(reinterpret_cast<char*>(result.data()), count);
        
        file.seekg(currentPos);
    }
    
    void printBinaryAsString(const std::vector<uint8_t>& data) {
        std::string result;
        bool foundJson = false;
        
        for (size_t i = 0; i < data.size(); i++) {
            if (data[i] >= 32 && data[i] <= 126) {  // Printable ASCII
                result += static_cast<char>(data[i]);
            } else if (data[i] == 0) {
                result += ' ';  // Replace null with space
            }
        }
        
        // Check if this looks like JSON
        if (result.find("StackedInfo") != std::string::npos ||
            result.find("captureParams") != std::string::npos ||
            result.find("\"gps\"") != std::string::npos ||
            result.find("{") != std::string::npos) {
            std::cout << "\n*** FOUND ORIGIN JSON DATA ***\n";
            std::cout << result << std::endl;
            std::cout << "*** END JSON DATA ***\n";
        } else if (result.length() > 20) {
            std::cout << " Binary data (" << data.size() << " bytes): " << result.substr(0, 100);
            if (result.length() > 100) std::cout << "...";
            std::cout << std::endl;
        } else {
            std::cout << " Binary data (" << data.size() << " bytes)" << std::endl;
        }
    }
    
    void scanRawTIFFStructure() {
        if (!file.is_open()) {
            std::cout << "Error: Could not open file for raw scanning" << std::endl;
            return;
        }
        
        std::cout << "\n=== Raw TIFF Structure Scan ===" << std::endl;
        
        // Read TIFF header
        TIFFHeader header;
        file.seekg(0);
        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        
        // Determine byte order
        if (header.byteOrder == 0x4949) {  // 'II' - Intel (little endian)
            isLittleEndian = true;
            std::cout << "Byte order: Little Endian (Intel)" << std::endl;
        } else if (header.byteOrder == 0x4D4D) {  // 'MM' - Motorola (big endian)
            isLittleEndian = false;
            std::cout << "Byte order: Big Endian (Motorola)" << std::endl;
        } else {
            std::cout << "Invalid TIFF file - bad byte order marker" << std::endl;
            return;
        }
        
        uint32_t magic = swapBytes16(header.magic);
        uint32_t ifdOffset = swapBytes32(header.ifdOffset);
        
        std::cout << "Magic number: " << magic << std::endl;
        std::cout << "First IFD offset: " << ifdOffset << std::endl;
        
        // Scan all IFDs
        scanIFD(ifdOffset, 0);
    }
    
    void scanIFD(uint32_t offset, int level) {
        if (offset == 0 || !file.is_open()) return;
        
        std::string indent(level * 2, ' ');
        std::cout << indent << "\n--- IFD at offset " << offset << " ---" << std::endl;
        
        file.seekg(offset);
        
        // Read number of entries
        uint16_t numEntries;
        file.read(reinterpret_cast<char*>(&numEntries), sizeof(numEntries));
        numEntries = swapBytes16(numEntries);
        
        std::cout << indent << "Number of entries: " << numEntries << std::endl;
        
        // Sanity check - if too many entries, likely corrupted
        if (numEntries > 1000) {
            std::cout << indent << "*** WARNING: Too many entries (" << numEntries 
                      << "), likely corrupted data. Stopping scan. ***" << std::endl;
            return;
        }
        
        // Read all IFD entries
        for (uint16_t i = 0; i < numEntries; i++) {
            IFDEntry entry;
            file.read(reinterpret_cast<char*>(&entry), sizeof(entry));
            
            // Swap bytes if needed
            entry.tag = swapBytes16(entry.tag);
            entry.type = swapBytes16(entry.type);
            entry.count = swapBytes32(entry.count);
            entry.valueOffset = swapBytes32(entry.valueOffset);
            
            std::cout << indent << "Tag " << entry.tag << " (0x" << std::hex << entry.tag << std::dec 
                      << ", " << getTagName(entry.tag) << ")";
            std::cout << " Type: " << getTypeName(entry.type) << " Count: " << entry.count;
            
            // Handle the value based on type and size
            uint32_t valueSize = getTypeSize(entry.type) * entry.count;
            
            if (entry.type == 2 && valueSize > 0) { // ASCII string
                std::string stringValue;
                if (valueSize <= 4) {
                    // Value is stored in the offset field itself
                    stringValue = std::string(reinterpret_cast<char*>(&entry.valueOffset), 
                                            std::min(4u, entry.count));
                } else {
                    // Value is at the offset
                    std::vector<uint8_t> data;
                    readBinaryValue(entry.valueOffset, entry.count, data);
                    
                    // Convert to string and clean up
                    for (uint8_t byte : data) {
                        if (byte == 0) break;  // Stop at null terminator
                        if (byte >= 32 && byte <= 126) {
                            stringValue += static_cast<char>(byte);
                        }
                    }
                }
                
                std::cout << " Value: \"" << stringValue << "\"";
                
                // Check if this looks like our JSON data
                if (stringValue.find("StackedInfo") != std::string::npos ||
                    stringValue.find("captureParams") != std::string::npos ||
                    stringValue.find("\"gps\"") != std::string::npos ||
                    (stringValue.find("{") != std::string::npos && stringValue.length() > 50)) {
                    std::cout << "\n" << indent << "*** FOUND ORIGIN JSON METADATA ***";
                    std::cout << "\n" << indent << stringValue;
                    std::cout << "\n" << indent << "*** END JSON METADATA ***";
                }
            } else if (entry.type == 7 && entry.count > 10) { // UNDEFINED data (could be JSON)
                std::cout << " ValueOffset: " << entry.valueOffset;
                std::vector<uint8_t> binaryData;
                readBinaryValue(entry.valueOffset, entry.count, binaryData);
                
                // Try to interpret as text
                std::cout << "\n" << indent << "  UNDEFINED data analysis:";
                printBinaryAsString(binaryData);
                
            } else if (entry.type == 3 && entry.count == 1) { // Single SHORT
                std::cout << " Value: " << (valueSize <= 4 ? (entry.valueOffset & 0xFFFF) : entry.valueOffset);
            } else if (entry.type == 4 && entry.count == 1) { // Single LONG  
                std::cout << " Value: " << entry.valueOffset;
            } else {
                std::cout << " ValueOffset: " << entry.valueOffset;
            }
            
            std::cout << std::endl;
            
            // If this is a sub-IFD, recursively scan it (but limit depth to prevent corruption issues)
            if (level < 3 && (entry.tag == 34665 || entry.tag == 34853)) { // EXIF or GPS IFD
                std::cout << indent << "Scanning sub-IFD:" << std::endl;
                scanIFD(entry.valueOffset, level + 1);
            }
        }
        
        // Read offset to next IFD (but only if at top level to avoid corruption)
        if (level == 0) {
            uint32_t nextIFDOffset;
            file.read(reinterpret_cast<char*>(&nextIFDOffset), sizeof(nextIFDOffset));
            nextIFDOffset = swapBytes32(nextIFDOffset);
            
            if (nextIFDOffset != 0 && nextIFDOffset < 1000000) { // Sanity check offset
                std::cout << indent << "Next IFD at offset: " << nextIFDOffset << std::endl;
                // Don't follow next IFD to avoid the corrupted second directory
                std::cout << indent << "*** Skipping next IFD to avoid corruption ***" << std::endl;
            }
        }
    }
    
    uint32_t getTypeSize(uint16_t type) {
        switch (type) {
            case 1: case 2: case 6: case 7: return 1;  // BYTE, ASCII, SBYTE, UNDEFINED
            case 3: case 8: return 2;                   // SHORT, SSHORT
            case 4: case 9: case 11: return 4;          // LONG, SLONG, FLOAT
            case 5: case 10: case 12: return 8;         // RATIONAL, SRATIONAL, DOUBLE
            default: return 1;
        }
    }
    
    void extractBasicInfo() {
        if (!tiff) return;
        
        uint32_t width, height;
        uint16_t bitsPerSample, samplesPerPixel, compression, photometric;
        
        std::cout << "=== Basic Image Information ===" << std::endl;
        
        if (TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &width)) {
            std::cout << "Width: " << width << " pixels" << std::endl;
        }
        
        if (TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &height)) {
            std::cout << "Height: " << height << " pixels" << std::endl;
        }
        
        if (TIFFGetField(tiff, TIFFTAG_BITSPERSAMPLE, &bitsPerSample)) {
            std::cout << "Bits per sample: " << bitsPerSample << std::endl;
        }
        
        if (TIFFGetField(tiff, TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel)) {
            std::cout << "Samples per pixel: " << samplesPerPixel << std::endl;
        }
        
        char* software;
        if (TIFFGetField(tiff, TIFFTAG_SOFTWARE, &software)) {
            std::cout << "Software: " << software << std::endl;
        }
    }
    
    void extractAll() {
        if (!tiff) {
            std::cerr << "Error: TIFF file not opened" << std::endl;
            return;
        }
        
        std::cout << "TIFF Metadata for: " << filename << std::endl;
        std::cout << "========================================" << std::endl;
        
        extractBasicInfo();
    }
    
    void extractAllTags() {
        scanRawTIFFStructure();
    }
};

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " <tiff_file> [options]" << std::endl;
    std::cout << "Extract and display metadata from a TIFF file" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --debug    Scan raw TIFF structure (finds all metadata)" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 3) {
        printUsage(argv[0]);
        return 1;
    }
    
    std::string filename = argv[1];
    bool debugMode = false;
    
    if (argc == 3 && std::string(argv[2]) == "--debug") {
        debugMode = true;
    }
    
    TIFFMetadataExtractor extractor(filename);
    
    if (!extractor.open()) {
        std::cerr << "Error: Could not open TIFF file '" << filename << "'" << std::endl;
        std::cerr << "Please check that the file exists and is a valid TIFF file." << std::endl;
        return 1;
    }
    
    if (debugMode) {
        extractor.extractAllTags();
    } else {
        extractor.extractAll();
    }
    
    return 0;
}