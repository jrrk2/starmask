#ifndef LOCAL2MASS_CATALOG_MMAP_H
#define LOCAL2MASS_CATALOG_MMAP_H

#include <QDebug>
#include <QString>
#include <QFile>
#include <QFileInfo>
#include <QTime>
#include <cmath>
#include <algorithm>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

class Local2MASSCatalog
{
public:
    struct Star {
        double ra, dec, magnitude;
        QString id;
        bool isValid = true;
    };
    
    static QString s_catalogPath;
    
    static void setCatalogPath(const QString& path) {
        s_catalogPath = path;
    }
    
    static QVector<Star> queryRegion(double centerRA, double centerDec, double radiusDegrees, double maxMagnitude = 20.0)
    {
        QVector<Star> stars;
        
        if (s_catalogPath.isEmpty()) {
            qDebug() << "❌ 2MASS catalog path not set";
            return stars;
        }
        
        qDebug() << QString("📂 Reading 2MASS catalog from: %1").arg(s_catalogPath);
        qDebug() << QString("🎯 Query region: RA=%1° Dec=%2° radius=%3° mag≤%4")
                    .arg(centerRA, 0, 'f', 4).arg(centerDec, 0, 'f', 4)
                    .arg(radiusDegrees, 0, 'f', 2).arg(maxMagnitude, 0, 'f', 1);
        
        auto startTime = QTime::currentTime();
        
        // Calculate declination search bounds
        double minDec = centerDec - radiusDegrees;
        double maxDec = centerDec + radiusDegrees;
        
        qDebug() << QString("📍 Dec search bounds: [%1°, %2°]").arg(minDec, 0, 'f', 4).arg(maxDec, 0, 'f', 4);
        
        // Memory-map the file
        MappedFile mappedFile(s_catalogPath);
        if (!mappedFile.isValid()) {
            qDebug() << "❌ Failed to memory-map catalog file";
            return stars;
        }
        
        qDebug() << QString("📊 Memory-mapped %1 MB file").arg(mappedFile.size() / (1024 * 1024));
        
        // Binary search to find the start position
        size_t startPos = findDeclinationStart(mappedFile, minDec);
        
        if (startPos == SIZE_MAX) {
            qDebug() << "❌ Binary search failed to find start position";
            return stars;
        }
        
        qDebug() << QString("📊 Binary search complete, starting sequential read from position %1").arg(startPos);
        
        // Now read sequentially until we pass maxDec
        int totalLines = 0;
        int starsInRegion = 0;
        int starsWithinMagnitude = 0;
        
        // Pre-calculate RA bounds for efficiency
        double minRA = centerRA - radiusDegrees;
        double maxRA = centerRA + radiusDegrees;
        bool wraparound = (minRA < 0 || maxRA > 360);
        if (minRA < 0) minRA += 360;
        if (maxRA > 360) maxRA -= 360;
        
        double cos_dec = cos(centerDec * M_PI / 180.0);
        
        // Process lines from the mapped memory
        const char* data = mappedFile.data();
        size_t fileSize = mappedFile.size();
        size_t pos = startPos;
        
        while (pos < fileSize) {
            // Find end of current line
            size_t lineEnd = pos;
            while (lineEnd < fileSize && data[lineEnd] != '\n' && data[lineEnd] != '\r') {
                lineEnd++;
            }
            
            if (lineEnd == pos) {
                // Empty line, skip
                pos = lineEnd + 1;
                continue;
            }
            
            // Extract line as string
            QString line = QString::fromUtf8(data + pos, lineEnd - pos);
            totalLines++;
            
            // Parse line format: RA|Dec|ID|magnitude
            QStringList parts = line.split('|');
            if (parts.size() >= 4) {
                bool raOk, decOk, magOk;
                double ra = parts[0].toDouble(&raOk);
                double dec = parts[1].toDouble(&decOk);
                QString id = parts[2].trimmed();
                double magnitude = parts[3].toDouble(&magOk);
                
                if (raOk && decOk && magOk) {
                    // Early exit if we've passed the maximum declination
                    if (dec > maxDec) {
                        qDebug() << QString("📈 Reached max declination %1° at line %2, stopping search")
                                    .arg(dec, 0, 'f', 4).arg(totalLines);
                        break;
                    }
                    
                    // Skip if still below minimum declination
                    if (dec >= minDec) {
                        // Quick RA bounds check
                        bool inRABounds;
                        if (wraparound) {
                            inRABounds = (ra >= minRA || ra <= maxRA);
                        } else {
                            inRABounds = (ra >= minRA && ra <= maxRA);
                        }
                        
                        if (inRABounds) {
                            // More precise distance check
                            double ra_diff = ra - centerRA;
                            double dec_diff = dec - centerDec;
                            
                            // Handle RA wraparound
                            if (ra_diff > 180) ra_diff -= 360;
                            if (ra_diff < -180) ra_diff += 360;
                            
                            double angular_distance = sqrt(pow(ra_diff * cos_dec, 2) + pow(dec_diff, 2));
                            
                            if (angular_distance <= radiusDegrees) {
                                starsInRegion++;
                                
                                if (magnitude <= maxMagnitude) {
                                    Star star;
                                    star.ra = ra;
                                    star.dec = dec;
                                    star.magnitude = magnitude;
                                    star.id = QString("2MASS_%1").arg(id);
                                    star.isValid = true;
                                    
                                    stars.append(star);
                                    starsWithinMagnitude++;
                                }
                            }
                        }
                    }
                }
            }
            
            // Progress update for very large result sets
            if (totalLines % 1000000 == 0) {
                qDebug() << QString("📊 Processed %1 lines, found %2 stars in region")
                            .arg(totalLines).arg(starsInRegion);
            }
            
            // Move to next line
            pos = lineEnd;
            while (pos < fileSize && (data[pos] == '\n' || data[pos] == '\r')) {
                pos++;
            }
        }
        
        auto elapsed = startTime.msecsTo(QTime::currentTime());
        
        qDebug() << QString("✅ 2MASS catalog search completed in %1ms").arg(elapsed);
        qDebug() << QString("📈 Statistics:");
        qDebug() << QString("   - Lines processed: %1").arg(totalLines);
        qDebug() << QString("   - Stars in region: %2").arg(starsInRegion);
        qDebug() << QString("   - Stars within magnitude limit: %3").arg(starsWithinMagnitude);
        qDebug() << QString("   - Final catalog size: %4").arg(stars.size());
        qDebug() << QString("   - Performance: mmap-optimized");
        
        // Sort by magnitude (brightest first)
        std::sort(stars.begin(), stars.end(), 
                  [](const Star& a, const Star& b) {
                      return a.magnitude < b.magnitude;
                  });
        
        return stars;
    }
    
private:
    // Cross-platform memory-mapped file wrapper
    class MappedFile {
    public:
        explicit MappedFile(const QString& filePath) 
            : m_data(nullptr), m_size(0), m_valid(false)
        {
#ifdef Q_OS_WIN
            // Windows implementation
            m_file = CreateFileA(filePath.toLocal8Bit().constData(),
                               GENERIC_READ, FILE_SHARE_READ, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            
            if (m_file == INVALID_HANDLE_VALUE) {
                qDebug() << "❌ Failed to open file for mapping (Windows)";
                return;
            }
            
            LARGE_INTEGER fileSize;
            if (!GetFileSizeEx(m_file, &fileSize)) {
                CloseHandle(m_file);
                return;
            }
            m_size = fileSize.QuadPart;
            
            m_mapping = CreateFileMapping(m_file, nullptr, PAGE_READONLY, 0, 0, nullptr);
            if (!m_mapping) {
                CloseHandle(m_file);
                return;
            }
            
            m_data = static_cast<const char*>(MapViewOfFile(m_mapping, FILE_MAP_READ, 0, 0, 0));
            if (!m_data) {
                CloseHandle(m_mapping);
                CloseHandle(m_file);
                return;
            }
#else
            // Unix/Linux/macOS implementation
            m_fd = open(filePath.toLocal8Bit().constData(), O_RDONLY);
            if (m_fd == -1) {
                qDebug() << "❌ Failed to open file for mapping (Unix)";
                return;
            }
            
            struct stat st;
            if (fstat(m_fd, &st) == -1) {
                close(m_fd);
                return;
            }
            m_size = st.st_size;
            
            m_data = static_cast<const char*>(mmap(nullptr, m_size, PROT_READ, MAP_PRIVATE, m_fd, 0));
            if (m_data == MAP_FAILED) {
                close(m_fd);
                m_data = nullptr;
                return;
            }
            
            // Advise kernel about access pattern
            madvise(const_cast<char*>(m_data), m_size, MADV_SEQUENTIAL);
#endif
            m_valid = true;
        }
        
        ~MappedFile() {
#ifdef Q_OS_WIN
            if (m_data) UnmapViewOfFile(m_data);
            if (m_mapping) CloseHandle(m_mapping);
            if (m_file != INVALID_HANDLE_VALUE) CloseHandle(m_file);
#else
            if (m_data && m_data != MAP_FAILED) {
                munmap(const_cast<char*>(m_data), m_size);
            }
            if (m_fd != -1) close(m_fd);
#endif
        }
        
        bool isValid() const { return m_valid; }
        const char* data() const { return m_data; }
        size_t size() const { return m_size; }
        
    private:
        const char* m_data;
        size_t m_size;
        bool m_valid;
        
#ifdef Q_OS_WIN
        HANDLE m_file = INVALID_HANDLE_VALUE;
        HANDLE m_mapping = nullptr;
#else
        int m_fd = -1;
#endif
    };
    
