// WCSDebugger.h - Comprehensive WCS debugging system
#ifndef WCS_DEBUGGER_H
#define WCS_DEBUGGER_H

#include <QObject>
#include <QVector>
#include <QPointF>
#include <QString>
#include <QDebug>
#include <pcl/AstrometricMetadata.h>
#include "StarCatalogValidator.h"

struct TestVector {
    // Input coordinates
    double inputRA = 0.0;       // degrees
    double inputDec = 0.0;      // degrees
    double inputPixelX = 0.0;   // pixels
    double inputPixelY = 0.0;   // pixels
    
    // Expected outputs
    double expectedRA = 0.0;    // degrees
    double expectedDec = 0.0;   // degrees
    double expectedX = 0.0;     // pixels
    double expectedY = 0.0;     // pixels
    
    // Actual outputs
    double actualRA = 0.0;      // degrees  
    double actualDec = 0.0;     // degrees
    double actualX = 0.0;       // pixels
    double actualY = 0.0;       // pixels
    
    // Errors
    double raError = 0.0;       // arcseconds
    double decError = 0.0;      // arcseconds
    double pixelError = 0.0;    // pixels
    
    QString name;
    bool passed = false;
};

class WCSDebugger : public QObject
{
    Q_OBJECT
    
public:
    explicit WCSDebugger(QObject* parent = nullptr);
    
    // Set the systems to debug
    void setStarCatalogValidator(StarCatalogValidator* validator);
    void setPCLAstrometricMetadata(const pcl::AstrometricMetadata& metadata);
    
    // Generate test vectors from real data
    void generateTestVectorsFromMatches(const ValidationResult& result,
                                       const QVector<QPoint>& detectedStars);
    
    // Manual test vector creation
    void addTestVector(const QString& name, double ra, double dec, 
                      double pixelX, double pixelY);
    
    // Comprehensive debugging
    void runFullDiagnostic();
    void runCoordinateTests();
    void runRoundTripTests(); 
    void runKnownStarTests();
    void runSystematicGridTests();
    
    // Specific problem diagnosis
    void diagnoseScaleProblems();
    void diagnoseRotationProblems(); 
    void diagnoseOffsetProblems();
    void diagnoseDistortionProblems();
    
    // Test specific transformations
    bool testSkyToPixel(double ra, double dec, double expectedX, double expectedY, 
                       double tolerance = 2.0);
    bool testPixelToSky(double x, double y, double expectedRA, double expectedDec,
                       double tolerance = 5.0); // arcseconds
    
    // Analysis and reporting
    void printDetailedReport();
    void printTestVectorSummary();
    void exportDebugReport(const QString& filename);
    
    // Quick fixes and suggestions
    QStringList getSuggestedFixes();
    void attemptAutomaticCorrections();

private:
    StarCatalogValidator* m_validator = nullptr;
    pcl::AstrometricMetadata m_pclMetadata;
    bool m_hasPCLMetadata = false;
    
    QVector<TestVector> m_testVectors;
    
    // Analysis helpers
    void analyzeSystematicErrors();
    void checkForCommonProblems();
    void validateWCSConsistency();
    
    // Test vector generators
    void generateGridTestVectors(int gridSize = 5);
    void generateBoundaryTestVectors();
    void generateCenterTestVectors();
    
    // Error analysis
    double calculateRMSError(const QVector<TestVector>& vectors, bool usePixels = true);
    double calculateSystematicBias(const QVector<TestVector>& vectors, bool useRA = true);
    
    // Known star positions for validation
    struct KnownStar {
        QString name;
        double ra, dec;      // J2000 coordinates
        double vmag;         // Visual magnitude
    };
    
    QVector<KnownStar> getKnownStarsInField(double centerRA, double centerDec, 
                                           double radiusDegrees);
};

// Implementation
WCSDebugger::WCSDebugger(QObject* parent) : QObject(parent)
{
}

void WCSDebugger::setStarCatalogValidator(StarCatalogValidator* validator)
{
    m_validator = validator;
    qDebug() << "WCSDebugger: StarCatalogValidator set";
}

void WCSDebugger::setPCLAstrometricMetadata(const pcl::AstrometricMetadata& metadata)
{
    m_pclMetadata = metadata;
    m_hasPCLMetadata = metadata.IsValid();
    qDebug() << "WCSDebugger: PCL AstrometricMetadata set, valid:" << m_hasPCLMetadata;
}

void WCSDebugger::generateTestVectorsFromMatches(const ValidationResult& result,
                                                const QVector<QPoint>& detectedStars)
{
    qDebug() << "\n=== GENERATING TEST VECTORS FROM MATCHES ===";
    
    if (!m_validator) {
        qDebug() << "❌ No validator set";
        return;
    }
    
    m_testVectors.clear();
    
    // Generate test vectors from successful matches
    for (const auto& match : result.matches) {
        if (!match.isGoodMatch) continue;
        
        if (match.detectedIndex >= 0 && match.detectedIndex < detectedStars.size() &&
            match.catalogIndex >= 0 && match.catalogIndex < result.catalogStars.size()) {
            
            const QPoint& detectedPos = detectedStars[match.detectedIndex];
            const CatalogStar& catalogStar = result.catalogStars[match.catalogIndex];
            
            TestVector testVec;
            testVec.name = QString("Match_%1_%2").arg(match.detectedIndex).arg(catalogStar.id);
            
            // Sky -> Pixel test
            testVec.inputRA = catalogStar.ra;
            testVec.inputDec = catalogStar.dec;
            testVec.expectedX = detectedPos.x();
            testVec.expectedY = detectedPos.y();
            
            // Test the transformation
            QPointF calculatedPixel = m_validator->skyToPixel(catalogStar.ra, catalogStar.dec);
            testVec.actualX = calculatedPixel.x();
            testVec.actualY = calculatedPixel.y();
            
            // Calculate errors
            testVec.pixelError = sqrt(pow(testVec.actualX - testVec.expectedX, 2) + 
                                     pow(testVec.actualY - testVec.expectedY, 2));
            testVec.passed = testVec.pixelError < 5.0; // 5 pixel tolerance
            
            m_testVectors.append(testVec);
            
            // Also create reverse test (Pixel -> Sky)
            TestVector reverseVec;
            reverseVec.name = QString("Reverse_%1_%2").arg(match.detectedIndex).arg(catalogStar.id);
            reverseVec.inputPixelX = detectedPos.x();
            reverseVec.inputPixelY = detectedPos.y();