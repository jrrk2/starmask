// GeometricAlgorithms.h - Advanced geometric algorithms for star matching
#ifndef GEOMETRIC_ALGORITHMS_H
#define GEOMETRIC_ALGORITHMS_H

#include <QVector>
#include <QPointF>
#include <QMatrix3x3>
#include <pcl/Matrix.h>
#include <pcl/Vector.h>
#include <cmath>

namespace GeometricAlgorithms {

// RANSAC-based robust estimation
struct RANSACParameters {
    int maxIterations = 1000;           // Maximum RANSAC iterations
    double inlierThreshold = 2.0;       // Inlier threshold in pixels
    double minInlierRatio = 0.6;        // Minimum inlier ratio for valid model
    int minSampleSize = 3;              // Minimum samples for model estimation
    double confidence = 0.99;           // Desired confidence level
};

struct TransformationModel {
    QMatrix3x3 transformMatrix;         // 2D transformation matrix
    QVector<int> inlierIndices;         // Indices of inlier points
    double rmsError = 0.0;              // RMS error of inliers
    int numInliers = 0;                 // Number of inliers
    bool isValid = false;               // Whether model is valid
    
    // Transformation parameters
    double scaleX = 1.0, scaleY = 1.0;  // Scale factors
    double rotation = 0.0;              // Rotation angle (radians)
    double translationX = 0.0, translationY = 0.0; // Translation
    double skew = 0.0;                  // Skew parameter
};

class RobustMatcher {
public:
    // RANSAC-based transformation estimation
    static TransformationModel estimateTransformation(
        const QVector<QPointF>& sourcePoints,
        const QVector<QPointF>& targetPoints,
        const RANSACParameters& params = RANSACParameters());
    
    // Estimate affine transformation (6 parameters: scale, rotation, translation, skew)
    static TransformationModel estimateAffineTransformation(
        const QVector<QPointF>& sourcePoints,
        const QVector<QPointF>& targetPoints,
        const RANSACParameters& params = RANSACParameters());
    
    // Estimate similarity transformation (4 parameters: scale, rotation, translation)
    static TransformationModel estimateSimilarityTransformation(
        const QVector<QPointF>& sourcePoints,
        const QVector<QPointF>& targetPoints,
        const RANSACParameters& params = RANSACParameters());
    
    // Apply transformation to points
    static QVector<QPointF> transformPoints(
        const QVector<QPointF>& points,
        const TransformationModel& model);
    
    // Calculate residuals after transformation
    static QVector<double> calculateResiduals(
        const QVector<QPointF>& sourcePoints,
        const QVector<QPointF>& targetPoints,
        const TransformationModel& model);

private:
    // Internal RANSAC implementation
    static TransformationModel runRANSAC(
        const QVector<QPointF>& sourcePoints,
        const QVector<QPointF>& targetPoints,
        const RANSACParameters& params,
        std::function<TransformationModel(const QVector<QPointF>&, const QVector<QPointF>&)> estimator);
    
    // Specific transformation estimators
    static TransformationModel estimateAffineFromPoints(
        const QVector<QPointF>& sourcePoints,
        const QVector<QPointF>& targetPoints);
    
    static TransformationModel estimateSimilarityFromPoints(
        const QVector<QPointF>& sourcePoints,
        const QVector<QPointF>& targetPoints);
};

// Advanced pattern matching algorithms
class PatternMatcher {
public:
    // Geometric hashing for pattern recognition
    struct HashEntry {
        QVector<int> starIndices;       // Stars forming the pattern
        QPointF referencePoint;         // Reference point for the pattern
        double scale;                   // Pattern scale
        double rotation;                // Pattern rotation
    };
    
    // Build geometric hash table from star positions
    static QHash<QString, QVector<HashEntry>> buildGeometricHashTable(
        const QVector<QPointF>& stars,
        double binSize = 1.0);
    
    // Match patterns using geometric hashing
    static QVector<QPair<int, int>> matchPatternsWithHashing(
        const QVector<QPointF>& pattern1,
        const QVector<QPointF>& pattern2,
        double tolerance = 0.1);
    
    // Improved triangle matching with invariant features
    static QVector<QPair<int, int>> matchTrianglesRobust(
        const QVector<QPointF>& stars1,
        const QVector<QPointF>& stars2,
        double tolerance = 0.05);
    
