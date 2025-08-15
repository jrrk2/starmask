// PixelMatchingDebugger.cpp - Missing method implementations
// Add these to a new PixelMatchingDebugger.cpp file or to your existing StarCatalogValidator.cpp

#include "PixelMatchingDebugger.h"
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <algorithm>
#include <numeric>
#include <cmath>

void PixelMatchingDebugger::calculateAllDistances()
{
    // This method is called internally and doesn't need to do additional work
    // since distances are calculated in diagnoseMatching()
    qDebug() << "Distance calculations completed for" << m_diagnostics.size() << "potential matches";
}

void PixelMatchingDebugger::identifyProblematicMatches()
{
    if (m_diagnostics.isEmpty()) return;
    
    qDebug() << "\n--- IDENTIFYING PROBLEMATIC MATCHES ---";
    
    int closeButFailing = 0;
    int farButPassing = 0;
    int shouldMatchButDont = 0;
    int unexpectedFailures = 0;
    
    for (const auto& diag : m_diagnostics) {
        // Close matches that fail criteria
        if (diag.pixelDistance <= m_currentPixelTolerance && !diag.passesOverallCheck) {
            closeButFailing++;
            if (closeButFailing <= 3) { // Show first few cases
                qDebug() << QString("⚠️  Close but failing: det[%1] -> cat[%2] %3, dist=%4 px")
                            .arg(diag.detectedIndex).arg(diag.catalogIndex)
                            .arg(diag.catalogId).arg(diag.pixelDistance);
                qDebug() << QString("    Reasons: %1").arg(diag.failureReasons.join(", "));
            }
        }
        
        // Far matches that somehow pass
        if (diag.pixelDistance > m_currentPixelTolerance * 1.5 && diag.passesOverallCheck) {
            farButPassing++;
            qDebug() << QString("⚠️  Far but passing: det[%1] -> cat[%2] %3, dist=%4 px")
                        .arg(diag.detectedIndex).arg(diag.catalogIndex)
                        .arg(diag.catalogId).arg(diag.pixelDistance);
        }
        
        // Visual matches that fail mathematically (the main issue)
        if (diag.shouldVisuallyMatch && !diag.passesOverallCheck) {
            shouldMatchButDont++;
            if (shouldMatchButDont <= 5) {
                qDebug() << QString("🎯 Visual mismatch: det[%1] -> cat[%2] %3, dist=%4 px")
                            .arg(diag.detectedIndex).arg(diag.catalogIndex)
                            .arg(diag.catalogId).arg(diag.pixelDistance);
                qDebug() << QString("    Should match visually but fails: %1")
                            .arg(diag.failureReasons.join(", "));
            }
        }
        
        // Unexpected failures (pass distance and magnitude but still fail)
        if (diag.passesDistanceCheck && diag.passesMagnitudeCheck && !diag.passesOverallCheck) {
            unexpectedFailures++;
        }
    }
    
    qDebug() << QString("Problematic match summary:");
    qDebug() << QString("  Close but failing: %1").arg(closeButFailing);
    qDebug() << QString("  Far but passing: %1").arg(farButPassing);
    qDebug() << QString("  Visual mismatches: %1").arg(shouldMatchButDont);
    qDebug() << QString("  Unexpected failures: %1").arg(unexpectedFailures);
    
    if (shouldMatchButDont > 0) {
        qDebug() << "\n🔍 PRIMARY ISSUE IDENTIFIED:";
        qDebug() << QString("  %1 stars appear to match visually but fail mathematical criteria").arg(shouldMatchButDont);
        qDebug() << "  This suggests either:";
        qDebug() << "    • Pixel tolerance too strict for image quality";
        qDebug() << "    • Magnitude difference criteria too restrictive";
        qDebug() << "    • Systematic coordinate offset not accounted for";
        qDebug() << "    • Additional validation logic rejecting good matches";
    }
    
    if (unexpectedFailures > 0) {
        qDebug() << QString("\n🔍 SECONDARY ISSUE: %1 matches pass basic criteria but fail overall validation").arg(unexpectedFailures);
        qDebug() << "  Check for additional validation logic beyond distance/magnitude";
    }
}

