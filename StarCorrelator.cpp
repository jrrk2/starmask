#include "StarCorrelator.h"

// Implementation
StarCorrelator::StarCorrelator(QObject *parent)
    : QObject(parent)
{
}

void StarCorrelator::loadDetectedStarsFromLog(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open file:" << filename;
        return;
    }
    
    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.contains("\"Star ") && line.contains("pos=")) {
            parseDetectedStar(line);
        }
    }
    
    qDebug() << "Loaded" << m_detectedStars.size() << "detected stars";
}

void StarCorrelator::loadCatalogStarsFromLog(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open file:" << filename;
        return;
    }
    
    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.contains("\"Star GDR3_") && line.contains("pos=")) {
            parseCatalogStar(line);
        }
    }
    
    qDebug() << "Loaded" << m_catalogStars.size() << "catalog stars";
}

void StarCorrelator::addDetectedStar(int id, double x, double y, double flux, double area, double radius, double snr)
{
    m_detectedStars.append(DetectedStar(id, x, y, flux, area, radius, snr));
}

void StarCorrelator::addCatalogStar(const QString& gaia_id, double x, double y, double magnitude)
{
    m_catalogStars.append(CorrCatalogStar(gaia_id, x, y, magnitude));
}

double StarCorrelator::magnitudeToFlux(double magnitude) const
{
    return qPow(10.0, (m_zeroPoint - magnitude) / 2.5);
}

double StarCorrelator::fluxToMagnitude(double flux) const
{
    if (flux <= 0) return 99.0;
    return m_zeroPoint - 2.5 * qLn(flux) / qLn(10.0);
}

void StarCorrelator::calibrateZeroPoint()
{
    if (m_matches.isEmpty()) {
        qDebug() << "No matches available for zero point calibration";
        return;
    }
    
    QVector<double> calculatedZps;
    
    for (const auto& match : m_matches) {
        if (match.flux > 0) {
            double calcZp = match.magnitude + 2.5 * qLn(match.flux) / qLn(10.0);
            calculatedZps.append(calcZp);
        }
    }
    
    if (!calculatedZps.isEmpty()) {
        std::sort(calculatedZps.begin(), calculatedZps.end());
        double medianZp = calculatedZps[calculatedZps.size() / 2];
        
        qDebug() << "=== ZERO POINT CALIBRATION ===";
        qDebug() << "Current zero point:" << QString::number(m_zeroPoint, 'f', 2);
        qDebug() << "Calculated zero point (median):" << QString::number(medianZp, 'f', 2);
        
        if (m_autoCalibrate) {
            m_zeroPoint = medianZp;
            qDebug() << "Auto-calibrated to:" << QString::number(m_zeroPoint, 'f', 2);
            updateMatchFluxData();
        }
    }
}

void StarCorrelator::updateMatchFluxData()
{
    for (auto& match : m_matches) {
        match.predicted_flux = magnitudeToFlux(match.magnitude);
        if (match.predicted_flux > 0) {
            match.flux_ratio = match.flux / match.predicted_flux;
        } else {
            match.flux_ratio = 0.0;
        }
        match.mag_diff = fluxToMagnitude(match.flux) - match.magnitude;
    }
}

