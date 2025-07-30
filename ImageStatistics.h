#ifndef IMAGE_STATISTICS_H
#define IMAGE_STATISTICS_H

#include <QString>

class ImageStatistics
{
public:
    ImageStatistics();
    ~ImageStatistics();

    // Calculate statistics for image data
    void calculate(const float* data, size_t count);
    void clear();
    
    // Accessors
    double minimum() const { return m_min; }
    double maximum() const { return m_max; }
    double mean() const { return m_mean; }
    double standardDeviation() const { return m_stdDev; }
    double median() const { return m_median; }
    double mad() const { return m_mad; } // Median Absolute Deviation
    
    size_t count() const { return m_count; }
    bool isValid() const { return m_count > 0; }
    
    // Formatted output
    QString toString() const;
    QString toDetailedString() const;
    
    // Percentile calculations
    double percentile(double p) const; // p in [0,100]
    
private:
    void calculateBasicStats(const float* data, size_t count);
    void calculateAdvancedStats(const float* data, size_t count);
    
    double m_min;
    double m_max;
    double m_mean;
    double m_stdDev;
    double m_median;
    double m_mad;
    size_t m_count;
    
    // Cache sorted data for percentile calculations
    mutable float* m_sortedData;
    mutable bool m_sortedDataValid;
    void ensureSortedData(const float* data, size_t count) const;
};

#endif // IMAGE_STATISTICS_H