void PixelMatchingDebugger::analyzeMatchingQuality()
{
    if (m_diagnostics.isEmpty()) return;
    
    qDebug() << "\n--- MATCHING QUALITY ANALYSIS ---";
    
    // Calculate overall matching efficiency
    int totalPotential = m_diagnostics.size();
    int withinSearchRadius = 0;
    int passDistance = 0;
    int passMagnitude = 0;
    int passOverall = 0;
    
    QVector<double> allDistances;
    double minDistance = 1000.0;
    double maxDistance = 0.0;
    
    for (const auto& diag : m_diagnostics) {
        allDistances.append(diag.pixelDistance);
        minDistance = std::min(minDistance, diag.pixelDistance);
        maxDistance = std::max(maxDistance, diag.pixelDistance);
        
        if (diag.pixelDistance <= 20.0) withinSearchRadius++; // Within search radius
        if (diag.passesDistanceCheck) passDistance++;
        if (diag.passesMagnitudeCheck) passMagnitude++;
        if (diag.passesOverallCheck) passOverall++;
    }
    
    if (!allDistances.isEmpty()) {
        std::sort(allDistances.begin(), allDistances.end());
        double medianDistance = allDistances[allDistances.size() / 2];
        double q25 = allDistances[allDistances.size() / 4];
        double q75 = allDistances[allDistances.size() * 3 / 4];
        double avgDistance = std::accumulate(allDistances.begin(), allDistances.end(), 0.0) / allDistances.size();
        
        qDebug() << QString("Distance quality metrics:");
        qDebug() << QString("  Range: %1 - %2 px").arg(minDistance).arg(maxDistance);
        qDebug() << QString("  Quartiles: Q25=%1, Median=%2, Q75=3 px").arg(q25).arg(medianDistance).arg(q75);
        qDebug() << QString("  Average: %1 px").arg(avgDistance);
        
        // Quality assessment
        if (medianDistance <= 3.0) {
            qDebug() << "✅ Distance quality: Excellent (median ≤ 3px)";
        } else if (medianDistance <= 5.0) {
            qDebug() << "✅ Distance quality: Good (median ≤ 5px)";
        } else if (medianDistance <= 8.0) {
            qDebug() << "⚠️  Distance quality: Fair (median ≤ 8px)";
        } else {
            qDebug() << "❌ Distance quality: Poor (median > 8px)";
        }
    }
    
    // Efficiency analysis
    qDebug() << QString("Matching efficiency:");
    if (totalPotential > 0) {
        qDebug() << QString("  Search efficiency: %1/%2 (%3%%) within search radius")
                    .arg(withinSearchRadius).arg(totalPotential).arg(100.0 * withinSearchRadius / totalPotential);
        qDebug() << QString("  Distance efficiency: %1/%2 (%3%%) pass distance criteria")
                    .arg(passDistance).arg(totalPotential).arg(100.0 * passDistance / totalPotential);
        qDebug() << QString("  Magnitude efficiency: %1/%2 (%3%%) pass magnitude criteria")
                    .arg(passMagnitude).arg(totalPotential).arg(100.0 * passMagnitude / totalPotential);
        qDebug() << QString("  Overall efficiency: %1/%2 (%3%%) pass all criteria")
                    .arg(passOverall).arg(totalPotential).arg(100.0 * passOverall / totalPotential);
    }
    
    // Bottleneck analysis
    qDebug() << QString("Bottleneck analysis:");
    double distanceEfficiency = (totalPotential > 0) ? (double)passDistance / totalPotential : 0.0;
    double magnitudeEfficiency = (totalPotential > 0) ? (double)passMagnitude / totalPotential : 0.0;
    double overallEfficiency = (totalPotential > 0) ? (double)passOverall / totalPotential : 0.0;
    
    if (distanceEfficiency < 0.5) {
        qDebug() << "🔍 BOTTLENECK: Distance criteria rejecting >50% of matches";
        qDebug() << "   → Consider increasing pixel tolerance";
        qDebug() << "   → Check for systematic coordinate offset";
    }
    
    if (magnitudeEfficiency < 0.7 && distanceEfficiency > 0.7) {
        qDebug() << "🔍 BOTTLENECK: Magnitude criteria rejecting many good distance matches";
        qDebug() << "   → Consider relaxing magnitude difference tolerance";
        qDebug() << "   → Check magnitude estimation accuracy";
    }
    
    if (overallEfficiency < distanceEfficiency * 0.8) {
        qDebug() << "🔍 BOTTLENECK: Additional validation logic rejecting good matches";
        qDebug() << "   → Review all validation criteria beyond distance/magnitude";
        qDebug() << "   → Check for overly strict quality filters";
    }
    
    // Quality grade
    if (overallEfficiency >= 0.7) {
        qDebug() << "🎯 MATCHING QUALITY: Excellent (≥70% efficiency)";
    } else if (overallEfficiency >= 0.5) {
        qDebug() << "🎯 MATCHING QUALITY: Good (≥50% efficiency)";
    } else if (overallEfficiency >= 0.3) {
        qDebug() << "🎯 MATCHING QUALITY: Fair (≥30% efficiency)";
    } else if (overallEfficiency >= 0.1) {
        qDebug() << "🎯 MATCHING QUALITY: Poor (≥10% efficiency)";
    } else {
        qDebug() << "🎯 MATCHING QUALITY: Critical (<10% efficiency)";
    }
}

