#include "PCLMockAPI.h"
#include "AstrometryDirectSolver.h"

int astrometry_direct(std::vector<StarPosition> stars)
{
  DirectSolveOptions options;
  options.verbose = true;
    
    // Sort by flux (brightest first) and limit
    std::sort(stars.begin(), stars.end(),
             [](const StarPosition& a, const StarPosition& b) {
                 return a.flux > b.flux;
             });
    
    // Create and initialize engine
    AstrometryDirectSolver solver;
    if (!solver.initialize(options)) {
        std::cerr << "Failed to initialize astrometry engine" << std::endl;
        return 1;
    }
    
    // Solve
    auto result = solver.solve(stars, options);
    
    // Output results
    if (result.solved) {
        std::cout << "SOLUTION FOUND:" << std::endl;
        std::cout << "RA Center: " << result.wcs.crval[0] << " degrees" << std::endl;
        std::cout << "Dec Center: " << result.wcs.crval[1] << " degrees" << std::endl;
        // Calculate pixel scale from CD matrix
        double cd00 = result.wcs.cd[0][0];
        double cd01 = result.wcs.cd[0][1];
        double pixelScale = sqrt(cd00 * cd00 + cd01 * cd01) * 3600.0;
        std::cout << "Pixel Scale: " << pixelScale << " arcsec/pixel" << std::endl;
        // Calculate rotation from CD matrix
        double rotation = atan2(cd01, cd00) * 180.0 / M_PI;
        std::cout << "Rotation: " << rotation << " degrees" << std::endl;
        std::cout << "Reference Pixel: (" << result.wcs.crpix[0] << ", " << result.wcs.crpix[1] << ")" << std::endl;
        std::cout << "Image Size: " << result.wcs.imagew << "x" << result.wcs.imageh << std::endl;
        return 0;
    } else {
        std::cout << "NO SOLUTION FOUND" << std::endl;
        std::cout << "Message: " << result.message << std::endl;
        return 1;
    }
    return 0;
}
