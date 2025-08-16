#include "StarChartWidget.h"

// StarChartWidget Implementation

StarChartWidget::StarChartWidget(QWidget* parent)
    : QWidget(parent)
    , m_plotMode(MagnitudeVsSpectralType)
    , m_colorScheme(SpectralTypeColors)
    , m_showLegend(true)
    , m_showGrid(true)
    , m_leftMargin(60)
    , m_rightMargin(20)
    , m_topMargin(20)
    , m_bottomMargin(60)
    , m_zoomLevel(1.0)
    , m_panOffset(0, 0)
    , m_isPanning(false)
    , m_hoveredStarIndex(-1)
{
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);
    
    // Initialize hover timer
    m_hoverTimer = new QTimer(this);
    m_hoverTimer->setSingleShot(true);
    m_hoverTimer->setInterval(500); // 500ms hover delay
    
    // Initialize spectral color map
    m_spectralColorMap["O"] = QColor(155, 176, 255);
    m_spectralColorMap["B"] = QColor(170, 191, 255);
    m_spectralColorMap["A"] = QColor(202, 215, 255);
    m_spectralColorMap["F"] = QColor(248, 247, 255);
    m_spectralColorMap["G"] = QColor(255, 244, 234);
    m_spectralColorMap["K"] = QColor(255, 210, 161);
    m_spectralColorMap["M"] = QColor(255, 204, 111);
    m_spectralColorMap["U"] = QColor(128, 128, 128);
}

void StarChartWidget::setStarData(const QVector<CatalogStar>& stars)
{
    m_stars = stars;
    calculateDataRanges();
    update();
}

void StarChartWidget::setPlotMode(int mode)
{
    m_plotMode = static_cast<PlotMode>(mode);
    calculateDataRanges();
    update();
}

void StarChartWidget::setColorScheme(int scheme)
{
    m_colorScheme = static_cast<ColorScheme>(scheme);
    update();
}

void StarChartWidget::setShowLegend(bool show)
{
    m_showLegend = show;
    update();
}

void StarChartWidget::setShowGrid(bool show)
{
    m_showGrid = show;
    update();
}

void StarChartWidget::resetZoom()
{
    m_zoomLevel = 1.0;
    m_panOffset = QPointF(0, 0);
    update();
}

void StarChartWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Clear background
    painter.fillRect(rect(), QColor(25, 25, 35)); // Dark background
    
    // Update chart rectangle (accounting for legend)
    int legendWidth = m_showLegend ? LEGEND_WIDTH : 0;
    m_chartRect = QRect(m_leftMargin, m_topMargin,
                        width() - m_leftMargin - m_rightMargin - legendWidth,
                        height() - m_topMargin - m_bottomMargin);
    
    if (m_chartRect.width() <= 0 || m_chartRect.height() <= 0) {
        return; // Not enough space to draw
    }
    
    // Draw grid first (if enabled)
    if (m_showGrid) {
        drawGrid(painter);
    }
    
    // Draw axes
    drawAxes(painter);
    
    // Draw chart content
    drawChart(painter);
    
    // Draw legend last (if enabled)
    if (m_showLegend) {
        drawLegend(painter);
    }
}

void StarChartWidget::drawChart(QPainter& painter)
{
    switch (m_plotMode) {
    case MagnitudeVsSpectralType:
        drawMagnitudeVsSpectralChart(painter);
        break;
    case RAvsDecPosition:
        drawPositionChart(painter);
        break;
    case MagnitudeDistribution:
        drawMagnitudeDistribution(painter);
        break;
    case SpectralTypeDistribution:
        drawSpectralDistribution(painter);
        break;
    }
}

