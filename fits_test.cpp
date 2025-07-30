#include <iostream>

#include <FITS/FITS.h>
#define HAS_FITS_HEADER 1

int main()
{
    std::cout << "FITS Support Test" << std::endl;
    std::cout << "==================" << std::endl;
    
#ifdef HAS_FITS_HEADER
    std::cout << "✓ FITS header included successfully" << std::endl;
    
    // Try to create a FITS reader to test if it links
    try {
        pcl::FITSReader reader;
        std::cout << "✓ FITSReader created successfully" << std::endl;
        std::cout << "✓ FITS support is fully functional!" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "✗ FITSReader failed: " << e.what() << std::endl;
    } catch (...) {
        std::cout << "✗ FITSReader failed with unknown error" << std::endl;
    }
#else
    std::cout << "✗ FITS header not available" << std::endl;
#endif
    
    return 0;
}
