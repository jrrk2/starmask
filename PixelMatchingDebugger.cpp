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
                qDebug() << QString("⚠️  Close but failing: det[%1] -> cat[%2] %3, dist=%.2f px")
                            .arg(diag.detectedIndex).arg(diag.catalogIndex)
                            .arg(diag.catalogId).arg(diag.pixelDistance);
                qDebug() << QString("    Reasons: %1").arg(diag.failureReasons.join(", "));
            }
        }
        
        // Far matches that somehow pass
        if (diag.pixelDistance > m_currentPixelTolerance * 1.5 && diag.passesOverallCheck) {
            farButPassing++;
            qDebug() << QString("⚠️  Far but passing: det[%1] -> cat[%2] %3, dist=%.2f px")
                        .arg(diag.detectedIndex).arg(diag.catalogIndex)
                        .arg(diag.catalogId).arg(diag.pixelDistance);
        }
        
        // Visual matches that fail mathematically (the main issue)
        if (diag.shouldVisuallyMatch && !diag.passesOverallCheck) {
            shouldMatchButDont++;
            if (shouldMatchButDont <= 5) {
                qDebug() << QString("🎯 Visual mismatch: det[%1] -> cat[%2] %3, dist=%.2f px")
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
        qDebug__ << "  This suggests either:";
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
        qDebug() << QString("  Range: %.2f - %.2f px").arg(minDistance).arg(maxDistance);
        qDebug() << QString("  Quartiles: Q25=%.2f, Median=%.2f, Q75=%.2f px").arg(q25).arg(medianDistance).arg(q75);
        qDebug() << QString("  Average: %.2f px").arg(avgDistance);
        
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
        qDebug() << QString("  Search efficiency: %1/%2 (%.1f%%) within search radius")
                    .arg(withinSearchRadius).arg(totalPotential).arg(100.0 * withinSearchRadius / totalPotential);
        qDebug() << QString("  Distance efficiency: %1/%2 (%.1f%%) pass distance criteria")
                    .arg(passDistance).arg(totalPotential).arg(100.0 * passDistance / totalPotential);
        qDebug() << QString("  Magnitude efficiency: %1/%2 (%.1f%%) pass magnitude criteria")
                    .arg(passMagnitude).arg(totalPotential).arg(100.0 * passMagnitude / totalPotential);
        qDebug() << QString("  Overall efficiency: %1/%2 (%.1f%%) pass all criteria")
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
    qDebug() << QString("  Catalog position: (%.2f, %.2f)")
                .arg(targetDiag->catalogPos.x()).arg(targetDiag->catalogPos.y());
    qDebug() << QString("  Catalog ID: %1").arg(targetDiag->catalogId);
    qDebug() << QString("  Catalog magnitude: %.2f").arg(targetDiag->magnitude);
    
    qDebug() << QString("Distance analysis:");
    qDebug() << QString("  Pixel distance: %.3f px").arg(targetDiag->pixelDistance);
    qDebug() << QString("  Current tolerance: %.1f px").arg(m_currentPixelTolerance);
    qDebug() << QString("  Passes distance check: %1").arg(targetDiag->passesDistanceCheck ? "YES" : "NO");
    
    qDebug() << QString("Magnitude analysis:");
    qDebug() << QString("  Magnitude difference: %.3f").arg(targetDiag->magnitudeDiff);
    qDebug() << QString("  Current tolerance: %.1f").arg(m_currentMagnitudeTolerance);
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
    
    for (int i = 0; i < std::min(10, nearbyStars.size()); ++i) {
        const auto& star = nearbyStars[i];
        qDebug() << QString("  %1. %2: (%.1f, %.1f) dist=%.2f px, mag=%.1f")
                    .arg(i + 1).arg(star.catalogId)
                    .arg(star.catalogPos.x()).arg(star.catalogPos.y())
                    .arg(star.distance).arg(star.magnitude);
    }
    
    if (nearbyStars.isEmpty()) {
        qDebug() << QString("  No catalog stars found within %1 px").arg(searchRadius);
        qDebug__ << "  Try increasing search radius or check coordinate transformations";
    } else if (nearbyStars.size() == 1) {
        qDebug() << "  Single nearest star found - good candidate for matching";
    } else {
        qDebug() << QString("  Multiple candidates - need distance/magnitude criteria to select best");
    }
}

void PixelMatchingDebugger::listAllMatchesInRadius(const QPoint& center, double radius)
{
    qDebug() << QString("\n=== ALL MATCHES WITHIN %.1f px OF (%2, %3) ===")
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
        qDebug() << QString("  det[%1] -> cat[%2] %3: dist=%.2f px %4")
                    .arg(match.detectedIndex).arg(match.catalogIndex)
                    .arg(match.catalogId).arg(match.pixelDistance).arg(status);
        
        if (!match.passesOverallCheck && !match.failureReasons.isEmpty()) {
            qDebug() << QString("    Reasons: %1").arg(match.failureReasons.join(", "));
        }
    }
}