// PhotometryComparisonWidget.cpp - Specialized widget for photometry visualization

#include "StarChartWidget.h"
#include "StarStatisticsChartDialog.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QToolTip>
#include <QtMath>
#include <algorithm>

PhotometryComparisonWidget::PhotometryComparisonWidget(QWidget* parent)
    : QWidget(parent)
    , m_plotMode(0)
    , m_colorScheme(0)
    , m_zoomCenter(0, 0)
    , m_zoomFactor(1.0)
{
    setMinimumSize(400, 300);
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void PhotometryComparisonWidget::setPhotometryData(const QVector<PhotometryMatch>& matches)
{
    m_matches = matches;
    update();
}

void PhotometryComparisonWidget::setPlotMode(int mode)
{
    m_plotMode = mode;
    update();
}

void PhotometryComparisonWidget::setColorScheme(int scheme)
{
    m_colorScheme = scheme;
    update();
}

void PhotometryComparisonWidget::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Clear background
    painter.fillRect(rect(), QColor(25, 25, 35));
    
    if (m_matches.isEmpty()) {
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter, "No photometry data available");
        return;
    }
    
    // Draw based on current plot mode
    switch (m_plotMode) {
        case 0: drawMagnitudeComparison(painter); break;
        case 1: drawFluxRatioPlot(painter); break;
        case 2: drawResidualsPlot(painter); break;
        case 3: drawCalibrationCurve(painter); break;
        default: drawMagnitudeComparison(painter); break;
    }
    
    // Draw legend
    drawLegend(painter);
}