void PixelMatchingDebugger::examineSpecificMatch(int detectedIndex, int catalogIndex)
{
    qDebug() << QString("\n=== EXAMINING SPECIFIC MATCH: det[%1] -> cat[%2] ===")
                .arg(detectedIndex).arg(catalogIndex);
    
    // Find this match in our diagnostics
    MatchDiagnostic* targetDiag = nullptr;
    for (auto& diag : m_diagnostics) {
        if (diag.detectedIndex == detectedIndex && diag.catalogIndex == catalogIndex) {
            targetDiag = &diag;
            break;
        }
    }
    
    if (!targetDiag) {
        qDebug() << "❌ Match not found in diagnostics";
        return;
    }
    
    qDebug() << QString("Match details:");
    qDebug() << QString("  Detected position: (%1, %2)")
                .arg(targetDiag->detectedPos.x()).arg(targetDiag->detectedPos.y());
    qDebug() << QString("  Catalog position: (%1, %2)")
                .arg(targetDiag->catalogPos.x()).arg(targetDiag->catalogPos.y());
    qDebug() << QString("  Catalog ID: %1").arg(targetDiag->catalogId);
    qDebug() << QString("  Catalog magnitude: %2").arg(targetDiag->magnitude);
    
    qDebug() << QString("Distance analysis:");
    qDebug() << QString("  Pixel distance: %1 px").arg(targetDiag->pixelDistance);
    qDebug() << QString("  Current tolerance: %1 px").arg(m_currentPixelTolerance);
    qDebug() << QString("  Passes distance check: %1").arg(targetDiag->passesDistanceCheck ? "YES" : "NO");
    
    qDebug() << QString("Magnitude analysis:");
    qDebug() << QString("  Magnitude difference: %1").arg(targetDiag->magnitudeDiff);
    qDebug() << QString("  Current tolerance: %1").arg(m_currentMagnitudeTolerance);
    qDebug() << QString("  Passes magnitude check: %1").arg(targetDiag->passesMagnitudeCheck ? "YES" : "NO");
    
    qDebug() << QString("Overall result:");
    qDebug() << QString("  Should visually match: %1").arg(targetDiag->shouldVisuallyMatch ? "YES" : "NO");
    qDebug() << QString("  Passes overall validation: %1").arg(targetDiag->passesOverallCheck ? "YES" : "NO");
    
    if (!targetDiag->failureReasons.isEmpty()) {
        qDebug() << QString("Failure reasons:");
        for (const QString& reason : targetDiag->failureReasons) {
            qDebug() << QString("  • %1").arg(reason);
        }
    }
    
    // Suggestions for this specific match
    qDebug() << QString("Suggestions:");
    if (targetDiag->pixelDistance <= 5.0 && !targetDiag->passesOverallCheck) {
        qDebug() << "  🔍 This is a close match that's failing - investigate validation logic";
    }
    if (targetDiag->shouldVisuallyMatch && !targetDiag->passesOverallCheck) {
        qDebug() << "  🎯 This should be a valid match - check criteria strictness";
    }
    if (targetDiag->passesDistanceCheck && targetDiag->passesMagnitudeCheck && !targetDiag->passesOverallCheck) {
        qDebug() << "  ⚠️  Passes basic checks but fails overall - additional validation issue";
    }
}