void StarChartWidget::drawMagnitudeVsSpectralChart(QPainter& painter)
{
    painter.setClipRect(m_chartRect);
    
    // Create position mapping for spectral types
    QMap<QString, int> spectralPositions;
    int position = 0;
    for (const QString& type : m_spectralTypes) {
        spectralPositions[type] = position++;
    }
    
    for (int i = 0; i < m_stars.size(); ++i) {
        const auto& star = m_stars[i];
        QString spectralType = star.spectralType.left(1);
        
        if (!spectralPositions.contains(spectralType)) continue;
        
        // Map to widget coordinates
        double x = m_chartRect.left() + (spectralPositions[spectralType] + 0.5) * 
                   m_chartRect.width() / qMax(1, m_spectralTypes.size());
        double y = m_chartRect.bottom() - (star.magnitude - m_minMagnitude) * 
                   m_chartRect.height() / qMax(0.1, m_maxMagnitude - m_minMagnitude);
        
        // Apply zoom and pan
        x = (x - m_chartRect.center().x()) * m_zoomLevel + m_chartRect.center().x() + m_panOffset.x();
        y = (y - m_chartRect.center().y()) * m_zoomLevel + m_chartRect.center().y() + m_panOffset.y();
        
        QColor color = getStarColor(star);
        double size = getStarSize(star);
        
        // Highlight hovered star
        if (i == m_hoveredStarIndex) {
            painter.setPen(QPen(Qt::yellow, 2));
            size *= 1.5;
        } else {
            painter.setPen(QPen(color.darker(150), 1));
        }
        
        painter.setBrush(color);
        painter.drawEllipse(QPointF(x, y), size/2, size/2);
    }
    
    painter.setClipping(false);
}

void StarChartWidget::drawPositionChart(QPainter& painter)
{
    painter.setClipRect(m_chartRect);
    
    for (int i = 0; i < m_stars.size(); ++i) {
        const auto& star = m_stars[i];
        
        // Map RA/Dec to widget coordinates
        double x = m_chartRect.left() + (star.ra - m_minRA) * 
                   m_chartRect.width() / qMax(0.1, m_maxRA - m_minRA);
        double y = m_chartRect.bottom() - (star.dec - m_minDec) * 
                   m_chartRect.height() / qMax(0.1, m_maxDec - m_minDec);
        
        // Apply zoom and pan
        x = (x - m_chartRect.center().x()) * m_zoomLevel + m_chartRect.center().x() + m_panOffset.x();
        y = (y - m_chartRect.center().y()) * m_zoomLevel + m_chartRect.center().y() + m_panOffset.y();
        
        QColor color = getStarColor(star);
        double size = getStarSize(star);
        
        // Highlight hovered star
        if (i == m_hoveredStarIndex) {
            painter.setPen(QPen(Qt::yellow, 2));
            size *= 1.5;
        } else {
            painter.setPen(QPen(color.darker(150), 1));
        }
        
        painter.setBrush(color);
        painter.drawEllipse(QPointF(x, y), size/2, size/2);
    }
    
    painter.setClipping(false);
}

void StarChartWidget::drawMagnitudeDistribution(QPainter& painter)
{
    painter.setClipRect(m_chartRect);
    
    // Create magnitude bins
    const int numBins = 20;
    double binWidth = (m_maxMagnitude - m_minMagnitude) / numBins;
    QVector<int> bins(numBins, 0);
    
    // Fill bins
    for (const auto& star : m_stars) {
        int binIndex = qBound(0, static_cast<int>((star.magnitude - m_minMagnitude) / binWidth), numBins - 1);
        bins[binIndex]++;
    }
    
    // Find max count for scaling
    int maxCount = *std::max_element(bins.begin(), bins.end());
    if (maxCount == 0) return;
    
    // Draw histogram bars
    double barWidth = m_chartRect.width() / static_cast<double>(numBins);
    
    for (int i = 0; i < numBins; ++i) {
        if (bins[i] == 0) continue;
        
        double x = m_chartRect.left() + i * barWidth;
        double height = bins[i] * m_chartRect.height() / static_cast<double>(maxCount);
        double y = m_chartRect.bottom() - height;
        
        QRectF barRect(x, y, barWidth * 0.8, height);
        
        QColor barColor = QColor(100, 150, 255, 180);
        painter.setPen(QPen(barColor.darker(150), 1));
        painter.setBrush(barColor);
        painter.drawRect(barRect);
        
        // Draw count label
        if (bins[i] > maxCount * 0.1) { // Only show labels for significant bars
            painter.setPen(Qt::white);
            painter.drawText(barRect, Qt::AlignCenter, QString::number(bins[i]));
        }
    }
    
    painter.setClipping(false);
}