    // Constellation matching using star brightness
    static QVector<QPair<int, int>> matchConstellations(
        const QVector<QPointF>& positions1,
        const QVector<double>& magnitudes1,
        const QVector<QPointF>& positions2,
        const QVector<double>& magnitudes2,
        double positionTolerance = 5.0,
        double magnitudeTolerance = 1.0);

private:
    // Helper functions for geometric hashing
    static QString computeHashKey(const QPointF& point, double binSize);
    static QVector<QPointF> normalizePattern(const QVector<QPointF>& pattern);
};

// Statistical validation algorithms
class StatisticalValidator {
public:
    // Validate matches using statistical tests
    struct ValidationResult {
        bool isValid = false;
        double pValue = 0.0;            // Statistical p-value
        double chiSquared = 0.0;        // Chi-squared statistic
        int degreesOfFreedom = 0;       // Degrees of freedom
        QString testName;               // Name of statistical test used
    };
    
    // Chi-squared test for goodness of fit
    static ValidationResult chiSquaredTest(
        const QVector<double>& residuals,
        double expectedVariance = 1.0);
    
    // F-test for comparing variances
    static ValidationResult fTest(
        const QVector<double>& residuals1,
        const QVector<double>& residuals2);
    
    // Kolmogorov-Smirnov test for distribution comparison
    static ValidationResult ksTest(
        const QVector<double>& sample1,
        const QVector<double>& sample2);
    
    // Outlier detection using Mahalanobis distance
    static QVector<bool> detectOutliers(
        const QVector<QPointF>& residuals,
        double threshold = 3.0);
    
    // Calculate match quality score
    static double calculateMatchQuality(
        const QVector<double>& distances,
        const QVector<double>& magnitudeDiffs,
        const QVector<bool>& geometricValid);

private:
    // Statistical helper functions
    static double calculateMean(const QVector<double>& values);
    static double calculateStandardDeviation(const QVector<double>& values);
    static double calculateMahalanobisDistance(const QPointF& point, 
                                              const QPointF& mean, 
                                              const QMatrix3x3& covariance);
};

// Distortion modeling algorithms
class DistortionModeler {
public:
    // Different distortion models
    enum DistortionType {
        RADIAL_POLYNOMIAL,      // Standard radial polynomial
        RADIAL_RATIONAL,        // Rational function model
        TANGENTIAL,             // Brown-Conrady model
        COMBINED               // Combined radial + tangential
    };
    
    struct DistortionParameters {
        DistortionType type = COMBINED;
        QPointF principalPoint;         // Principal point (optical center)
        
        // Radial distortion coefficients
        double k1 = 0.0, k2 = 0.0, k3 = 0.0;
        
        // Tangential distortion coefficients  
        double p1 = 0.0, p2 = 0.0;
        
        // Higher order coefficients (if needed)
        double k4 = 0.0, k5 = 0.0, k6 = 0.0;
        
        // Quality metrics
        double rmsError = 0.0;
        int numPointsUsed = 0;
        bool isCalibrated = false;
    };
    
    // Calibrate distortion model from point correspondences
    static DistortionParameters calibrateDistortion(
        const QVector<QPointF>& imagePoints,
        const QVector<QPointF>& worldPoints,
        DistortionType type = COMBINED,
        const QPointF& initialPrincipalPoint = QPointF());
    
    // Apply distortion correction
    static QPointF correctDistortion(const QPointF& distortedPoint,
                                   const DistortionParameters& params);
    
    // Apply distortion (for validation)
    static QPointF applyDistortion(const QPointF& undistortedPoint,
                                 const DistortionParameters& params);
    
    // Validate distortion model
    static double validateModel(const DistortionParameters& params,
                              const QVector<QPointF>& testImagePoints,
                              const QVector<QPointF>& testWorldPoints);

private:
    // Internal distortion calculation functions
    static QPointF calculateRadialDistortion(const QPointF& point,
                                            const QPointF& center,
                                            double k1, double k2, double k3 = 0.0);
    
    static QPointF calculateTangentialDistortion(const QPointF& point,
                                                const QPointF& center,
                                                double p1, double p2);
    