void StarCorrelator::correlateStars()
{
    m_matches.clear();
    
    // Reset match flags
    for (auto& star : m_detectedStars) star.matched = false;
    for (auto& star : m_catalogStars) star.matched = false;
    
    // Use greedy matching
    QVector<QPair<double, QPair<int, int>>> distancePairs;
    
    for (int i = 0; i < m_detectedStars.size(); i++) {
        const auto& detected = m_detectedStars[i];
        
        for (int j = 0; j < m_catalogStars.size(); j++) {
            const auto& catalog = m_catalogStars[j];
            
            if (catalog.x < 0 || catalog.x >= m_imageWidth || 
                catalog.y < 0 || catalog.y >= m_imageHeight) {
                continue;
            }
            
            double dx = detected.x - catalog.x;
            double dy = detected.y - catalog.y;
            double distance = qSqrt(dx*dx + dy*dy);
            
            if (distance < m_matchThreshold) {
                distancePairs.append({distance, {i, j}});
            }
        }
    }
    
    std::sort(distancePairs.begin(), distancePairs.end());
    
    for (const auto& pair : distancePairs) {
        double distance = pair.first;
        int detIdx = pair.second.first;
        int catIdx = pair.second.second;
        
        if (m_detectedStars[detIdx].matched || m_catalogStars[catIdx].matched) {
            continue;
        }
        
        CorrStarMatch match;
        match.detected_id = m_detectedStars[detIdx].id;
        match.catalog_id = m_catalogStars[catIdx].gaia_id;
        match.distance = distance;
        match.detected_x = m_detectedStars[detIdx].x;
        match.detected_y = m_detectedStars[detIdx].y;
        match.catalog_x = m_catalogStars[catIdx].x;
        match.catalog_y = m_catalogStars[catIdx].y;
        match.magnitude = m_catalogStars[catIdx].magnitude;
        match.flux = m_detectedStars[detIdx].flux;
        match.predicted_flux = magnitudeToFlux(m_catalogStars[catIdx].magnitude);
        
        if (match.predicted_flux > 0) {
            match.flux_ratio = match.flux / match.predicted_flux;
        } else {
            match.flux_ratio = 0.0;
        }
        
        match.mag_diff = fluxToMagnitude(match.flux) - match.magnitude;
        
        m_matches.append(match);
        m_detectedStars[detIdx].matched = true;
        m_catalogStars[catIdx].matched = true;
    }
    
    // Auto-calibrate zero point if enabled
    if (m_autoCalibrate && !m_matches.isEmpty()) {
        calibrateZeroPoint();
    }
}

void StarCorrelator::printDetailedStatistics() const
{
    qDebug() << "=== DETAILED CORRELATION STATISTICS ===";
    qDebug() << "Image dimensions:" << QString::number(m_imageWidth) << "x" << QString::number(m_imageHeight);
    qDebug() << "Match threshold:" << QString::number(m_matchThreshold) << "pixels";
    qDebug() << "Current zero point:" << QString::number(m_zeroPoint, 'f', 2);
    qDebug() << "";
    
    qDebug() << "Detected stars:" << m_detectedStars.size();
    qDebug() << "Catalog stars total:" << m_catalogStars.size();
    
    int catalogInBounds = 0;
    for (const auto& cat : m_catalogStars) {
        if (cat.x >= 0 && cat.x < m_imageWidth && cat.y >= 0 && cat.y < m_imageHeight) {
            catalogInBounds++;
        }
    }
    qDebug() << "Catalog stars in image bounds:" << catalogInBounds;
    qDebug() << "Matches found:" << m_matches.size();
    
    if (!m_matches.isEmpty()) {
        double totalError = 0, maxError = 0, minError = 1e6;
        for (const auto& match : m_matches) {
            totalError += match.distance;
            maxError = qMax(maxError, match.distance);
            minError = qMin(minError, match.distance);
        }
        
        double avgError = totalError / m_matches.size();
        qDebug() << "Position accuracy:";
        qDebug() << "  Average error:" << QString::number(avgError, 'f', 2) << "pixels";
        qDebug() << "  Min/Max error:" << QString::number(minError, 'f', 2) << "/" << QString::number(maxError, 'f', 2) << "pixels";
        
        double matchRateDetected = (double)m_matches.size() / m_detectedStars.size() * 100.0;
        double matchRateCatalog = (double)m_matches.size() / catalogInBounds * 100.0;
        qDebug() << "Match rates:";
        qDebug() << "  Of detected stars:" << QString::number(matchRateDetected, 'f', 1) << "%";
        qDebug() << "  Of catalog stars in bounds:" << QString::number(matchRateCatalog, 'f', 1) << "%";
    }
}