void PhotometryComparisonWidget::drawMagnitudeComparison(QPainter& painter)
{
    // Set up coordinate system
    const int margin = 60;
    QRect plotRect = rect().adjusted(margin, margin, -margin, -margin);
    
    if (plotRect.width() <= 0 || plotRect.height() <= 0) return;
    
    // Find data ranges
    double minCatalogMag = std::numeric_limits<double>::max();
    double maxCatalogMag = std::numeric_limits<double>::lowest();
    double minDetectedMag = std::numeric_limits<double>::max();
    double maxDetectedMag = std::numeric_limits<double>::lowest();
    
    for (const auto& match : m_matches) {
        minCatalogMag = std::min(minCatalogMag, match.catalogMagnitude);
        maxCatalogMag = std::max(maxCatalogMag, match.catalogMagnitude);
        minDetectedMag = std::min(minDetectedMag, match.calculatedMagnitude);
        maxDetectedMag = std::max(maxDetectedMag, match.calculatedMagnitude);
    }
    
    // Add padding to ranges
    double magRangeCat = maxCatalogMag - minCatalogMag;
    double magRangeDet = maxDetectedMag - minDetectedMag;
    minCatalogMag -= magRangeCat * 0.1;
    maxCatalogMag += magRangeCat * 0.1;
    minDetectedMag -= magRangeDet * 0.1;
    maxDetectedMag += magRangeDet * 0.1;
    
    // Use same range for both axes to show 1:1 line clearly
    double minMag = std::min(minCatalogMag, minDetectedMag);
    double maxMag = std::max(maxCatalogMag, maxDetectedMag);
    
    // Draw axes
    painter.setPen(QPen(Qt::lightGray, 1));
    painter.drawRect(plotRect);
    
    // Draw grid
    painter.setPen(QPen(QColor(60, 60, 70), 1));
    for (int i = 1; i < 10; ++i) {
        double frac = i / 10.0;
        int x = plotRect.left() + frac * plotRect.width();
        int y = plotRect.top() + frac * plotRect.height();
        painter.drawLine(x, plotRect.top(), x, plotRect.bottom());
        painter.drawLine(plotRect.left(), y, plotRect.right(), y);
    }
    
    // Draw 1:1 line (perfect correlation)
    painter.setPen(QPen(Qt::white, 2, Qt::DashLine));
    painter.drawLine(plotRect.topLeft(), plotRect.bottomRight());
    
    // Draw data points
    for (const auto& match : m_matches) {
        // Transform coordinates
        double xFrac = (match.catalogMagnitude - minMag) / (maxMag - minMag);
        double yFrac = 1.0 - (match.calculatedMagnitude - minMag) / (maxMag - minMag); // Invert Y
        
        int x = plotRect.left() + xFrac * plotRect.width();
        int y = plotRect.top() + yFrac * plotRect.height();
        
        // Choose color based on scheme
        QColor pointColor;
        if (m_colorScheme == 0) { // Quality colors
            if (match.quality == "Excellent") pointColor = QColor(0, 255, 0);
            else if (match.quality == "Good") pointColor = QColor(255, 255, 0);
            else if (match.quality == "Fair") pointColor = QColor(255, 165, 0);
            else pointColor = QColor(255, 0, 0);
        } else if (m_colorScheme == 1) { // SNR colors
            double snr = match.snr;
            if (snr > 50) pointColor = QColor(0, 255, 0);
            else if (snr > 20) pointColor = QColor(128, 255, 0);
            else if (snr > 10) pointColor = QColor(255, 255, 0);
            else if (snr > 5) pointColor = QColor(255, 128, 0);
            else pointColor = QColor(255, 0, 0);
        } else { // Magnitude difference colors
            double absDiff = qAbs(match.magnitudeDifference);
            if (absDiff < 0.1) pointColor = QColor(0, 255, 0);
            else if (absDiff < 0.2) pointColor = QColor(255, 255, 0);
            else if (absDiff < 0.5) pointColor = QColor(255, 165, 0);
            else pointColor = QColor(255, 0, 0);
        }
        
        // Size based on SNR
        double size = 3 + std::min(match.snr / 10.0, 8.0);
        
        painter.setPen(QPen(pointColor.darker(150), 1));
        painter.setBrush(pointColor);
        painter.drawEllipse(QPointF(x, y), size/2, size/2);
    }
    
    // Draw axes labels
    painter.setPen(Qt::white);
    QFont labelFont = painter.font();
    labelFont.setPointSize(10);
    painter.setFont(labelFont);
    
    // X-axis label
    painter.drawText(QRect(plotRect.left(), plotRect.bottom() + 10, 
                          plotRect.width(), 30), 
                    Qt::AlignCenter, "Catalog Magnitude");
    
    // Y-axis label (rotated)
    painter.save();
    painter.translate(margin - 40, plotRect.center().y());
    painter.rotate(-90);
    painter.drawText(QRect(-50, -10, 100, 20), Qt::AlignCenter, "Detected Magnitude");
    painter.restore();
    
    // Draw tick marks and values
    painter.setPen(Qt::lightGray);
    QFont tickFont = painter.font();
    tickFont.setPointSize(8);
    painter.setFont(tickFont);
    
    // X-axis ticks
    for (int i = 0; i <= 10; ++i) {
        double frac = i / 10.0;
        int x = plotRect.left() + frac * plotRect.width();
        painter.drawLine(x, plotRect.bottom(), x, plotRect.bottom() + 5);
        
        double magValue = minMag + frac * (maxMag - minMag);
        painter.drawText(QRect(x - 20, plotRect.bottom() + 8, 40, 15), 
                        Qt::AlignCenter, QString::number(magValue, 'f', 1));
    }
    
    // Y-axis ticks
    for (int i = 0; i <= 10; ++i) {
        double frac = i / 10.0;
        int y = plotRect.bottom() - frac * plotRect.height();
        painter.drawLine(plotRect.left() - 5, y, plotRect.left(), y);
        
        double magValue = minMag + frac * (maxMag - minMag);
        painter.drawText(QRect(plotRect.left() - 45, y - 8, 35, 16), 
                        Qt::AlignRight | Qt::AlignVCenter, QString::number(magValue, 'f', 1));
    }
    
    // Draw title
    painter.setPen(Qt::white);
    QFont titleFont = painter.font();
    titleFont.setPointSize(12);
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.drawText(QRect(0, 10, width(), 30), Qt::AlignCenter, 
                    "Detected vs Catalog Magnitude Comparison");
}