void StarChartWidget::drawSpectralDistribution(QPainter& painter)
{
    painter.setClipRect(m_chartRect);
    
    // Count stars by spectral type
    QMap<QString, int> spectralCounts;
    for (const auto& star : m_stars) {
        QString type = star.spectralType.left(1);
        if (!type.isEmpty()) {
            spectralCounts[type]++;
        }
    }
    
    if (spectralCounts.isEmpty()) return;
    
    // Find max count for scaling
    int maxCount = 0;
    for (auto it = spectralCounts.begin(); it != spectralCounts.end(); ++it) {
        maxCount = qMax(maxCount, it.value());
    }
    
    // Draw bars
    double barWidth = m_chartRect.width() / static_cast<double>(spectralCounts.size());
    int barIndex = 0;
    
    for (auto it = spectralCounts.begin(); it != spectralCounts.end(); ++it, ++barIndex) {
        double x = m_chartRect.left() + barIndex * barWidth;
        double height = it.value() * m_chartRect.height() / static_cast<double>(maxCount);
        double y = m_chartRect.bottom() - height;
        
        QRectF barRect(x, y, barWidth * 0.8, height);
        
        QColor barColor = m_spectralColorMap.value(it.key(), QColor(128, 128, 128));
        painter.setPen(QPen(barColor.darker(150), 1));
        painter.setBrush(barColor);
        painter.drawRect(barRect);
        
        // Draw spectral type label
        painter.setPen(Qt::white);
        QFont font = painter.font();
        font.setBold(true);
        painter.setFont(font);
        painter.drawText(QRectF(x, m_chartRect.bottom() + 5, barWidth, 20), 
                        Qt::AlignCenter, it.key());
        
        // Draw count label
        painter.drawText(barRect, Qt::AlignCenter, QString::number(it.value()));
    }
    
    painter.setClipping(false);
}

void StarChartWidget::drawLegend(QPainter& painter)
{
    if (!m_showLegend) return;
    
    QRect legendRect(width() - LEGEND_WIDTH + 10, m_topMargin, 
                     LEGEND_WIDTH - 20, height() - m_topMargin - m_bottomMargin);
    
    // Legend background
    painter.setPen(QPen(Qt::gray, 1));
    painter.setBrush(QColor(40, 40, 50, 200));
    painter.drawRect(legendRect);
    
    // Legend title
    painter.setPen(Qt::white);
    QFont titleFont = painter.font();
    titleFont.setBold(true);
    titleFont.setPointSize(10);
    painter.setFont(titleFont);
    
    QString legendTitle;
    switch (m_colorScheme) {
    case SpectralTypeColors:
        legendTitle = "Spectral Types";
        break;
    case MagnitudeColors:
        legendTitle = "Magnitude";
        break;
    case MonochromeColors:
        legendTitle = "Size = Brightness";
        break;
    }
    
    painter.drawText(legendRect.adjusted(5, 5, -5, 0), Qt::AlignTop | Qt::AlignHCenter, legendTitle);
    
    // Legend content
    QFont normalFont = painter.font();
    normalFont.setBold(false);
    normalFont.setPointSize(9);
    painter.setFont(normalFont);
    
    int yOffset = 30;
    
    if (m_colorScheme == SpectralTypeColors) {
        // Show spectral type colors
        for (const QString& type : m_spectralTypes) {
            QColor color = m_spectralColorMap.value(type, QColor(128, 128, 128));
            
            // Draw color swatch
            QRect swatchRect(legendRect.left() + 10, legendRect.top() + yOffset, 15, 15);
            painter.setPen(QPen(color.darker(150), 1));
            painter.setBrush(color);
            painter.drawEllipse(swatchRect);
            
            // Draw label
            painter.setPen(Qt::white);
            painter.drawText(legendRect.left() + 30, legendRect.top() + yOffset + 12, 
                           QString("%1-type").arg(type));
            
            yOffset += 20;
        }
    } else if (m_colorScheme == MagnitudeColors) {
        // Show magnitude gradient
        QLinearGradient gradient(0, 0, 0, 100);
        gradient.setColorAt(0, Qt::blue);
        gradient.setColorAt(0.5, Qt::green);
        gradient.setColorAt(1, Qt::red);
        
        QRect gradientRect(legendRect.left() + 10, legendRect.top() + yOffset, 15, 100);
        painter.setPen(Qt::gray);
        painter.setBrush(QBrush(gradient));
        painter.drawRect(gradientRect);
        
        // Labels
        painter.setPen(Qt::white);
        painter.drawText(legendRect.left() + 30, legendRect.top() + yOffset + 5, 
                        QString("Bright (%1)").arg(m_minMagnitude, 0, 'f', 1));
        painter.drawText(legendRect.left() + 30, legendRect.top() + yOffset + 95, 
                        QString("Faint (%1)").arg(m_maxMagnitude, 0, 'f', 1));
    } else {
        // Monochrome - show size scale
        painter.setPen(Qt::lightGray);
        painter.setBrush(Qt::lightGray);
        
        // Draw different sized circles
        for (int i = 0; i < 4; ++i) {
            double size = MIN_STAR_SIZE + i * (MAX_STAR_SIZE - MIN_STAR_SIZE) / 3.0;
            painter.drawEllipse(QPointF(legendRect.left() + 20, legendRect.top() + yOffset + 10), 
                              size/2, size/2);
            yOffset += size + 5;
        }
        
        painter.setPen(Qt::white);
        painter.drawText(legendRect.left() + 10, legendRect.top() + 30, "Brighter");
        painter.drawText(legendRect.left() + 10, legendRect.top() + yOffset - 10, "Fainter");
    }
}

