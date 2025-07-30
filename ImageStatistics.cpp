#include "ImageStatistics.h"
#include <algorithm>
#include <cmath>
#include <numeric>

ImageStatistics::ImageStatistics()
    : m_min(0.0)
    , m_max(0.0)
    , m_mean(0.0)
    , m_stdDev(0.0)
    , m_median(0.0)
    , m_mad(0.0)
    , m_count(0)
    , m_sortedData(nullptr)
    , m_sortedDataValid(false)
{
}

ImageStatistics::~ImageStatistics()
{
    delete[] m_sortedData;
}

void ImageStatistics::calculate(const float* data, size_t count)
{
    clear();
    
    if (!data || count == 0) {
        return;
    }
    
    m_count = count;
    calculateBasicStats(data, count);
    calculateAdvancedStats(data, count);
}

void ImageStatistics::clear()
{
    m_min = m_max = m_mean = m_stdDev = m_median = m_mad = 0.0;
    m_count = 0;
    
    delete[] m_sortedData;
    m_sortedData = nullptr;
    m_sortedDataValid = false;
}

void ImageStatistics::calculateBasicStats(const float* data, size_t count)
{
    // Find min/max and calculate mean
    m_min = m_max = data[0];
    double sum = 0.0;
    
    for (size_t i = 0; i < count; ++i) {
        float value = data[i];
        if (std::isfinite(value)) { // Skip NaN and infinite values
            m_min = std::min(m_min, static_cast<double>(value));
            m_max = std::max(m_max, static_cast<double>(value));
            sum += value;
        }
    }
    
    m_mean = sum / count;
    
    // Calculate standard deviation
    double sumSquaredDiff = 0.0;
    for (size_t i = 0; i < count; ++i) {
        float value = data[i];
        if (std::isfinite(value)) {
            double diff = value - m_mean;
            sumSquaredDiff += diff * diff;
        }
    }
    
    m_stdDev = std::sqrt(sumSquaredDiff / count);
}

void ImageStatistics::calculateAdvancedStats(const float* data, size_t count)
{
    // Ensure we have sorted data for median calculations
    ensureSortedData(data, count);
    
    // Calculate median
    if (count % 2 == 1) {
        m_median = m_sortedData[count / 2];
    } else {
        size_t mid = count / 2;
        m_median = (m_sortedData[mid - 1] + m_sortedData[mid]) * 0.5;
    }
    
    // Calculate MAD (Median Absolute Deviation)
    float* absDeviations = new float[count];
    for (size_t i = 0; i < count; ++i) {
        absDeviations[i] = std::abs(data[i] - static_cast<float>(m_median));
    }
    
    std::sort(absDeviations, absDeviations + count);
    
    if (count % 2 == 1) {
        m_mad = absDeviations[count / 2];
    } else {
        size_t mid = count / 2;
        m_mad = (absDeviations[mid - 1] + absDeviations[mid]) * 0.5;
    }
    
    delete[] absDeviations;
}

void ImageStatistics::ensureSortedData(const float* data, size_t count) const
{
    if (!m_sortedDataValid) {
        delete[] m_sortedData;
        m_sortedData = new float[count];
        std::copy(data, data + count, m_sortedData);
        std::sort(m_sortedData, m_sortedData + count);
        m_sortedDataValid = true;
    }
}

double ImageStatistics::percentile(double p) const
{
    if (!m_sortedDataValid || m_count == 0) {
        return 0.0;
    }
    
    p = std::clamp(p, 0.0, 100.0);
    
    if (p == 0.0) return m_sortedData[0];
    if (p == 100.0) return m_sortedData[m_count - 1];
    
    double index = (p / 100.0) * (m_count - 1);
    size_t lowerIndex = static_cast<size_t>(std::floor(index));
    size_t upperIndex = static_cast<size_t>(std::ceil(index));
    
    if (lowerIndex == upperIndex) {
        return m_sortedData[lowerIndex];
    }
    
    double weight = index - lowerIndex;
    return m_sortedData[lowerIndex] * (1.0 - weight) + m_sortedData[upperIndex] * weight;
}

QString ImageStatistics::toString() const
{
    if (!isValid()) {
        return "No data";
    }
    
    return QString("Min: %1, Max: %2, Mean: %3, StdDev: %4")
           .arg(m_min, 0, 'g', 6)
           .arg(m_max, 0, 'g', 6)
           .arg(m_mean, 0, 'g', 6)
           .arg(m_stdDev, 0, 'g', 6);
}

QString ImageStatistics::toDetailedString() const
{
    if (!isValid()) {
        return "No statistical data available";
    }
    
    return QString("Pixels: %1\n"
                   "Minimum: %2\n"
                   "Maximum: %3\n"
                   "Mean: %4\n"
                   "Std Dev: %5\n"
                   "Median: %6\n"
                   "MAD: %7\n"
                   "Range: %8")
           .arg(m_count)
           .arg(m_min, 0, 'g', 8)
           .arg(m_max, 0, 'g', 8)
           .arg(m_mean, 0, 'g', 8)
           .arg(m_stdDev, 0, 'g', 8)
           .arg(m_median, 0, 'g', 8)
           .arg(m_mad, 0, 'g', 8)
           .arg(m_max - m_min, 0, 'g', 8);
}