void PixelMatchingDebugger::findNearestCatalogStar(const QPoint& detectedPos, int searchRadius)
{
    qDebug() << QString("\n=== FINDING NEAREST CATALOG STARS TO (%1, %2) ===")
                .arg(detectedPos.x()).arg(detectedPos.y());
    
    struct NearbyMatch {
        int catalogIndex;
        QString catalogId;
        QPointF catalogPos;
        double magnitude;
        double distance;
    };
    
    QVector<NearbyMatch> nearbyStars;
    
    for (int i = 0; i < m_catalogStars.size(); ++i) {
        if (!m_catalogStars[i].isValid) continue;
        
        double distance = sqrt(pow(detectedPos.x() - m_catalogStars[i].pixelPos.x(), 2) +
                             pow(detectedPos.y() - m_catalogStars[i].pixelPos.y(), 2));
        
        if (distance <= searchRadius) {
            NearbyMatch match;
            match.catalogIndex = i;
            match.catalogId = m_catalogStars[i].id;
            match.catalogPos = m_catalogStars[i].pixelPos;
            match.magnitude = m_catalogStars[i].magnitude;
            match.distance = distance;
            nearbyStars.append(match);
        }
    }
    
    // Sort by distance
    std::sort(nearbyStars.begin(), nearbyStars.end(),
        [](const NearbyMatch& a, const NearbyMatch& b) {
            return a.distance < b.distance;
        });
    
    qDebug() << QString("Found %1 catalog stars within %2 px:")
                .arg(nearbyStars.size()).arg(searchRadius);
    
    for (int i = 0; i < std::min(10, (int)(nearbyStars.size())); ++i) {
        const auto& star = nearbyStars[i];
        qDebug() << QString("  %1. %2: (%3, %4) dist=%5 px, mag=%6")
                    .arg(i + 1).arg(star.catalogId)
                    .arg(star.catalogPos.x()).arg(star.catalogPos.y())
                    .arg(star.distance).arg(star.magnitude);
    }
    
    if (nearbyStars.isEmpty()) {
        qDebug() << QString("  No catalog stars found within %1 px").arg(searchRadius);
        qDebug() << "  Try increasing search radius or check coordinate transformations";
    } else if (nearbyStars.size() == 1) {
        qDebug() << "  Single nearest star found - good candidate for matching";
    } else {
        qDebug() << QString("  Multiple candidates - need distance/magnitude criteria to select best");
    }
}

void PixelMatchingDebugger::listAllMatchesInRadius(const QPoint& center, double radius)
{
    qDebug() << QString("\n=== ALL MATCHES WITHIN %1 px OF (%2, %3) ===")
                .arg(radius).arg(center.x()).arg(center.y());
    
    QVector<MatchDiagnostic> nearbyMatches;
    
    for (const auto& diag : m_diagnostics) {
        double distanceToCenter = sqrt(pow(diag.detectedPos.x() - center.x(), 2) +
                                     pow(diag.detectedPos.y() - center.y(), 2));
        
        if (distanceToCenter <= radius) {
            nearbyMatches.append(diag);
        }
    }
    
    // Sort by distance to center
    std::sort(nearbyMatches.begin(), nearbyMatches.end(),
        [&center](const MatchDiagnostic& a, const MatchDiagnostic& b) {
            double distA = sqrt(pow(a.detectedPos.x() - center.x(), 2) + pow(a.detectedPos.y() - center.y(), 2));
            double distB = sqrt(pow(b.detectedPos.x() - center.x(), 2) + pow(b.detectedPos.y() - center.y(), 2));
            return distA < distB;
        });
    
    qDebug() << QString("Found %1 matches in region:").arg(nearbyMatches.size());
    
    for (const auto& match : nearbyMatches) {
        QString status = match.passesOverallCheck ? "✅ PASS" : "❌ FAIL";
        qDebug() << QString("  det[%1] -> cat[%2] %3: dist=%4 px %5")
                    .arg(match.detectedIndex).arg(match.catalogIndex)
                    .arg(match.catalogId).arg(match.pixelDistance).arg(status);
        
        if (!match.passesOverallCheck && !match.failureReasons.isEmpty()) {
            qDebug() << QString("    Reasons: %1").arg(match.failureReasons.join(", "));
        }
    }
}