void StarChartWidget::drawGrid(QPainter& painter)
{
    painter.setPen(QPen(QColor(60, 60, 80), 1, Qt::DotLine));
    
    // Vertical grid lines
    for (int x = m_chartRect.left(); x <= m_chartRect.right(); x += GRID_SPACING) {
        painter.drawLine(x, m_chartRect.top(), x, m_chartRect.bottom());
    }
    
    // Horizontal grid lines
    for (int y = m_chartRect.top(); y <= m_chartRect.bottom(); y += GRID_SPACING) {
        painter.drawLine(m_chartRect.left(), y, m_chartRect.right(), y);
    }
}

void StarChartWidget::drawAxes(QPainter& painter)
{
    painter.setPen(QPen(Qt::white, 2));
    
    // Draw axes
    painter.drawLine(m_chartRect.bottomLeft(), m_chartRect.topLeft()); // Y-axis
    painter.drawLine(m_chartRect.bottomLeft(), m_chartRect.bottomRight()); // X-axis
    
    // Axis labels
    QFont axisFont = painter.font();
    axisFont.setPointSize(10);
    painter.setFont(axisFont);
    
    QString xLabel, yLabel;
    
    switch (m_plotMode) {
    case MagnitudeVsSpectralType:
        xLabel = "Spectral Type";
        yLabel = "Magnitude";
        break;
    case RAvsDecPosition:
        xLabel = "Right Ascension (°)";
        yLabel = "Declination (°)";
        break;
    case MagnitudeDistribution:
        xLabel = "Magnitude";
        yLabel = "Number of Stars";
        break;
    case SpectralTypeDistribution:
        xLabel = "Spectral Type";
        yLabel = "Number of Stars";
        break;
    }
    
    // X-axis label
    painter.drawText(QRect(m_chartRect.left(), height() - m_bottomMargin + 35,
                          m_chartRect.width(), 20), Qt::AlignCenter, xLabel);
    
    // Y-axis label (rotated)
    painter.save();
    painter.translate(15, m_chartRect.center().y());
    painter.rotate(-90);
    painter.drawText(QRect(-m_chartRect.height()/2, -10, m_chartRect.height(), 20), 
                    Qt::AlignCenter, yLabel);
    painter.restore();
    
    // Tick marks and values
    drawAxisTicks(painter);
}

