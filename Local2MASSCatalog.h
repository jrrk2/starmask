#ifndef LOCAL2MASS_CATALOG_OPTIMIZED_H
#define LOCAL2MASS_CATALOG_OPTIMIZED_H

#include <QDebug>
#include <QString>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QTime>
#include <cmath>
#include <algorithm>

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
        
        QFile file(s_catalogPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qDebug() << "❌ Failed to open 2MASS catalog:" << s_catalogPath;
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
        
        // Binary search to find the start position
        qint64 startPos = findDeclinationStart(file, minDec);
        
        if (startPos < 0) {
            qDebug() << "❌ Binary search failed to find start position";
            return stars;
        }
        
        // Seek to the start position
        file.seek(startPos);
        QTextStream in(&file);
        
        qDebug() << QString("📊 Binary search complete, starting sequential read from position %1").arg(startPos);
        
        // Now read sequentially until we pass maxDec
        QString line;
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
        
        while (!in.atEnd()) {
            line = in.readLine().trimmed();
            totalLines++;
            
            if (line.isEmpty()) continue;
            
            // Parse line format: RA|Dec|ID|magnitude
            QStringList parts = line.split('|');
            if (parts.size() < 4) continue;
            
            bool raOk, decOk, magOk;
            double ra = parts[0].toDouble(&raOk);
            double dec = parts[1].toDouble(&decOk);
            QString id = parts[2].trimmed();
            double magnitude = parts[3].toDouble(&magOk);
            
            if (!raOk || !decOk || !magOk) continue;
            
            // Early exit if we've passed the maximum declination
            if (dec > maxDec) {
                qDebug() << QString("📈 Reached max declination %1° at line %2, stopping search")
                            .arg(dec, 0, 'f', 4).arg(totalLines);
                break;
            }
            
            // Skip if still below minimum declination (shouldn't happen with good binary search)
            if (dec < minDec) continue;
            
            // Quick RA bounds check
            bool inRABounds;
            if (wraparound) {
                inRABounds = (ra >= minRA || ra <= maxRA);
            } else {
                inRABounds = (ra >= minRA && ra <= maxRA);
            }
            
            if (!inRABounds) continue;
            
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
            
            // Progress update for very large result sets
            if (totalLines % 50000 == 0) {
                qDebug() << QString("📊 Processed %1 lines, found %2 stars in region")
                            .arg(totalLines).arg(starsInRegion);
            }
        }
        
        auto elapsed = startTime.msecsTo(QTime::currentTime());
        
        qDebug() << QString("✅ 2MASS catalog search completed in %1ms").arg(elapsed);
        qDebug() << QString("📈 Statistics:");
        qDebug() << QString("   - Lines processed: %1").arg(totalLines);
        qDebug() << QString("   - Stars in region: %2").arg(starsInRegion);
        qDebug() << QString("   - Stars within magnitude limit: %3").arg(starsWithinMagnitude);
        qDebug() << QString("   - Final catalog size: %4").arg(stars.size());
        qDebug() << QString("   - Performance improvement: ~%1x faster")
                    .arg(estimatePerformanceImprovement(file.size(), totalLines));
        
        // Sort by magnitude (brightest first)
        std::sort(stars.begin(), stars.end(), 
                  [](const Star& a, const Star& b) {
                      return a.magnitude < b.magnitude;
                  });
        
        return stars;
    }
    
    // Binary search to find the first line with declination >= targetDec
    static qint64 findDeclinationStart(QFile& file, double targetDec)
    {
        qint64 fileSize = file.size();
        qint64 left = 0;
        qint64 right = fileSize;
        qint64 bestPos = 0;
        
        qDebug() << QString("🔍 Binary search for Dec >= %1° in %2 MB file")
                    .arg(targetDec, 0, 'f', 4).arg(fileSize / (1024 * 1024));
        
        int iterations = 0;
        const int maxIterations = 35; // log2(20GB) ≈ 34 iterations max
        
        while (left < right && iterations < maxIterations) {
            iterations++;
            qint64 mid = left + (right - left) / 2;
            
            // Find the start of a line near this position
            qint64 lineStart = findLineStart(file, mid);
            if (lineStart < 0) {
                qDebug() << QString("⚠️  Could not find line start near position %1").arg(mid);
                left = mid + 1;
                continue;
            }
            
            // Read the declination from this line
            double dec = readDeclinationAtPosition(file, lineStart);
            if (dec == -999.0) {
                qDebug() << QString("⚠️  Could not read declination at position %1").arg(lineStart);
                left = mid + 1;
                continue;
            }
            
            // Only log every few iterations to reduce noise
            if (iterations <= 10 || iterations % 5 == 0) {
                qDebug() << QString("   Iteration %1: pos=%2, dec=%3° (target=%4°)")
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
            static qint64 lastPos = -1;
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
        
        qDebug() << QString("🎯 Binary search completed in %1 iterations, found position %2")
                    .arg(iterations).arg(bestPos);
        
        return bestPos;
    }
    
    // Find the start of a line at or before the given position
    static qint64 findLineStart(QFile& file, qint64 position)
    {
        if (position <= 0) return 0;
        
        // Read backwards to find the previous newline
        qint64 searchStart = std::max(0LL, position - 500); // Look back up to 500 chars
        file.seek(searchStart);
        
        QByteArray chunk = file.read(position - searchStart + 200);
        if (chunk.isEmpty()) return -1;
        
        // Find the last complete line start in this chunk
        int lastNewline = chunk.lastIndexOf('\n', chunk.size() - (position - searchStart) - 1);
        
        if (lastNewline >= 0) {
            return searchStart + lastNewline + 1;
        } else {
            // No newline found, try going back further
            if (searchStart > 0) {
                return findLineStart(file, searchStart);
            }
            return 0; // Beginning of file
        }
    }
    
    // Read just the declination from a line at the given position
    static double readDeclinationAtPosition(QFile& file, qint64 position)
    {
        file.seek(position);
        QTextStream in(&file);
        
        QString line = in.readLine();
        if (line.isEmpty()) return -999.0;
        
        QStringList parts = line.split('|');
        if (parts.size() < 2) return -999.0;
        
        bool ok;
        double dec = parts[1].toDouble(&ok);
        return ok ? dec : -999.0;
    }
    
    // Estimate performance improvement
    static int estimatePerformanceImprovement(qint64 fileSize, int linesProcessed)
    {
        // Rough estimate: if we processed N lines out of a file that would have ~fileSize/avgLineLength total lines
        const int avgLineLength = 50; // Estimate
        qint64 estimatedTotalLines = fileSize / avgLineLength;
        
        if (linesProcessed > 0 && estimatedTotalLines > linesProcessed) {
            return static_cast<int>(estimatedTotalLines / linesProcessed);
        }
        return 1;
    }
    
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
        return QString("2MASS catalog: %1 (%2 MB)")
               .arg(info.fileName())
               .arg(info.size() / (1024 * 1024));
    }
    
    // Test method to verify catalog ordering
    static void verifyCatalogOrdering(int samplesToCheck = 1000)
    {
        if (!isAvailable()) {
            qDebug() << "❌ Catalog not available for verification";
            return;
        }
        
        QFile file(s_catalogPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qDebug() << "❌ Failed to open catalog for verification";
            return;
        }
        
        QTextStream in(&file);
        QString line;
        double lastDec = -90.0;
        bool isOrdered = true;
        int lineCount = 0;
        int violations = 0;
        
        qDebug() << QString("🔍 Verifying catalog ordering (checking %1 samples)...").arg(samplesToCheck);
        
        while (!in.atEnd() && lineCount < samplesToCheck) {
            line = in.readLine().trimmed();
            lineCount++;
            
            if (line.isEmpty()) continue;
            
            QStringList parts = line.split('|');
            if (parts.size() < 2) continue;
            
            bool ok;
            double dec = parts[1].toDouble(&ok);
            if (!ok) continue;
            
            if (dec < lastDec) {
                if (violations == 0) {
                    qDebug() << QString("❌ Ordering violation at line %1: %.6f° < %.6f°")
                                .arg(lineCount).arg(dec).arg(lastDec);
                }
                violations++;
                isOrdered = false;
            }
            
            lastDec = dec;
            
            if (lineCount % 100 == 0) {
                qDebug() << QString("   Checked %1 lines, current Dec: %.4f°").arg(lineCount).arg(dec);
            }
        }
        
        if (isOrdered) {
            qDebug() << QString("✅ Catalog is properly ordered by declination (%1 samples checked)")
                        .arg(lineCount);
            qDebug() << QString("   Dec range in sample: -90° to %.4f°").arg(lastDec);
        } else {
            qDebug() << QString("❌ Catalog has %1 ordering violations in %2 samples")
                        .arg(violations).arg(lineCount);
            qDebug() << "   Binary search optimization may not work correctly!";
        }
    }
};

#endif // LOCAL2MASS_CATALOG_OPTIMIZED_H