// Add these to your existing PixelMatchingDebugger.cpp file

#include "PixelMatchingDebugger.h"
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <algorithm>
#include <numeric>
#include <cmath>

PixelMatchingDebugger::PixelMatchingDebugger(QObject* parent) : QObject(parent)
{
    qDebug() << "PixelMatchingDebugger initialized";
}

void PixelMatchingDebugger::diagnoseMatching(const QVector<QPoint>& detectedStars,
                                           const QVector<CatalogStar>& catalogStars,
                                           const ValidationResult& result,
                                           const StarCatalogValidator* validator)
{
    qDebug() << "=== PIXEL MATCHING DIAGNOSIS ===";
    
    // Store data for analysis
    m_detectedStars = detectedStars;
    m_catalogStars = catalogStars;
    m_result = result;
    m_validator = validator;
    
    // Clear previous diagnostics
    m_diagnostics.clear();
    
    // Create diagnostics for all potential matches
    for (int i = 0; i < detectedStars.size(); ++i) {
        QPoint detectedPos = detectedStars[i];
        
        // Find closest catalog star
        double closestDistance = 1000.0;
        int closestCatalogIndex = -1;
        
        for (int j = 0; j < catalogStars.size(); ++j) {
            if (!catalogStars[j].isValid) continue;
            
            double distance = sqrt(pow(detectedPos.x() - catalogStars[j].pixelPos.x(), 2) +
                                 pow(detectedPos.y() - catalogStars[j].pixelPos.y(), 2));
            
            if (distance < closestDistance) {
                closestDistance = distance;
                closestCatalogIndex = j;
            }
        }
        
        if (closestCatalogIndex >= 0 && closestDistance <= 20.0) { // Within search radius
            MatchDiagnostic diag;
            diag.detectedIndex = i;
            diag.catalogIndex = closestCatalogIndex;
            diag.detectedPos = detectedPos;
            diag.catalogPos = catalogStars[closestCatalogIndex].pixelPos;
            diag.catalogId = catalogStars[closestCatalogIndex].id;
            diag.magnitude = catalogStars[closestCatalogIndex].magnitude;
            diag.pixelDistance = closestDistance;
            diag.magnitudeDiff = 0.0; // Would need detected star magnitude to calculate
            
            // Check criteria
            diag.passesDistanceCheck = (closestDistance <= m_currentPixelTolerance);
            diag.passesMagnitudeCheck = true; // Default to true without magnitude data
            
            // Check if this match appears in the results
            bool foundInResults = false;
            for (const auto& match : result.matches) {
                if (match.detectedIndex == i && match.catalogIndex == closestCatalogIndex) {
                    diag.passesOverallCheck = match.isGoodMatch;
                    diag.magnitudeDiff = match.magnitudeDiff;
                    diag.passesMagnitudeCheck = (match.magnitudeDiff <= m_currentMagnitudeTolerance);
                    foundInResults = true;
                    break;
                }
            }
            
            if (!foundInResults) {
                diag.passesOverallCheck = false;
                diag.failureReasons.append("Not found in results");
            }
            
            // Assess if this should visually match
            diag.shouldVisuallyMatch = (closestDistance <= 8.0); // Visual assessment
            
            // Add failure reasons
            if (!diag.passesDistanceCheck) {
                diag.failureReasons.append(QString("Distance %1 > tolerance %2")
                                         .arg(closestDistance).arg(m_currentPixelTolerance));
            }
            if (!diag.passesMagnitudeCheck) {
                diag.failureReasons.append(QString("MagDiff %1 > tolerance %2")
                                         .arg(diag.magnitudeDiff).arg(m_currentMagnitudeTolerance));
            }
            
            m_diagnostics.append(diag);
        }
    }
    
    qDebug() << QString("Created %1 match diagnostics").arg(m_diagnostics.size());
    
    // Run analysis
    calculateAllDistances();
    identifyProblematicMatches();
    checkForSystematicBias();
    analyzeMatchingQuality();
}

