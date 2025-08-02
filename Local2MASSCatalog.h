// Add this class to handle local 2MASS catalog loading

#include <QDebug>
#include <QString>
#include <QFile>
#include <QFileInfo>

class Local2MASSCatalog
{
public:
    struct Star {
        double ra, dec, magnitude;
        QString id;
        bool isValid = true;
    };
    
    static QString s_catalogPath;  // Global path to catalog file
    
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
        qDebug() << QString("🎯 Query region: RA=%.4f° Dec=%.4f° radius=%.2f° mag≤%.1f")
                    .arg(centerRA).arg(centerDec).arg(radiusDegrees).arg(maxMagnitude);
        
        QTextStream in(&file);
        QString line;
        int totalLines = 0;
        int starsInRegion = 0;
        int starsWithinMagnitude = 0;
        
        // Calculate search bounds for efficiency
        double minRA = centerRA - radiusDegrees;
        double maxRA = centerRA + radiusDegrees;
        double minDec = centerDec - radiusDegrees;
        double maxDec = centerDec + radiusDegrees;
        
        // Handle RA wraparound at 0/360 degrees
        bool wraparound = (minRA < 0 || maxRA > 360);
        if (minRA < 0) minRA += 360;
        if (maxRA > 360) maxRA -= 360;
        
        qDebug() << QString("📍 Search bounds: RA=[%.2f,%.2f] Dec=[%.2f,%.2f] %3")
                    .arg(minRA).arg(maxRA).arg(minDec).arg(maxDec)
                    .arg(wraparound ? "(wraparound)" : "");
        
        auto startTime = QTime::currentTime();
        
        while (!in.atEnd()) {
            line = in.readLine().trimmed();
            totalLines++;
            
            if (line.isEmpty()) continue;
            
            // Parse line format: RA|Dec|ID |magnitude
            QStringList parts = line.split('|');
            if (parts.size() < 4) continue;
            
            bool raOk, decOk, magOk;
            double ra = parts[0].toDouble(&raOk);
            double dec = parts[1].toDouble(&decOk);
            QString id = parts[2].trimmed();
            double magnitude = parts[3].toDouble(&magOk);
            
            if (!raOk || !decOk || !magOk) continue;
            
            // Quick bounds check first (for efficiency)
            bool inRABounds;
            if (wraparound) {
                inRABounds = (ra >= minRA || ra <= maxRA);
            } else {
                inRABounds = (ra >= minRA && ra <= maxRA);
            }
            
            if (!inRABounds || dec < minDec || dec > maxDec) continue;
            
            // More precise distance check
            double ra_diff = ra - centerRA;
            double dec_diff = dec - centerDec;
            
            // Handle RA wraparound
            if (ra_diff > 180) ra_diff -= 360;
            if (ra_diff < -180) ra_diff += 360;
            
            double cos_dec = cos(centerDec * M_PI / 180.0);
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
            
            // Progress update for large files
            if (totalLines % 100000 == 0) {
                qDebug() << QString("📊 Processed %1 lines, found %2 stars in region")
                            .arg(totalLines).arg(starsInRegion);
            }
        }
        
        auto elapsed = startTime.msecsTo(QTime::currentTime());
        
        qDebug() << QString("✅ 2MASS catalog search completed in %1ms").arg(elapsed);
        qDebug() << QString("📈 Statistics:");
        qDebug() << QString("   - Total lines processed: %1").arg(totalLines);
        qDebug() << QString("   - Stars in region: %2").arg(starsInRegion);
        qDebug() << QString("   - Stars within magnitude limit: %3").arg(starsWithinMagnitude);
        qDebug() << QString("   - Final catalog size: %4").arg(stars.size());
        
        // Sort by magnitude (brightest first)
        std::sort(stars.begin(), stars.end(), 
                  [](const Star& a, const Star& b) {
                      return a.magnitude < b.magnitude;
                  });
        
        return stars;
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
};
