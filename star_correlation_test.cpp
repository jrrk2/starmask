#include "StarCorrelator.h"

int main() {
    StarCorrelator correlator;
    
    std::cout << "Enhanced Star Correlation Analysis\n";
    std::cout << "==================================\n";

    
    // Load from your log file
    std::cout << "Loading data from log file...\n";
    #include "stars.h"
    //    correlator.loadDetectedStarsFromLog("output.txt");
    //    correlator.loadCatalogStarsFromLog("output.txt");
    
    // Set parameters
    correlator.setImageDimensions(3056, 2048);
    correlator.setMatchThreshold(5.0);
    correlator.setZeroPoint(25.0); // You may need to adjust this
    
    // Perform correlation
    correlator.correlateStars();
    
    // Print all analyses
    correlator.printStatistics();
    correlator.printMatches();
    correlator.printUnmatchedDetected(20);
    correlator.printUnmatchedCatalog(10);
    correlator.analyzeBrightStars();
    correlator.analyzeFluxMagnitudeCorrelation();
    correlator.testDifferentThresholds();
    
    // Export results
    correlator.exportMatches("star_matches.csv");
    
    return 0;
}