void StarCorrelator::printMatchDetails() const
{
    if (m_matches.isEmpty()) return;
    
    qDebug() << "=== DETAILED MATCH ANALYSIS ===";
    qDebug() << "ID | Det(x,y)     | Cat(x,y)     | Err | Cat.Mag | Det.Mag | Flux  | Ratio  | ΔMag";
    qDebug() << "---+--------------+--------------+-----+---------+---------+-------+--------+-----";
    
    for (const auto& match : m_matches) {
        double detMag = fluxToMagnitude(match.flux);
        QString line = QString("%1 | (%2,%3) | (%4,%5) | %6 | %7 | %8 | %9 | %10 | %11")
                      .arg(match.detected_id, 2)
                      .arg(match.detected_x, 4, 'f', 1)
                      .arg(match.detected_y, 4, 'f', 1)
                      .arg(match.catalog_x, 4, 'f', 1)
                      .arg(match.catalog_y, 4, 'f', 1)
                      .arg(match.distance, 3, 'f', 1)
                      .arg(match.magnitude, 7, 'f', 1)
                      .arg(detMag, 7, 'f', 1)
                      .arg(match.flux, 5, 'f', 0)
                      .arg(match.flux_ratio, 6, 'f', 3)
                      .arg(match.mag_diff, 4, 'f', 1);
        qDebug() << line;
    }
}

void StarCorrelator::analyzePhotometricAccuracy() const
{
    if (m_matches.isEmpty()) {
        qDebug() << "=== No matches for photometric analysis ===";
        return;
    }
    
    qDebug() << "=== PHOTOMETRIC ACCURACY ANALYSIS ===";
    
    QVector<double> fluxRatios, magDiffs;
    double sumFluxRatio = 0, sumMagDiff = 0;
    
    for (const auto& match : m_matches) {
        if (match.flux_ratio > 0 && match.flux_ratio < 1000) {
            fluxRatios.append(match.flux_ratio);
            sumFluxRatio += match.flux_ratio;
        }
        if (qAbs(match.mag_diff) < 10) {
            magDiffs.append(match.mag_diff);
            sumMagDiff += match.mag_diff;
        }
    }
    
    if (!fluxRatios.isEmpty()) {
        std::sort(fluxRatios.begin(), fluxRatios.end());
        double medianFluxRatio = fluxRatios[fluxRatios.size() / 2];
        double meanFluxRatio = sumFluxRatio / fluxRatios.size();
        
        qDebug() << "Flux ratio statistics (observed/predicted):";
        qDebug() << "  Count:" << fluxRatios.size();
        qDebug() << "  Mean:" << QString::number(meanFluxRatio, 'f', 3);
        qDebug() << "  Median:" << QString::number(medianFluxRatio, 'f', 3);
        qDebug() << "  Range:" << QString::number(fluxRatios.first(), 'f', 3) << "-" << QString::number(fluxRatios.last(), 'f', 3);
    }
    
    if (!magDiffs.isEmpty()) {
        std::sort(magDiffs.begin(), magDiffs.end());
        double medianMagDiff = magDiffs[magDiffs.size() / 2];
        double meanMagDiff = sumMagDiff / magDiffs.size();
        
        qDebug() << "";
        qDebug() << "Magnitude difference statistics (detected - catalog):";
        qDebug() << "  Count:" << magDiffs.size();
        qDebug() << "  Mean:" << QString::number(meanMagDiff, 'f', 3);
        qDebug() << "  Median:" << QString::number(medianMagDiff, 'f', 3);
        qDebug() << "  Range:" << QString::number(magDiffs.first(), 'f', 3) << "-" << QString::number(magDiffs.last(), 'f', 3);
    }
}