void PhotometryComparisonWidget::drawFluxRatioPlot(QPainter& painter)
{
    const int margin = 60;
    QRect plotRect = rect().adjusted(margin, margin, -margin, -margin);
    
    if (plotRect.width() <= 0 || plotRect.height() <= 0) return;
    
    // Find data ranges
    double minMag = std::numeric_limits<double>::max();
    double maxMag = std::numeric_limits<double>::lowest();
    double minRatio = std::numeric_limits<double>::max();
    double maxRatio = std::numeric_limits<double>::lowest();
    
    for (const auto& match : m_matches) {
        minMag = std::min(minMag, match.catalogMagnitude);
        maxMag = std::max(maxMag, match.catalogMagnitude);
        
        // Filter extreme ratios
        if (match.fluxRatio > 0.01 && match.fluxRatio < 100.0) {
            minRatio = std::min(minRatio, match.fluxRatio);
            maxRatio = std::max(maxRatio, match.fluxRatio);
        }
    }
    
    // Add padding
    double magRange = maxMag - minMag;
    minMag -= magRange * 0.05;
    maxMag += magRange * 0.05;
    
    // Use log scale for flux ratio
    double logMinRatio = std::log10(std::max(minRatio, 0.01));
    double logMaxRatio = std::log10(std::min(maxRatio, 100.0));
    double logRange = logMaxRatio - logMinRatio;
    logMinRatio -= logRange * 0.1;
    logMaxRatio += logRange * 0.1;
    
    // Draw axes and grid
    painter.setPen(QPen(Qt::lightGray, 1));
    painter.drawRect(plotRect);
    
    painter.setPen(QPen(QColor(60, 60, 70), 1));
    for (int i = 1; i < 10; ++i) {
        double frac = i / 10.0;
        int x = plotRect.left() + frac * plotRect.width();
        int y = plotRect.top() + frac * plotRect.height();
        painter.drawLine(x, plotRect.top(), x, plotRect.bottom());
        painter.drawLine(plotRect.left(), y, plotRect.right(), y);
    }
    
    // Draw ideal flux ratio line (ratio = 1.0)
    double idealLogRatio = 0.0; // log10(1.0) = 0
    if (idealLogRatio >= logMinRatio && idealLogRatio <= logMaxRatio) {
        double yFrac = 1.0 - (idealLogRatio - logMinRatio) / (logMaxRatio - logMinRatio);
        int y = plotRect.top() + yFrac * plotRect.height();
        painter.setPen(QPen(Qt::white, 2, Qt::DashLine));
        painter.drawLine(plotRect.left(), y, plotRect.right(), y);
    }
    
    // Draw data points
    for (const auto& match : m_matches) {
        if (match.fluxRatio <= 0.01 || match.fluxRatio >= 100.0) continue;
        
        // Transform coordinates
        double xFrac = (match.catalogMagnitude - minMag) / (maxMag - minMag);
        double logRatio = std::log10(match.fluxRatio);
        double yFrac = 1.0 - (logRatio - logMinRatio) / (logMaxRatio - logMinRatio);
        
        int x = plotRect.left() + xFrac * plotRect.width();
        int y = plotRect.top() + yFrac * plotRect.height();
        
        // Color by quality
        QColor pointColor;
        if (match.quality == "Excellent") pointColor = QColor(0, 255, 0);
        else if (match.quality == "Good") pointColor = QColor(255, 255, 0);
        else if (match.quality == "Fair") pointColor = QColor(255, 165, 0);
        else pointColor = QColor(255, 0, 0);
        
        double size = 3 + std::min(match.snr / 10.0, 6.0);
        
        painter.setPen(QPen(pointColor.darker(150), 1));
        painter.setBrush(pointColor);
        painter.drawEllipse(QPointF(x, y), size/2, size/2);
    }
    
    // Labels and ticks
    painter.setPen(Qt::white);
    QFont labelFont = painter.font();
    labelFont.setPointSize(10);
    painter.setFont(labelFont);
    
    painter.drawText(QRect(plotRect.left(), plotRect.bottom() + 10, 
                          plotRect.width(), 30), 
                    Qt::AlignCenter, "Catalog Magnitude");
    
    painter.save();
    painter.translate(margin - 40, plotRect.center().y());
    painter.rotate(-90);
    painter.drawText(QRect(-60, -10, 120, 20), Qt::AlignCenter, "Flux Ratio (Detected/Predicted)");
    painter.restore();
    
    // Draw title
    QFont titleFont = painter.font();
    titleFont.setPointSize(12);
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.drawText(QRect(0, 10, width(), 30), Qt::AlignCenter, 
                    "Flux Ratio vs Catalog Magnitude");
}

