#include "StarMaskGenerator.h"
#include "PCLMockAPI.h"

#include <pcl/Image.h>
#include <pcl/api/APIInterface.h>

// Simple star detection algorithm since PCL StarDetector may not be available
#include <algorithm>
#include <cmath>
#include <QDebug>

StarMaskResult StarMaskGenerator::detectStars(const ImageData& imageData, float threshold)
{
    StarMaskResult result;

    if (!imageData.isValid()) {
        qDebug() << "Invalid image data for star detection";
        return result;
    }

    try {
        // Initialize PCL Mock API
        pcl_mock::InitializeMockAPI();
        if (!API) {
            API = new pcl::APIInterface(pcl_mock::GetMockFunctionResolver());
        }

        // Use first channel only for star detection
        int width = imageData.width;
        int height = imageData.height;
        const float* pixels = imageData.pixels.constData();

        // Simple star detection algorithm
        // Find local maxima above threshold
        std::vector<float> localMaxima;
        std::vector<QPoint> candidates;

        // Calculate image statistics for adaptive threshold
        float sum = 0.0f;
        float sumSq = 0.0f;
        int count = 0;

        for (int i = 0; i < width * height; ++i) {
            float val = pixels[i];
            sum += val;
            sumSq += val * val;
            count++;
        }

        float mean = sum / count;
        float variance = (sumSq / count) - (mean * mean);
        float stddev = std::sqrt(variance);
        
        // Adaptive threshold based on image statistics
        float detectionThreshold = mean + threshold * stddev * 5.0f;
        
        qDebug() << "Star detection: mean=" << mean << "stddev=" << stddev << "threshold=" << detectionThreshold;

        // Find local maxima
        int radius = 3; // Search radius for local maxima
        for (int y = radius; y < height - radius; y += 2) { // Skip pixels for speed
            for (int x = radius; x < width - radius; x += 2) {
                int centerIdx = y * width + x;
                float centerVal = pixels[centerIdx];
                
                if (centerVal < detectionThreshold) continue;
                
                // Check if this is a local maximum
                bool isLocalMax = true;
                for (int dy = -radius; dy <= radius && isLocalMax; ++dy) {
                    for (int dx = -radius; dx <= radius && isLocalMax; ++dx) {
                        if (dx == 0 && dy == 0) continue;
                        
                        int checkIdx = (y + dy) * width + (x + dx);
                        if (pixels[checkIdx] >= centerVal) {
                            isLocalMax = false;
                        }
                    }
                }
                
                if (isLocalMax) {
                    candidates.push_back(QPoint(x, y));
                    localMaxima.push_back(centerVal);
                }
            }
        }

        qDebug() << "Found" << candidates.size() << "star candidates";

        // Sort by brightness and take the brightest ones
        std::vector<int> indices(candidates.size());
        std::iota(indices.begin(), indices.end(), 0);
        
        std::sort(indices.begin(), indices.end(), [&](int a, int b) {
            return localMaxima[a] > localMaxima[b];
        });

        // Limit to reasonable number of stars
        int maxStars = std::min(500, static_cast<int>(candidates.size()));
        
        // Create mask image
        result.maskImage = QImage(width, height, QImage::Format_Grayscale8);
        result.maskImage.fill(0);

        // Process the brightest candidates
        for (int i = 0; i < maxStars; ++i) {
            int idx = indices[i];
            QPoint center = candidates[idx];
            float brightness = localMaxima[idx];
            
            // Estimate star radius based on brightness
            float starRadius = 2.0f + (brightness - mean) / stddev * 0.5f;
            starRadius = std::max(1.0f, std::min(starRadius, 8.0f));
            
            result.starCenters.append(center);
            result.starRadii.append(starRadius);
            result.starValid.append(true);

            // Draw star in mask
            int r = static_cast<int>(std::ceil(starRadius));
            for (int dy = -r; dy <= r; ++dy) {
                for (int dx = -r; dx <= r; ++dx) {
                    int px = center.x() + dx;
                    int py = center.y() + dy;
                    if (px >= 0 && py >= 0 && px < width && py < height) {
                        if (dx * dx + dy * dy <= r * r) {
                            result.maskImage.setPixel(px, py, 255);
                        }
                    }
                }
            }
        }

        qDebug() << "Star detection completed:" << result.starCenters.size() << "stars detected";

    } catch (const std::exception& e) {
        qDebug() << "Error in star detection:" << e.what();
    } catch (...) {
        qDebug() << "Unknown error in star detection";
    }

    return result;
}