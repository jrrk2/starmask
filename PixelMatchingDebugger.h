// PixelMatchingDebugger.h - Fixed version with proper Qt MOC support
#ifndef PIXEL_MATCHING_DEBUGGER_H
#define PIXEL_MATCHING_DEBUGGER_H

#include <QObject>
#include <QVector>
#include <QPointF>
#include <QDebug>
#include <QStringList>
#include "StarCatalogValidator.h"

class PixelMatchingDebugger : public QObject
{
    Q_OBJECT
    
public:
    struct MatchDiagnostic {
        int detectedIndex = -1;
        int catalogIndex = -1;
        QPoint detectedPos;
        QPointF catalogPos;
        QString catalogId;
        double magnitude = 0.0;
        
        // Distance metrics
        double pixelDistance = 0.0;
        double magnitudeDiff = 0.0;
        
        // Criteria checks
        bool passesDistanceCheck = false;
        bool passesMagnitudeCheck = false;
        bool passesOverallCheck = false;
        
        // Why it failed
        QStringList failureReasons;
        
        // Visual assessment
        bool shouldVisuallyMatch = false; // Manual assessment
    };
    
    explicit PixelMatchingDebugger(QObject* parent = nullptr);
    virtual ~PixelMatchingDebugger() = default;
    
    // Main debugging method
    void diagnoseMatching(const QVector<QPoint>& detectedStars,
                         const QVector<CatalogStar>& catalogStars,
                         const ValidationResult& result,
                         const StarCatalogValidator* validator);
    
    // Specific diagnostic methods
    void analyzeDistanceCriteria(double pixelTolerance);
    void analyzeMagnitudeCriteria(double magnitudeTolerance);
    void findMissedMatches(double searchRadius = 10.0);
    void suggestParameterAdjustments();
    
    // Interactive debugging
    void examineSpecificMatch(int detectedIndex, int catalogIndex);
    void findNearestCatalogStar(const QPoint& detectedPos, int searchRadius = 20);
    void listAllMatchesInRadius(const QPoint& center, double radius);
    
    // Test parameter sensitivity
    void testToleranceRange(double minPixelTol, double maxPixelTol, double step = 0.5);
    void testMagnitudeRange(double minMagTol, double maxMagTol, double step = 0.2);
    
    // Export debugging info
    void exportDebugReport(const QString& filename);
    void printSummaryReport();

private:
    QVector<MatchDiagnostic> m_diagnostics;
    QVector<QPoint> m_detectedStars;
    QVector<CatalogStar> m_catalogStars;
    ValidationResult m_result;
    const StarCatalogValidator* m_validator = nullptr;
    
    double m_currentPixelTolerance = 5.0;
    double m_currentMagnitudeTolerance = 2.0;
    
    // Analysis helpers (these were missing)
    void calculateAllDistances();
    void identifyProblematicMatches();
    void checkForSystematicBias();
    void analyzeMatchingQuality(); 
    
    // Statistics
    struct MatchingStats {
        int totalDetected = 0;
        int totalCatalog = 0;
        int potentialMatches = 0;    // Within search radius
        int distancePassingMatches = 0;  // Pass distance criteria
        int magnitudePassingMatches = 0; // Pass magnitude criteria  
        int actualMatches = 0;       // Pass all criteria
        
        double avgDistance = 0.0;
        double medianDistance = 0.0;
        double maxDistance = 0.0;
        
        double avgMagnitudeDiff = 0.0;
        double maxMagnitudeDiff = 0.0;
        
        QPointF systematicOffset;    // Average dx, dy
    };
    
    MatchingStats calculateStats();
};

#endif // PIXEL_MATCHING_DEBUGGER_H