void StarCorrelator::exportMatches(const QString& filename) const
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "Failed to create export file:" << filename;
        return;
    }
    
    QTextStream out(&file);
    out << "detected_id,detected_x,detected_y,catalog_id,catalog_x,catalog_y,distance,magnitude,flux,predicted_flux,flux_ratio,mag_diff\n";
    
    for (const auto& match : m_matches) {
        out << match.detected_id << ","
            << match.detected_x << "," << match.detected_y << ","
            << match.catalog_id << ","
            << match.catalog_x << "," << match.catalog_y << ","
            << match.distance << "," << match.magnitude << ","
            << match.flux << "," << match.predicted_flux << ","
            << match.flux_ratio << "," << match.mag_diff << "\n";
    }
    
    qDebug() << "Matches exported to" << filename;
}

double StarCorrelator::getAverageError() const
{
    if (m_matches.isEmpty()) return 0.0;
    
    double totalError = 0;
    for (const auto& match : m_matches) {
        totalError += match.distance;
    }
    return totalError / m_matches.size();
}

double StarCorrelator::getMatchRate() const
{
    if (m_detectedStars.isEmpty()) return 0.0;
    return (double)m_matches.size() / m_detectedStars.size() * 100.0;
}

// Private parsing methods
void StarCorrelator::parseDetectedStar(const QString& line)
{
    QRegularExpression starRegex("Star (\\d+):");
    auto match = starRegex.match(line);
    if (!match.hasMatch()) return;
    
    int id = match.captured(1).toInt();
    
    double x, y, flux, area, radius, snr;
    if (parseStarData(line, x, y, flux, area, radius, snr)) {
        addDetectedStar(id, x, y, flux, area, radius, snr);
    }
}

void StarCorrelator::parseCatalogStar(const QString& line)
{
    QRegularExpression gdr3Regex("(GDR3_\\w+):");
    auto match = gdr3Regex.match(line);
    if (!match.hasMatch()) return;
    
    QString gaiaId = match.captured(1);
    
    QRegularExpression posRegex("pos=\\(([-\\d.]+),([-\\d.]+)\\)");
    QRegularExpression magRegex("magnitude=([-\\d.]+)");
    
    auto posMatch = posRegex.match(line);
    auto magMatch = magRegex.match(line);
    
    if (posMatch.hasMatch() && magMatch.hasMatch()) {
        double x = posMatch.captured(1).toDouble();
        double y = posMatch.captured(2).toDouble();
        double magnitude = magMatch.captured(1).toDouble();
        
        addCatalogStar(gaiaId, x, y, magnitude);
    }
}

bool StarCorrelator::parseStarData(const QString& line, double& x, double& y, double& flux, double& area, double& radius, double& snr)
{
    QRegularExpression posRegex("pos=\\(([-\\d.]+),([-\\d.]+)\\)");
    QRegularExpression fluxRegex("flux=([-\\d.]+)");
    QRegularExpression areaRegex("area=([-\\d.]+)");
    QRegularExpression radiusRegex("radius=([-\\d.]+)");
    QRegularExpression snrRegex("SNR=([-\\d.]+)");
    
    auto posMatch = posRegex.match(line);
    auto fluxMatch = fluxRegex.match(line);
    auto areaMatch = areaRegex.match(line);
    auto radiusMatch = radiusRegex.match(line);
    auto snrMatch = snrRegex.match(line);
    
    if (posMatch.hasMatch() && fluxMatch.hasMatch() && areaMatch.hasMatch() && 
        radiusMatch.hasMatch() && snrMatch.hasMatch()) {
        
        x = posMatch.captured(1).toDouble();
        y = posMatch.captured(2).toDouble();
        flux = fluxMatch.captured(1).toDouble();
        area = areaMatch.captured(1).toDouble();
        radius = radiusMatch.captured(1).toDouble();
        snr = snrMatch.captured(1).toDouble();
        
        return true;
    }
    
    return false;
}

QString StarCorrelator::extractValue(const QString& line, int start) const
{
    int end = line.indexOf(" ", start);
    if (end == -1) end = line.length();
    return line.mid(start, end - start);
}