void PixelMatchingDebugger::analyzeDistanceCriteria(double pixelTolerance)
{
    if (m_diagnostics.isEmpty()) {
        qDebug() << "No diagnostic data available for distance analysis";
        return;
    }
    
    qDebug() << QString("=== DISTANCE CRITERIA ANALYSIS (tolerance: %1 px) ===").arg(pixelTolerance);
    
    QVector<double> distances;
    int withinTolerance = 0;
    int passingValidation = 0;
    int shouldMatchButDont = 0;
    
    for (const auto& diag : m_diagnostics) {
        distances.append(diag.pixelDistance);
        
        if (diag.pixelDistance <= pixelTolerance) {
            withinTolerance++;
        }
        
        if (diag.passesOverallCheck) {
            passingValidation++;
        }
        
        if (diag.pixelDistance <= pixelTolerance && !diag.passesOverallCheck) {
            shouldMatchButDont++;
        }
    }
    
    if (!distances.isEmpty()) {
        std::sort(distances.begin(), distances.end());
        double medianDistance = distances[distances.size() / 2];
        double q25 = distances[distances.size() / 4];
        double q75 = distances[distances.size() * 3 / 4];
        double avgDistance = std::accumulate(distances.begin(), distances.end(), 0.0) / distances.size();
        
        qDebug() << QString("Distance statistics:");
        qDebug() << QString("  Median: %1 px, Average: %2 px").arg(medianDistance).arg(avgDistance);
        qDebug() << QString("  Q25: %1 px, Q75: %2 px").arg(q25).arg(q75);
        qDebug() << QString("  Range: %1 - %2 px").arg(distances.first()).arg(distances.last());
        
        qDebug() << QString("Criteria efficiency:");
        qDebug() << QString("  Within tolerance: %1/%2 (%3%%)")
                    .arg(withinTolerance).arg(distances.size())
                    .arg(100.0 * withinTolerance / distances.size());
        qDebug() << QString("  Passing validation: %1/%2 (%3%%)")
                    .arg(passingValidation).arg(distances.size())
                    .arg(100.0 * passingValidation / distances.size());
        qDebug() << QString("  Should match but don't: %1").arg(shouldMatchButDont);
        
        if (shouldMatchButDont > 0) {
            qDebug() << QString("🔍 ISSUE: %1 stars within distance tolerance but failing validation")
                        .arg(shouldMatchButDont);
            qDebug() << "   Likely causes: magnitude criteria, additional validation logic";
        }
        
        // Suggest optimal tolerance
        if (medianDistance * 2.0 < pixelTolerance) {
            qDebug() << QString("💡 SUGGESTION: Current tolerance (%1) may be too loose. Try %2 px")
                        .arg(pixelTolerance).arg(medianDistance * 1.5);
        } else if (medianDistance > pixelTolerance) {
            qDebug() << QString("💡 SUGGESTION: Current tolerance (%1) may be too strict. Try %2 px")
                        .arg(pixelTolerance).arg(medianDistance * 1.2);
        }
    }
}

void PixelMatchingDebugger::findMissedMatches(double searchRadius)
{
    qDebug() << QString("=== FINDING MISSED MATCHES (search radius: %1 px) ===").arg(searchRadius);
    
    int missedCount = 0;
    int visualMisses = 0;
    
    for (const auto& diag : m_diagnostics) {
        if (diag.pixelDistance <= searchRadius && !diag.passesOverallCheck) {
            missedCount++;
            
            if (diag.shouldVisuallyMatch) {
                visualMisses++;
                
                if (visualMisses <= 5) { // Show first 5 cases
                    qDebug() << QString("  Visual miss %1: det[%2] -> cat[%3] %4")
                                .arg(visualMisses).arg(diag.detectedIndex)
                                .arg(diag.catalogIndex).arg(diag.catalogId);
                    qDebug() << QString("    Distance: %1 px, Reasons: %2")
                                .arg(diag.pixelDistance).arg(diag.failureReasons.join("; "));
                }
            }
        }
    }
    
    qDebug() << QString("Found %1 missed matches within %2 px radius")
                .arg(missedCount).arg(searchRadius);
    qDebug() << QString("Of these, %1 should match visually").arg(visualMisses);
    
    if (visualMisses > 0) {
        qDebug() << "🎯 ACTION NEEDED: Investigate why visually matching stars fail validation";
    }
}