void PhotometryComparisonWidget::drawResidualsPlot(QPainter& painter)
{
    const int margin = 60;
    QRect plotRect = rect().adjusted(margin, margin, -margin, -margin);
    
    if (plotRect.width() <= 0 || plotRect.height() <= 0) return;
    
    // Find data ranges
    double minMag = std::numeric_limits<double>::max();
    double maxMag = std::numeric_limits<double>::lowest();
    double minResidual = std::numeric_limits<double>::max();
    double maxResidual = std::numeric_limits<double>::lowest();
    
    for (const auto& match : m_matches) {
        minMag = std::min(minMag, match.catalogMagnitude);
        maxMag = std::max(maxMag, match.catalogMagnitude);
        minResidual = std::min(minResidual, match.magnitudeDifference);
        maxResidual = std::max(maxResidual, match.magnitudeDifference);
    }
    
    // Add padding
    double magRange = maxMag - minMag;
    minMag -= magRange * 0.05;
    maxMag += magRange * 0.05;
    
    double residualRange = maxResidual - minResidual;
    minResidual -= residualRange * 0.1;
    maxResidual += residualRange * 0.1;
    
    // Draw axes and grid
    painter.setPen(QPen(Qt::lightGray, 1));
    painter.drawRect(plotRect);
    
    painter.setPen(QPen(QColor(60, 60, 70), 1));
    for (int i = 1; i < 10; ++i) {
        double frac = i / 10.0;
        int x = plotRect.left() + frac * plotRect.width();
        int y = plotRect.top() + frac * plotRect.height();
        painter.drawLine(x, plotRect.top(), x, plotRect.bottom());
        painter.drawLine(plotRect.left(), y, plotRect.right(), y);
    }
    
    // Draw zero residual line
    if (minResidual <= 0.0 && maxResidual >= 0.0) {
        double yFrac = 1.0 - (0.0 - minResidual) / (maxResidual - minResidual);
        int y = plotRect.top() + yFrac * plotRect.height();
        painter.setPen(QPen(Qt::white, 2, Qt::DashLine));
        painter.drawLine(plotRect.left(), y, plotRect.right(), y);
    }
    
    // Draw data points
    for (const auto& match : m_matches) {
        double xFrac = (match.catalogMagnitude - minMag) / (maxMag - minMag);
        double yFrac = 1.0 - (match.magnitudeDifference - minResidual) / (maxResidual - minResidual);
        
        int x = plotRect.left() + xFrac * plotRect.width();
        int y = plotRect.top() + yFrac * plotRect.height();
        
        // Color by residual magnitude
        QColor pointColor;
        double absResidual = qAbs(match.magnitudeDifference);
        if (absResidual < 0.1) pointColor = QColor(0, 255, 0);
        else if (absResidual < 0.2) pointColor = QColor(255, 255, 0);
        else if (absResidual < 0.5) pointColor = QColor(255, 165, 0);
        else pointColor = QColor(255, 0, 0);
        
        double size = 3 + std::min(match.snr / 15.0, 5.0);
        
        painter.setPen(QPen(pointColor.darker(150), 1));
        painter.setBrush(pointColor);
        painter.drawEllipse(QPointF(x, y), size/2, size/2);
    }
    
    // Labels
    painter.setPen(Qt::white);
    QFont labelFont = painter.font();
    labelFont.setPointSize(10);
    painter.setFont(labelFont);
    
    painter.drawText(QRect(plotRect.left(), plotRect.bottom() + 10, 
                          plotRect.width(), 30), 
                    Qt::AlignCenter, "Catalog Magnitude");
    
    painter.save();
    painter.translate(margin - 40, plotRect.center().y());
    painter.rotate(-90);
    painter.drawText(QRect(-60, -10, 120, 20), Qt::AlignCenter, "Magnitude Residual (Det - Cat)");
    painter.restore();
    
    // Draw title
    QFont titleFont = painter.font();
    titleFont.setPointSize(12);
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.drawText(QRect(0, 10, width(), 30), Qt::AlignCenter, 
                    "Photometry Residuals vs Catalog Magnitude");
}

void PhotometryComparisonWidget::drawCalibrationCurve(QPainter& painter)
{
    // This would show the photometric calibration function
    // For now, show a simplified version
    painter.setPen(Qt::white);
    painter.drawText(rect(), Qt::AlignCenter, 
                    "Calibration Curve\n(Advanced feature - under development)");
}