void StarChartWidget::drawAxisTicks(QPainter& painter)
{
    painter.setPen(Qt::lightGray);
    QFont tickFont = painter.font();
    tickFont.setPointSize(8);
    painter.setFont(tickFont);
    QMap<QString, int> spectralCounts;
    
    switch (m_plotMode) {
    case MagnitudeVsSpectralType:
        // X-axis: spectral types
        for (int i = 0; i < m_spectralTypes.size(); ++i) {
            double x = m_chartRect.left() + (i + 0.5) * m_chartRect.width() / m_spectralTypes.size();
            painter.drawLine(x, m_chartRect.bottom(), x, m_chartRect.bottom() + 5);
            painter.drawText(QRectF(x - 15, m_chartRect.bottom() + 8, 30, 15), 
                           Qt::AlignCenter, m_spectralTypes[i]);
        }
        
        // Y-axis: magnitude values
        for (int i = 0; i <= 10; ++i) {
            double mag = m_minMagnitude + i * (m_maxMagnitude - m_minMagnitude) / 10.0;
            double y = m_chartRect.bottom() - i * m_chartRect.height() / 10.0;
            painter.drawLine(m_chartRect.left() - 5, y, m_chartRect.left(), y);
            painter.drawText(QRectF(5, y - 8, 50, 16), Qt::AlignRight | Qt::AlignVCenter, 
                           QString::number(mag, 'f', 1));
        }
        break;
        
    case RAvsDecPosition:
        // X-axis: RA values
        for (int i = 0; i <= 5; ++i) {
            double ra = m_minRA + i * (m_maxRA - m_minRA) / 5.0;
            double x = m_chartRect.left() + i * m_chartRect.width() / 5.0;
            painter.drawLine(x, m_chartRect.bottom(), x, m_chartRect.bottom() + 5);
            painter.drawText(QRectF(x - 25, m_chartRect.bottom() + 8, 50, 15), 
                           Qt::AlignCenter, QString::number(ra, 'f', 1));
        }
        
        // Y-axis: Dec values
        for (int i = 0; i <= 5; ++i) {
            double dec = m_minDec + i * (m_maxDec - m_minDec) / 5.0;
            double y = m_chartRect.bottom() - i * m_chartRect.height() / 5.0;
            painter.drawLine(m_chartRect.left() - 5, y, m_chartRect.left(), y);
            painter.drawText(QRectF(5, y - 8, 50, 16), Qt::AlignRight | Qt::AlignVCenter, 
                           QString::number(dec, 'f', 1));
        }
        break;
        
    case MagnitudeDistribution:
        // X-axis: magnitude bins
        for (int i = 0; i <= 10; ++i) {
            double mag = m_minMagnitude + i * (m_maxMagnitude - m_minMagnitude) / 10.0;
            double x = m_chartRect.left() + i * m_chartRect.width() / 10.0;
            painter.drawLine(x, m_chartRect.bottom(), x, m_chartRect.bottom() + 5);
            painter.drawText(QRectF(x - 20, m_chartRect.bottom() + 8, 40, 15), 
                           Qt::AlignCenter, QString::number(mag, 'f', 1));
        }

	{
	    // Y-axis: count values
	    int maxCount = 0;
	    const int numBins = 20;
	    double binWidth = (m_maxMagnitude - m_minMagnitude) / numBins;
	    QVector<int> bins(numBins, 0);
	    for (const auto& star : m_stars) {
		int binIndex = qBound(0, static_cast<int>((star.magnitude - m_minMagnitude) / binWidth), numBins - 1);
		bins[binIndex]++;
	    }

	    maxCount = *std::max_element(bins.begin(), bins.end());

	    for (int i = 0; i <= 5; ++i) {
		int count = i * maxCount / 5;
		double y = m_chartRect.bottom() - i * m_chartRect.height() / 5.0;
		painter.drawLine(m_chartRect.left() - 5, y, m_chartRect.left(), y);
		painter.drawText(QRectF(5, y - 8, 50, 16), Qt::AlignRight | Qt::AlignVCenter, 
			       QString::number(count));
	    }
	}
        break;
        
    case SpectralTypeDistribution:
        // X-axis: spectral types (already drawn in drawSpectralDistribution)
        // Y-axis: count values
        for (const auto& star : m_stars) {
            QString type = star.spectralType.left(1);
            if (!type.isEmpty()) {
                spectralCounts[type]++;
            }
        }
        
        int maxSpectralCount = 0;
        for (auto it = spectralCounts.begin(); it != spectralCounts.end(); ++it) {
            maxSpectralCount = qMax(maxSpectralCount, it.value());
        }
        
        for (int i = 0; i <= 5; ++i) {
            int count = i * maxSpectralCount / 5;
            double y = m_chartRect.bottom() - i * m_chartRect.height() / 5.0;
            painter.drawLine(m_chartRect.left() - 5, y, m_chartRect.left(), y);
            painter.drawText(QRectF(5, y - 8, 50, 16), Qt::AlignRight | Qt::AlignVCenter, 
                           QString::number(count));
        }
        break;
    }
}