void PixelMatchingDebugger::testToleranceRange(double minPixelTol, double maxPixelTol, double step)
{
    qDebug() << QString("\n=== TESTING TOLERANCE RANGE %1 - %2 px (step %3) ===")
                .arg(minPixelTol).arg(maxPixelTol).arg(step);
    
    if (m_diagnostics.isEmpty()) {
        qDebug() << "No diagnostic data available for tolerance testing";
        return;
    }
    
    struct ToleranceResult {
        double tolerance;
        int matchCount;
        int potentialMatches;
        double efficiency;
    };
    
    QVector<ToleranceResult> results;
    
    for (double tol = minPixelTol; tol <= maxPixelTol; tol += step) {
        ToleranceResult result;
        result.tolerance = tol;
        result.matchCount = 0;
        result.potentialMatches = 0;
        
        for (const auto& diag : m_diagnostics) {
            if (diag.pixelDistance <= tol) {
                result.potentialMatches++;
                // Simulate validation with this tolerance
                bool wouldPass = (diag.pixelDistance <= tol) && 
                               diag.passesMagnitudeCheck && 
                               !diag.failureReasons.contains("Not found in results");
                if (wouldPass) {
                    result.matchCount++;
                }
            }
        }
        
        result.efficiency = result.potentialMatches > 0 ? 
                          (double)result.matchCount / result.potentialMatches : 0.0;
        
        results.append(result);
        
        qDebug() << QString("  Tolerance %1 px: %2 matches/%3 potential (%4%% efficiency)")
                    .arg(tol).arg(result.matchCount).arg(result.potentialMatches)
                    .arg(result.efficiency * 100.0);
    }
    
    // Find optimal tolerance
    if (!results.isEmpty()) {
        auto optimalIt = std::max_element(results.begin(), results.end(),
            [](const ToleranceResult& a, const ToleranceResult& b) {
                // Optimize for balance of match count and efficiency
                double scoreA = a.matchCount * a.efficiency;
                double scoreB = b.matchCount * b.efficiency;
                return scoreA < scoreB;
            });
        
        qDebug() << QString("💡 OPTIMAL TOLERANCE: %1 px (%2 matches, %3%% efficiency)")
                    .arg(optimalIt->tolerance).arg(optimalIt->matchCount)
                    .arg(optimalIt->efficiency * 100.0);
    }
}