void PhotometryComparisonWidget::drawLegend(QPainter& painter)
{
    if (m_matches.isEmpty()) return;
    
    const int legendX = width() - 180;
    const int legendY = 30;
    const int legendW = 170;
    const int legendH = 120;
    
    // Draw legend background
    painter.setPen(QPen(Qt::white, 1));
    painter.setBrush(QColor(0, 0, 0, 180));
    painter.drawRect(legendX, legendY, legendW, legendH);
    
    painter.setPen(Qt::white);
    QFont legendFont = painter.font();
    legendFont.setPointSize(9);
    painter.setFont(legendFont);
    
    int yOffset = legendY + 15;
    
    if (m_colorScheme == 0) { // Quality colors
        painter.drawText(legendX + 5, yOffset, "Quality Legend:");
        yOffset += 18;
        
        QList<QPair<QString, QColor>> qualityColors = {
            {"Excellent", QColor(0, 255, 0)},
            {"Good", QColor(255, 255, 0)},
            {"Fair", QColor(255, 165, 0)},
            {"Poor", QColor(255, 0, 0)}
        };
        
        for (const auto& item : qualityColors) {
            painter.setPen(QPen(item.second.darker(150), 1));
            painter.setBrush(item.second);
            painter.drawEllipse(QPointF(legendX + 15, yOffset), 4, 4);
            
            painter.setPen(Qt::white);
            painter.drawText(legendX + 25, yOffset + 3, item.first);
            yOffset += 15;
        }
    } else if (m_colorScheme == 1) { // SNR colors
        painter.drawText(legendX + 5, yOffset, "SNR Legend:");
        yOffset += 18;
        
        QList<QPair<QString, QColor>> snrColors = {
            {">50", QColor(0, 255, 0)},
            {"20-50", QColor(128, 255, 0)},
            {"10-20", QColor(255, 255, 0)},
            {"5-10", QColor(255, 128, 0)},
            {"<5", QColor(255, 0, 0)}
        };
        
        for (const auto& item : snrColors) {
            painter.setPen(QPen(item.second.darker(150), 1));
            painter.setBrush(item.second);
            painter.drawEllipse(QPointF(legendX + 15, yOffset), 4, 4);
            
            painter.setPen(Qt::white);
            painter.drawText(legendX + 25, yOffset + 3, item.first);
            yOffset += 15;
        }
    }
    
    // Show statistics summary
    if (yOffset < legendY + legendH - 20) {
        painter.setPen(Qt::lightGray);
        QFont smallFont = legendFont;
        smallFont.setPointSize(8);
        painter.setFont(smallFont);
        painter.drawText(legendX + 5, yOffset + 10, 
                        QString("Total: %1 matches").arg(m_matches.size()));
    }
}

void PhotometryComparisonWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_zoomCenter = event->position();
    }
}

void PhotometryComparisonWidget::mouseMoveEvent(QMouseEvent* event)
{
    // Find nearest data point for tooltip
    if (m_matches.isEmpty()) return;
    
    const int margin = 60;
    QRect plotRect = rect().adjusted(margin, margin, -margin, -margin);
    
    if (!plotRect.contains(event->position().toPoint())) {
        QToolTip::hideText();
        return;
    }
    
    // Find data ranges (simplified for tooltip)
    double minMag = 999, maxMag = -999;
    for (const auto& match : m_matches) {
        minMag = std::min(minMag, match.catalogMagnitude);
        maxMag = std::max(maxMag, match.catalogMagnitude);
    }
    
    // Find closest point
    double minDist = std::numeric_limits<double>::max();
    const PhotometryMatch* closestMatch = nullptr;
    
    for (const auto& match : m_matches) {
        double xFrac = (match.catalogMagnitude - minMag) / (maxMag - minMag);
        double yFrac = 1.0 - (match.calculatedMagnitude - minMag) / (maxMag - minMag);
        
        int x = plotRect.left() + xFrac * plotRect.width();
        int y = plotRect.top() + yFrac * plotRect.height();
        
        double dist = std::sqrt(std::pow(x - event->position().x(), 2) + 
                               std::pow(y - event->position().y(), 2));
        
        if (dist < minDist && dist < 20) { // Within 20 pixels
            minDist = dist;
            closestMatch = &match;
        }
    }
    
    if (closestMatch) {
        QString tooltip = QString(
            "Star ID: %1\n"
            "Catalog Mag: %2\n"
            "Detected Mag: %3\n"
            "Difference: %4\n"
            "SNR: %5\n"
            "Quality: %6"
        ).arg(closestMatch->detectedStarId)
         .arg(closestMatch->catalogMagnitude, 0, 'f', 2)
         .arg(closestMatch->calculatedMagnitude, 0, 'f', 2)
         .arg(closestMatch->magnitudeDifference, 0, 'f', 3)
         .arg(closestMatch->snr, 0, 'f', 1)
         .arg(closestMatch->quality);
        
        QToolTip::showText(event->globalPosition().toPoint(), tooltip);
    } else {
        QToolTip::hideText();
    }
}

void PhotometryComparisonWidget::wheelEvent(QWheelEvent* event)
{
    // Implement zoom functionality
    const double zoomFactor = 1.2;
    
    if (event->angleDelta().y() > 0) {
        m_zoomFactor *= zoomFactor;
    } else {
        m_zoomFactor /= zoomFactor;
    }
    
    m_zoomFactor = std::max(0.1, std::min(m_zoomFactor, 10.0));
    update();
}