QColor StarChartWidget::getStarColor(const CatalogStar& star) const
{
    switch (m_colorScheme) {
    case SpectralTypeColors:
        return m_spectralColorMap.value(star.spectralType.left(1), QColor(200, 200, 200));
        
    case MagnitudeColors: {
        // Color gradient from blue (bright) to red (faint)
        double normalized = (star.magnitude - m_minMagnitude) / qMax(0.1, m_maxMagnitude - m_minMagnitude);
        normalized = qBound(0.0, normalized, 1.0);
        
        if (normalized < 0.5) {
            // Blue to green
            double t = normalized * 2.0;
            return QColor(static_cast<int>((1.0 - t) * 100 + t * 0),
                         static_cast<int>((1.0 - t) * 150 + t * 255),
                         static_cast<int>((1.0 - t) * 255 + t * 0));
        } else {
            // Green to red
            double t = (normalized - 0.5) * 2.0;
            return QColor(static_cast<int>((1.0 - t) * 0 + t * 255),
                         static_cast<int>((1.0 - t) * 255 + t * 0),
                         0);
        }
    }
    
    case MonochromeColors:
    default:
        return QColor(200, 200, 200);
    }
}

double StarChartWidget::getStarSize(const CatalogStar& star) const
{
    // Invert magnitude scale (brighter stars = larger size)
    double normalized = 1.0 - (star.magnitude - m_minMagnitude) / qMax(0.1, m_maxMagnitude - m_minMagnitude);
    normalized = qBound(0.0, normalized, 1.0);
    return MIN_STAR_SIZE + normalized * (MAX_STAR_SIZE - MIN_STAR_SIZE);
}

QPointF StarChartWidget::mapStarToWidget(const CatalogStar& star) const
{
    // This method maps star coordinates to widget coordinates based on current plot mode
    switch (m_plotMode) {
    case MagnitudeVsSpectralType: {
        QString spectralType = star.spectralType.left(1);
        int typeIndex = m_spectralTypes.indexOf(spectralType);
        if (typeIndex < 0) typeIndex = 0;
        
        double x = m_chartRect.left() + (typeIndex + 0.5) * m_chartRect.width() / qMax(1, m_spectralTypes.size());
        double y = m_chartRect.bottom() - (star.magnitude - m_minMagnitude) * 
                   m_chartRect.height() / qMax(0.1, m_maxMagnitude - m_minMagnitude);
        return QPointF(x, y);
    }
    
    case RAvsDecPosition: {
        double x = m_chartRect.left() + (star.ra - m_minRA) * 
                   m_chartRect.width() / qMax(0.1, m_maxRA - m_minRA);
        double y = m_chartRect.bottom() - (star.dec - m_minDec) * 
                   m_chartRect.height() / qMax(0.1, m_maxDec - m_minDec);
        return QPointF(x, y);
    }
    
    default:
        return QPointF(0, 0);
    }
}