void PixelMatchingDebugger::printSummaryReport()
{
    qDebug() << "\n=== PIXEL MATCHING DEBUG SUMMARY ===";
    
    if (m_diagnostics.isEmpty()) {
        qDebug() << "No diagnostic data available";
        return;
    }
    
    MatchingStats stats = calculateStats();
    
    qDebug() << QString("Summary Statistics:");
    qDebug() << QString("  Detected stars: %1").arg(stats.totalDetected);
    qDebug() << QString("  Catalog stars: %1").arg(stats.totalCatalog);
    qDebug() << QString("  Potential matches (within search): %1").arg(stats.potentialMatches);
    qDebug() << QString("  Distance-passing matches: %1").arg(stats.distancePassingMatches);
    qDebug() << QString("  Actual matches: %1").arg(stats.actualMatches);
    
    if (stats.potentialMatches > 0) {
        double efficiency = (double)stats.actualMatches / stats.potentialMatches * 100.0;
        qDebug() << QString("  Overall efficiency: %1%%").arg(efficiency);
    }
    
    qDebug() << QString("Distance Analysis:");
    qDebug() << QString("  Average distance: %1 px").arg(stats.avgDistance);
    qDebug() << QString("  Median distance: %1 px").arg(stats.medianDistance);
    qDebug() << QString("  Maximum distance: %1 px").arg(stats.maxDistance);
    
    qDebug() << QString("Systematic Offset:");
    qDebug() << QString("  dx=%1 px, dy=%2 px").arg(stats.systematicOffset.x()).arg(stats.systematicOffset.y());
    double offsetMagnitude = sqrt(pow(stats.systematicOffset.x(), 2) + pow(stats.systematicOffset.y(), 2));
    qDebug() << QString("  Offset magnitude: %1 px").arg(offsetMagnitude);
    
    if (offsetMagnitude > 2.0) {
        qDebug() << "⚠️  LARGE SYSTEMATIC OFFSET - Check WCS calibration";
    } else if (offsetMagnitude > 1.0) {
        qDebug() << "⚠️  Moderate systematic offset - Consider recalibration";
    } else {
        qDebug() << "✅ Systematic offset within acceptable range";
    }
    
    // Count problematic cases
    int visualMismatches = 0;
    int closeButFailing = 0;
    
    for (const auto& diag : m_diagnostics) {
        if (diag.shouldVisuallyMatch && !diag.passesOverallCheck) {
            visualMismatches++;
        }
        if (diag.pixelDistance <= m_currentPixelTolerance && !diag.passesOverallCheck) {
            closeButFailing++;
        }
    }
    
    qDebug() << QString("Problem Identification:");
    qDebug() << QString("  Visual mismatches: %1").arg(visualMismatches);
    qDebug() << QString("  Close but failing: %1").arg(closeButFailing);
    
    if (visualMismatches > 0 || closeButFailing > 0) {
        qDebug() << "\
🔍 PRIMARY ISSUES DETECTED:";
        if (visualMismatches > 0) {
            qDebug() << QString("  • %1 stars that should match visually are failing validation").arg(visualMismatches);
        }
        if (closeButFailing > 0) {
            qDebug() << QString("  • %1 stars within pixel tolerance are failing validation").arg(closeButFailing);
        }
        qDebug() << "\
💡 RECOMMENDED ACTIONS:";
        qDebug() << "  1. Check magnitude difference criteria";
        qDebug() << "  2. Review additional validation logic";
        qDebug() << "  3. Consider increasing pixel tolerance";
        qDebug() << "  4. Verify coordinate transformations";
    } else {
        qDebug() << "✅ No major matching issues detected";
    }
}

PixelMatchingDebugger::MatchingStats PixelMatchingDebugger::calculateStats()
{
    MatchingStats stats;
    
    if (m_diagnostics.isEmpty()) {
        return stats;
    }
    
    stats.totalDetected = m_detectedStars.size();
    stats.totalCatalog = m_catalogStars.size();
    stats.potentialMatches = m_diagnostics.size();
    
    QVector<double> distances;
    double sumDx = 0, sumDy = 0;
    int offsetCount = 0;
    
    for (const auto& diag : m_diagnostics) {
        distances.append(diag.pixelDistance);
        
        if (diag.passesDistanceCheck) {
            stats.distancePassingMatches++;
        }
        if (diag.passesMagnitudeCheck) {
            stats.magnitudePassingMatches++;
        }
        if (diag.passesOverallCheck) {
            stats.actualMatches++;
        }
        
        // Calculate systematic offset
        sumDx += diag.detectedPos.x() - diag.catalogPos.x();
        sumDy += diag.detectedPos.y() - diag.catalogPos.y();
        offsetCount++;
    }
    
    if (!distances.isEmpty()) {
        stats.avgDistance = std::accumulate(distances.begin(), distances.end(), 0.0) / distances.size();
        stats.maxDistance = *std::max_element(distances.begin(), distances.end());
        
        std::sort(distances.begin(), distances.end());
        stats.medianDistance = distances[distances.size() / 2];
    }
    
    if (offsetCount > 0) {
        stats.systematicOffset = QPointF(sumDx / offsetCount, sumDy / offsetCount);
    }
    
    return stats;
}

void PixelMatchingDebugger::checkForSystematicBias()
{

}