    // Levenberg-Marquardt optimization for parameter fitting
    static DistortionParameters optimizeParameters(
        const QVector<QPointF>& imagePoints,
        const QVector<QPointF>& worldPoints,
        const DistortionParameters& initialParams);
};

} // namespace GeometricAlgorithms

// Implementation file: GeometricAlgorithms.cpp
#include "GeometricAlgorithms.h"
#include <QRandomGenerator>
#include <algorithm>
#include <numeric>

namespace GeometricAlgorithms {

TransformationModel RobustMatcher::estimateTransformation(
    const QVector<QPointF>& sourcePoints,
    const QVector<QPointF>& targetPoints,
    const RANSACParameters& params)
{
    if (sourcePoints.size() != targetPoints.size() || sourcePoints.size() < params.minSampleSize) {
        return TransformationModel(); // Invalid input
    }
    
    // Default to affine transformation
    return estimateAffineTransformation(sourcePoints, targetPoints, params);
}

TransformationModel RobustMatcher::estimateAffineTransformation(
    const QVector<QPointF>& sourcePoints,
    const QVector<QPointF>& targetPoints,
    const RANSACParameters& params)
{
    auto estimator = [](const QVector<QPointF>& src, const QVector<QPointF>& tgt) -> TransformationModel {
        return estimateAffineFromPoints(src, tgt);
    };
    
    return runRANSAC(sourcePoints, targetPoints, params, estimator);
}

TransformationModel RobustMatcher::estimateSimilarityTransformation(
    const QVector<QPointF>& sourcePoints,
    const QVector<QPointF>& targetPoints,
    const RANSACParameters& params)
{
    auto estimator = [](const QVector<QPointF>& src, const QVector<QPointF>& tgt) -> TransformationModel {
        return estimateSimilarityFromPoints(src, tgt);
    };
    
    return runRANSAC(sourcePoints, targetPoints, params, estimator);
}

TransformationModel RobustMatcher::runRANSAC(
    const QVector<QPointF>& sourcePoints,
    const QVector<QPointF>& targetPoints,
    const RANSACParameters& params,
    std::function<TransformationModel(const QVector<QPointF>&, const QVector<QPointF>&)> estimator)
{
    TransformationModel bestModel;
    int maxInliers = 0;
    
    QRandomGenerator* rng = QRandomGenerator::global();
    
    for (int iteration = 0; iteration < params.maxIterations; ++iteration) {
        // Select random sample
        QVector<int> sampleIndices;
        while (sampleIndices.size() < params.minSampleSize) {
            int idx = rng->bounded(sourcePoints.size());
            if (!sampleIndices.contains(idx)) {
                sampleIndices.append(idx);
            }
        }
        
        // Extract sample points
        QVector<QPointF> sampleSource, sampleTarget;
        for (int idx : sampleIndices) {
            sampleSource.append(sourcePoints[idx]);
            sampleTarget.append(targetPoints[idx]);
        }
        
        // Estimate transformation from sample
        TransformationModel candidateModel = estimator(sampleSource, sampleTarget);
        if (!candidateModel.isValid) continue;
        
        // Count inliers
        QVector<int> inliers;
        QVector<double> residuals = calculateResiduals(sourcePoints, targetPoints, candidateModel);
        
        for (int i = 0; i < residuals.size(); ++i) {
            if (residuals[i] < params.inlierThreshold) {
                inliers.append(i);
            }
        }
        
        // Check if this is the best model so far
        if (inliers.size() > maxInliers) {
            maxInliers = inliers.size();
            bestModel = candidateModel;
            bestModel.inlierIndices = inliers;
            bestModel.numInliers = inliers.size();
            
            // Calculate RMS error for inliers
            double sumSquaredErrors = 0.0;
            for (int idx : inliers) {
                sumSquaredErrors += residuals[idx] * residuals[idx];
            }
            bestModel.rmsError = sqrt(sumSquaredErrors / inliers.size());
        }
        
        // Early termination if we have enough inliers
        double inlierRatio = static_cast<double>(inliers.size()) / sourcePoints.size();
        if (inlierRatio > params.minInlierRatio) {
            break;
        }
    }
    
    // Final validation
    double finalInlierRatio = static_cast<double>(bestModel.numInliers) / sourcePoints.size();
    bestModel.isValid = (finalInlierRatio >= params.minInlierRatio) && 
                       (bestModel.numInliers >= params.minSampleSize);
    
    return bestModel;
}

TransformationModel RobustMatcher::estimateAffineFromPoints(
    const QVector<QPointF>& sourcePoints,
    const QVector<QPointF>& targetPoints)
{
    TransformationModel model;
    
    if (sourcePoints.size() < 3 || sourcePoints.size() != targetPoints.size()) {
        return model; // Invalid
    }
    
    try {
        // Set up least squares system for affine transformation
        // [x'] = [a b tx] [x]
        // [y'] = [c d ty] [y]
        // [1 ] = [0 0 1 ] [1]
        
        int n = sourcePoints.size();
        pcl::Matrix A(2 * n, 6);
        pcl::Vector b(2 * n);
        
        for (int i = 0; i < n; ++i) {
            const QPointF& src = sourcePoints[i];
            const QPointF& tgt = targetPoints[i];
            
            // Equation for x coordinate
            A[2*i][0] = src.x();    // a
            A[2*i][1] = src.y();    // b
            A[2*i][2] = 1.0;        // tx
            A[2*i][3] = 0.0;        // c
            A[2*i][4] = 0.0;        // d
            A[2*i][5] = 0.0;        // ty
            b[2*i] = tgt.x();
            
            // Equation for y coordinate
            A[2*i+1][0] = 0.0;      // a
            A[2*i+1][1] = 0.0;      // b
            A[2*i+1][2] = 0.0;      // tx
            A[2*i+1][3] = src.x();  // c
            A[2*i+1][4] = src.y();  // d
            A[2*i+1][5] = 1.0;      // ty
            b[2*i+1] = tgt.y();
        }
        
        // Solve least squares: (A^T * A) * x = A^T * b
        pcl::Matrix AtA = A.Transpose() * A;
        pcl::Vector Atb = A.Transpose() * b;
        pcl::Vector solution = AtA.Inverse() * Atb;
        
        // Extract transformation parameters
        double a = solution[0];   // Scale/rotation x component
        double b = solution[1];   // Skew/rotation component
        double tx = solution[2];  // Translation x
        double c = solution[3];   // Skew/rotation component
        double d = solution[4];   // Scale/rotation y component
        double ty = solution[5];  // Translation y
        
        // Build transformation matrix
        model.transformMatrix(0, 0) = a;   model.transformMatrix(0, 1) = b;   model.transformMatrix(0, 2) = tx;
        model.transformMatrix(1, 0) = c;   model.transformMatrix(1, 1) = d;   model.transformMatrix(1, 2) = ty;
        model.transformMatrix(2, 0) = 0.0; model.transformMatrix(2, 1) = 0.0; model.transformMatrix(2, 2) = 1.0;
        
        // Extract geometric parameters
        model.scaleX = sqrt(a*a + c*c);
        model.scaleY = sqrt(b*b + d*d);
        model.rotation = atan2(c, a);
        model.translationX = tx;
        model.translationY = ty;
        model.skew = atan2(b, d) - model.rotation;
        
        model.isValid = true;
        
    } catch (...) {
        model.isValid = false;
    }
    
    return model;
}

TransformationModel RobustMatcher::estimateSimilarityFromPoints(
    const QVector<QPointF>& sourcePoints,
    const QVector<QPointF>& targetPoints)
{
    TransformationModel model;
    
    if (sourcePoints.size() < 2 || sourcePoints.size() != targetPoints.size()) {
        return model; // Invalid
    }
    
    try {
        // Similarity transformation: scale + rotation + translation
        // [x'] = [s*cos(θ) -s*sin(θ) tx] [x]
        // [y'] = [s*sin(θ)  s*cos(θ) ty] [y]
        // [1 ] = [0         0        1 ] [1]
        
        int n = sourcePoints.size();
        pcl::Matrix A(2 * n, 4);
        pcl::Vector b(2 * n);
        
        for (int i = 0; i < n; ++i) {
            const QPointF& src = sourcePoints[i];
            const QPointF& tgt = targetPoints[i];
            
            // Equation for x coordinate: x' = a*x - b*y + tx
            A[2*i][0] = src.x();    // a = s*cos(θ)
            A[2*i][1] = -src.y();   // -b = -s*sin(θ)
            A[2*i][2] = 1.0;        // tx
            A[2*i][3] = 0.0;
            b[2*i] = tgt.x();
            
            // Equation for y coordinate: y' = b*x + a*y + ty
            A[2*i+1][0] = src.y();  // a = s*cos(θ)
            A[2*i+1][1] = src.x();  // b = s*sin(θ)
            A[2*i+1][2] = 0.0;
            A[2*i+1][3] = 1.0;      // ty
            b[2*i+1] = tgt.y();
        }
        
        // Solve least squares
        pcl::Matrix AtA = A.Transpose() * A;
        pcl::Vector Atb = A.Transpose() * b;
        pcl::Vector solution = AtA.Inverse() * Atb;
        
        // Extract parameters
        double a = solution[0];   // s*cos(θ)
        double b = solution[1];   // s*sin(θ)
        double tx = solution[2];
        double ty = solution[3];
        
        // Calculate scale and rotation
        double scale = sqrt(a*a + b*b);
        double rotation = atan2(b, a);
        
        // Build transformation matrix
        model.transformMatrix(0, 0) = a;   model.transformMatrix(0, 1) = -b;  model.transformMatrix(0, 2) = tx;
        model.transformMatrix(1, 0) = b;   model.transformMatrix(1, 1) = a;   model.transformMatrix(1, 2) = ty;
        model.transformMatrix(2, 0) = 0.0; model.transformMatrix(2, 1) = 0.0; model.transformMatrix(2, 2) = 1.0;
        
        // Store parameters
        model.scaleX = model.scaleY = scale;
        model.rotation = rotation;
        model.translationX = tx;
        model.translationY = ty;
        model.skew = 0.0; // No skew in similarity transformation
        
        model.isValid = true;
        
    } catch (...) {
        model.isValid = false;
    }
    
    return model;
}

QVector<QPointF> RobustMatcher::transformPoints(
    const QVector<QPointF>& points,
    const TransformationModel& model)
{
    QVector<QPointF> transformedPoints;
    transformedPoints.reserve(points.size());
    
    for (const QPointF& point : points) {
        // Apply transformation matrix
        double x = model.transformMatrix(0, 0) * point.x() + 
                  model.transformMatrix(0, 1) * point.y() + 
                  model.transformMatrix(0, 2);
                  
        double y = model.transformMatrix(1, 0) * point.x() + 
                  model.transformMatrix(1, 1) * point.y() + 
                  model.transformMatrix(1, 2);
        
        transformedPoints.append(QPointF(x, y));
    }
    
    return transformedPoints;
}

QVector<double> RobustMatcher::calculateResiduals(
    const QVector<QPointF>& sourcePoints,
    const QVector<QPointF>& targetPoints,
    const TransformationModel& model)
{
    QVector<double> residuals;
    QVector<QPointF> transformedPoints = transformPoints(sourcePoints, model);
    
    for (int i = 0; i < transformedPoints.size(); ++i) {
        double dx = transformedPoints[i].x() - targetPoints[i].x();
        double dy = transformedPoints[i].y() - targetPoints[i].y();
        residuals.append(sqrt(dx*dx + dy*dy));
    }
    
    return residuals;
}

// Pattern matching implementation
QVector<QPair<int, int>> PatternMatcher::matchTrianglesRobust(
    const QVector<QPointF>& stars1,
    const QVector<QPointF>& stars2,
    double tolerance)
{
    QVector<QPair<int, int>> matches;
    
    if (stars1.size() < 3 || stars2.size() < 3) {
        return matches;
    }
    
    // Generate triangle patterns for both star sets
    struct TriangleDescriptor {
        QVector<int> starIndices;
        double side1, side2, side3;
        double perimeter;
        double area;
        
        // Scale-invariant descriptors
        double ratio12, ratio13, ratio23;
        double normalizedArea;
        
        void calculateInvariants() {
            perimeter = side1 + side2 + side3;
            double maxSide = std::max({side1, side2, side3});
            
            ratio12 = side1 / maxSide;
            ratio13 = side2 / maxSide;
            ratio23 = side3 / maxSide;
            
            normalizedArea = area / (perimeter * perimeter);
        }
        
        double similarity(const TriangleDescriptor& other) const {
            double ratioError = std::abs(ratio12 - other.ratio12) +
                               std::abs(ratio13 - other.ratio13) +
                               std::abs(ratio23 - other.ratio23);
            double areaError = std::abs(normalizedArea - other.normalizedArea);
            
            return (ratioError / 3.0) + areaError;
        }
    };
    
    auto generateTriangles = [](const QVector<QPointF>& stars) -> QVector<TriangleDescriptor> {
        QVector<TriangleDescriptor> triangles;
        
        for (int i = 0; i < stars.size() - 2; ++i) {
            for (int j = i + 1; j < stars.size() - 1; ++j) {
                for (int k = j + 1; k < stars.size(); ++k) {
                    TriangleDescriptor triangle;
                    triangle.starIndices = {i, j, k};
                    
                    const QPointF& p1 = stars[i];
                    const QPointF& p2 = stars[j];
                    const QPointF& p3 = stars[k];
                    
                    // Calculate side lengths
                    triangle.side1 = sqrt(pow(p2.x() - p1.x(), 2) + pow(p2.y() - p1.y(), 2));
                    triangle.side2 = sqrt(pow(p3.x() - p2.x(), 2) + pow(p3.y() - p2.y(), 2));
                    triangle.side3 = sqrt(pow(p1.x() - p3.x(), 2) + pow(p1.y() - p3.y(), 2));
                    
                    // Skip degenerate triangles
                    if (triangle.side1 < 5.0 || triangle.side2 < 5.0 || triangle.side3 < 5.0) {
                        continue;
                    }
                    
                    // Calculate area using cross product
                    triangle.area = 0.5 * std::abs((p2.x() - p1.x()) * (p3.y() - p1.y()) - 
                                                  (p3.x() - p1.x()) * (p2.y() - p1.y()));
                    
                    // Skip very thin triangles
                    double perimeter = triangle.side1 + triangle.side2 + triangle.side3;
                    if (triangle.area < 0.1 * perimeter) continue;
                    
                    triangle.calculateInvariants();
                    triangles.append(triangle);
                }
            }
        }
        
        return triangles;
    };
    
    QVector<TriangleDescriptor> triangles1 = generateTriangles(stars1);
    QVector<TriangleDescriptor> triangles2 = generateTriangles(stars2);
    
    // Match triangles based on invariant descriptors
    QVector<QPair<int, int>> triangleMatches;
    
    for (int i = 0; i < triangles1.size(); ++i) {
        for (int j = 0; j < triangles2.size(); ++j) {
            double similarity = triangles1[i].similarity(triangles2[j]);
            
            if (similarity < tolerance) {
                triangleMatches.append(qMakePair(i, j));
            }
        }
    }
    
    // Convert triangle matches to star matches
    QMap<QPair<int, int>, int> starMatchVotes;
    
    for (const auto& triangleMatch : triangleMatches) {
        const TriangleDescriptor& tri1 = triangles1[triangleMatch.first];
        const TriangleDescriptor& tri2 = triangles2[triangleMatch.second];
        
        // Each triangle gives us 3 potential star matches
        for (int k = 0; k < 3; ++k) {
            QPair<int, int> starMatch(tri1.starIndices[k], tri2.starIndices[k]);
            starMatchVotes[starMatch]++;
        }
    }
    
    // Select star matches with sufficient votes
    int minVotes = std::max(1, triangleMatches.size() / 5); // At least 20% of triangle matches
    
    for (auto it = starMatchVotes.begin(); it != starMatchVotes.end(); ++it) {
        if (it.value() >= minVotes) {
            matches.append(it.key());
        }
    }
    
    return matches;
}

// Statistical validation implementation
StatisticalValidator::ValidationResult StatisticalValidator::chiSquaredTest(
    const QVector<double>& residuals,
    double expectedVariance)
{
    ValidationResult result;
    result.testName = "Chi-Squared Goodness of Fit";
    
    if (residuals.size() < 3) {
        return result; // Insufficient data
    }
    
    // Calculate chi-squared statistic
    double sumSquaredResiduals = 0.0;
    for (double residual : residuals) {
        sumSquaredResiduals += residual * residual;
    }
    
    result.chiSquared = sumSquaredResiduals / expectedVariance;
    result.degreesOfFreedom = residuals.size() - 1;
    
    // Simple p-value approximation (for more accuracy, use proper chi-squared distribution)
    // This is a rough approximation - in practice, you'd use a statistical library
    double criticalValue = result.degreesOfFreedom + 2.0 * sqrt(2.0 * result.degreesOfFreedom);
    result.pValue = (result.chiSquared < criticalValue) ? 0.95 : 0.05; // Rough estimate
    
    result.isValid = result.pValue > 0.05; // 5% significance level
    
    return result;
}

QVector<bool> StatisticalValidator::detectOutliers(
    const QVector<QPointF>& residuals,
    double threshold)
{
    QVector<bool> isOutlier(residuals.size(), false);
    
    if (residuals.size() < 3) {
        return isOutlier;
    }
    
    // Calculate mean and covariance of residuals
    QPointF mean(0.0, 0.0);
    for (const QPointF& residual : residuals) {
        mean += residual;
    }
    mean /= residuals.size();
    
    // Calculate covariance matrix
    double cxx = 0.0, cxy = 0.0, cyy = 0.0;
    for (const QPointF& residual : residuals) {
        double dx = residual.x() - mean.x();
        double dy = residual.y() - mean.y();
        cxx += dx * dx;
        cxy += dx * dy;
        cyy += dy * dy;
    }
    
    cxx /= (residuals.size() - 1);
    cxy /= (residuals.size() - 1);
    cyy /= (residuals.size() - 1);
    
    // Build covariance matrix
    QMatrix3x3 covariance;
    covariance(0, 0) = cxx; covariance(0, 1) = cxy;
    covariance(1, 0) = cxy; covariance(1, 1) = cyy;
    
    // Calculate Mahalanobis distance for each point
    for (int i = 0; i < residuals.size(); ++i) {
        double mahalanobisDistance = calculateMahalanobisDistance(residuals[i], mean, covariance);
        isOutlier[i] = (mahalanobisDistance > threshold);
    }
    
    return isOutlier;
}

double StatisticalValidator::calculateMahalanobisDistance(
    const QPointF& point, 
    const QPointF& mean, 
    const QMatrix3x3& covariance)
{
    // Calculate (x - μ)^T * Σ^(-1) * (x - μ)
    QPointF diff = point - mean;
    
    // For 2D case, we can calculate the inverse analytically
    double det = covariance(0, 0) * covariance(1, 1) - covariance(0, 1) * covariance(1, 0);
    
    if (std::abs(det) < 1e-10) {
        // Singular covariance matrix, use Euclidean distance
        return sqrt(diff.x() * diff.x() + diff.y() * diff.y());
    }
    
    // Inverse covariance matrix
    double invCxx = covariance(1, 1) / det;
    double invCxy = -covariance(0, 1) / det;
    double invCyy = covariance(0, 0) / det;
    
    // Mahalanobis distance
    double distance = diff.x() * (invCxx * diff.x() + invCxy * diff.y()) +
                     diff.y() * (invCxy * diff.x() + invCyy * diff.y());
    
    return sqrt(std::max(0.0, distance));
}

double StatisticalValidator::calculateMatchQuality(
    const QVector<double>& distances,
    const QVector<double>& magnitudeDiffs,
    const QVector<bool>& geometricValid)
{
    if (distances.isEmpty()) return 0.0;
    
    double qualityScore = 0.0;
    int validMatches = 0;
    
    for (int i = 0; i < distances.size(); ++i) {
        if (i < geometricValid.size() && !geometricValid[i]) {
            continue; // Skip geometrically invalid matches
        }
        
        // Distance quality (closer is better)
        double distanceQuality = 1.0 / (1.0 + distances[i] / 5.0); // Normalize by 5 pixels
        
        // Magnitude quality (if available)
        double magnitudeQuality = 1.0;
        if (i < magnitudeDiffs.size()) {
            magnitudeQuality = 1.0 / (1.0 + magnitudeDiffs[i] / 2.0); // Normalize by 2 magnitudes
        }
        
        // Combined quality
        qualityScore += distanceQuality * magnitudeQuality;
        validMatches++;
    }
    
    return validMatches > 0 ? qualityScore / validMatches : 0.0;
}

} // namespace GeometricAlgorithms

#endif // GEOMETRIC_ALGORITHMS_H