int StarChartWidget::findStarAtPosition(const QPoint& pos) const
{
    for (int i = 0; i < m_stars.size(); ++i) {
        QPointF starPos = mapStarToWidget(m_stars[i]);
        
        // Apply zoom and pan
        starPos.setX((starPos.x() - m_chartRect.center().x()) * m_zoomLevel + 
                     m_chartRect.center().x() + m_panOffset.x());
        starPos.setY((starPos.y() - m_chartRect.center().y()) * m_zoomLevel + 
                     m_chartRect.center().y() + m_panOffset.y());
        
        double distance = QPointF(pos - starPos.toPoint()).manhattanLength();
        double starSize = getStarSize(m_stars[i]);
        
        if (distance <= starSize) {
            return i;
        }
    }
    return -1;
}

void StarChartWidget::calculateDataRanges()
{
    if (m_stars.isEmpty()) return;
    
    m_minMagnitude = std::numeric_limits<double>::max();
    m_maxMagnitude = std::numeric_limits<double>::lowest();
    m_minRA = std::numeric_limits<double>::max();
    m_maxRA = std::numeric_limits<double>::lowest();
    m_minDec = std::numeric_limits<double>::max();
    m_maxDec = std::numeric_limits<double>::lowest();
    m_spectralTypes.clear();
    
    QSet<QString> spectralTypeSet;
    
    for (const auto& star : m_stars) {
        m_minMagnitude = qMin(m_minMagnitude, star.magnitude);
        m_maxMagnitude = qMax(m_maxMagnitude, star.magnitude);
        m_minRA = qMin(m_minRA, star.ra);
        m_maxRA = qMax(m_maxRA, star.ra);
        m_minDec = qMin(m_minDec, star.dec);
        m_maxDec = qMax(m_maxDec, star.dec);
        
        QString spectralType = star.spectralType.left(1);
        if (!spectralType.isEmpty()) {
            spectralTypeSet.insert(spectralType);
        }
    }
    
    // Sort spectral types in standard order
    QStringList standardOrder = {"O", "B", "A", "F", "G", "K", "M", "U"};
    for (const QString& type : standardOrder) {
        if (spectralTypeSet.contains(type)) {
            m_spectralTypes.append(type);
        }
    }
}

void StarChartWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        int starIndex = findStarAtPosition(event->pos());
        if (starIndex >= 0) {
            emit starClicked(m_stars[starIndex]);
        } else {
            // Start panning
            m_isPanning = true;
            m_lastPanPoint = event->pos();
        }
    } else if (event->button() == Qt::RightButton) {
        resetZoom();
    }
}

void StarChartWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_isPanning && (event->buttons() & Qt::LeftButton)) {
        QPoint delta = event->pos() - m_lastPanPoint;
        m_panOffset += QPointF(delta);
        m_lastPanPoint = event->pos();
        update();
    } else {
        // Handle hover
        int starIndex = findStarAtPosition(event->pos());
        if (starIndex != m_hoveredStarIndex) {
            m_hoveredStarIndex = starIndex;
            update();
            
            if (starIndex >= 0) {
                emit starHovered(m_stars[starIndex]);
            }
        }
    }
}

void StarChartWidget::wheelEvent(QWheelEvent* event)
{
    const double zoomFactor = 1.15;
    
    if (event->angleDelta().y() > 0) {
        m_zoomLevel *= zoomFactor;
    } else {
        m_zoomLevel /= zoomFactor;
    }
    
    m_zoomLevel = qBound(0.1, m_zoomLevel, 10.0);
    update();
}

void StarChartWidget::resizeEvent(QResizeEvent* event)
{
    Q_UNUSED(event)
    updateTransform();
}

void StarChartWidget::updateTransform()
{
    // Update chart rectangle based on current widget size
    int legendWidth = m_showLegend ? LEGEND_WIDTH : 0;
    m_chartRect = QRect(m_leftMargin, m_topMargin,
                        width() - m_leftMargin - m_rightMargin - legendWidth,
                        height() - m_topMargin - m_bottomMargin);
}