    // Binary search in memory-mapped data
    static size_t findDeclinationStart(const MappedFile& file, double targetDec)
    {
        const char* data = file.data();
        size_t fileSize = file.size();
        size_t left = 0;
        size_t right = fileSize;
        size_t bestPos = 0;
        
        qDebug() << QString("🔍 mmap binary search for Dec >= %1° in %2 MB file")
                    .arg(targetDec, 0, 'f', 4).arg(fileSize / (1024 * 1024));
        
        int iterations = 0;
        const int maxIterations = 35;
        
        while (left < right && iterations < maxIterations) {
            iterations++;
            size_t mid = left + (right - left) / 2;
            
            // Find the start of a line near this position
            size_t lineStart = findLineStart(data, fileSize, mid);
            if (lineStart == SIZE_MAX) {
                qDebug() << QString("⚠️  Could not find line start near position %1").arg(mid);
                left = mid + 1;
                continue;
            }
            
            // Read the declination from this line
            double dec = readDeclinationAtPosition(data, fileSize, lineStart);
            if (dec == -999.0) {
                qDebug() << QString("⚠️  Could not read declination at position %1").arg(lineStart);
                left = mid + 1;
                continue;
            }
            
            // Only log every few iterations to reduce noise
            if (iterations <= 10 || iterations % 5 == 0) {
                qDebug() << QString("   mmap iteration %1: pos=%2, dec=%3° (target=%4°)")
                            .arg(iterations).arg(lineStart).arg(dec, 0, 'f', 4).arg(targetDec, 0, 'f', 4);
            }
            
            if (dec >= targetDec) {
                bestPos = lineStart;
                right = mid;
                
                // Early exit if we're very close to the target
                if (std::abs(dec - targetDec) < 0.001) {
                    qDebug() << QString("   🎯 Found very close match at iteration %1").arg(iterations);
                    break;
                }
            } else {
                left = mid + 1;
            }
            
            // Detect infinite loops (same position)
            static size_t lastPos = SIZE_MAX;
            static int sameCount = 0;
            
            if (lineStart == lastPos) {
                sameCount++;
                if (sameCount > 3) {
                    qDebug() << QString("⚠️  Breaking infinite loop at position %1").arg(lineStart);
                    break;
                }
            } else {
                sameCount = 0;
            }
            lastPos = lineStart;
        }
        
        qDebug() << QString("🎯 mmap binary search completed in %1 iterations, found position %2")
                    .arg(iterations).arg(bestPos);
        
        return bestPos;
    }
    
