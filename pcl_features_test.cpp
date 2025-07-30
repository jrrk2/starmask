// Simple test to check what PCL features are available
// You can compile this separately to test your PCL installation

#include <iostream>

// Test what PCL headers are available
#ifdef __has_include
    #if __has_include(<pcl/XISF.h>)
        #include <pcl/XISF.h>
        #define HAS_PCL_XISF 1
    #endif
    
    #if __has_include(<pcl/FITS.h>)
        #include <pcl/FITS.h>
        #define HAS_PCL_FITS 1
    #endif
    
    #if __has_include(<pcl/Image.h>)
        #include <pcl/Image.h>
        #define HAS_PCL_IMAGE 1
    #endif
#else
    // Fallback for older compilers
    #include <pcl/XISF.h>
    #include <pcl/Image.h>
    #define HAS_PCL_XISF 1
    #define HAS_PCL_IMAGE 1
    // Don't include FITS by default
#endif

int main()
{
    std::cout << "PCL Feature Detection:\n";
    
#ifdef HAS_PCL_IMAGE
    std::cout << "✓ PCL Image support available\n";
#else
    std::cout << "✗ PCL Image support NOT available\n";
#endif

#ifdef HAS_PCL_XISF
    std::cout << "✓ PCL XISF support available\n";
#else
    std::cout << "✗ PCL XISF support NOT available\n";
#endif

#ifdef HAS_PCL_FITS
    std::cout << "✓ PCL FITS support available\n";
#else
    std::cout << "✗ PCL FITS support NOT available\n";
#endif

    return 0;
}