    // Find line start in memory-mapped data
    static size_t findLineStart(const char* data, size_t fileSize, size_t position)
    {
        if (position == 0) return 0;
        
        // Search backwards for newline
        size_t searchStart = (position > 500) ? position - 500 : 0;
        
        for (size_t i = position; i > searchStart; --i) {
            if (data[i] == '\n' || data[i] == '\r') {
                // Skip any additional newline characters
                size_t lineStart = i + 1;
                while (lineStart < fileSize && (data[lineStart] == '\n' || data[lineStart] == '\r')) {
                    lineStart++;
                }
                return lineStart;
            }
        }
        
        // If no newline found, try going back further
        if (searchStart > 0) {
            return findLineStart(data, fileSize, searchStart);
        }
        
        return 0; // Beginning of file
    }
    
    // Read declination from memory-mapped data
    static double readDeclinationAtPosition(const char* data, size_t fileSize, size_t position)
    {
        if (position >= fileSize) return -999.0;
        
        // Find end of line
        size_t lineEnd = position;
        while (lineEnd < fileSize && data[lineEnd] != '\n' && data[lineEnd] != '\r') {
            lineEnd++;
        }
        
        if (lineEnd == position) return -999.0;
        
        // Extract line and parse
        QString line = QString::fromUtf8(data + position, lineEnd - position);
        QStringList parts = line.split('|');
        
        if (parts.size() < 2) return -999.0;
        
        bool ok;
        double dec = parts[1].toDouble(&ok);
        return ok ? dec : -999.0;
    }
    
public:
    // Quick method to check if catalog is available
    static bool isAvailable() {
        return !s_catalogPath.isEmpty() && QFile::exists(s_catalogPath);
    }
    
    // Get catalog file info
    static QString getCatalogInfo() {
        if (!isAvailable()) {
            return "2MASS catalog not available";
        }
        
        QFileInfo info(s_catalogPath);
        return QString("2MASS catalog: %1 (%2 MB, mmap-optimized)")
               .arg(info.fileName())
               .arg(info.size() / (1024 * 1024));
    }
};

#endif // LOCAL2MASS_CATALOG_MMAP_H
