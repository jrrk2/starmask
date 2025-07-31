cmake_minimum_required(VERSION 3.16)

project(XISFTestCreator VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# Set up Qt6 automoc
set(CMAKE_AUTOMOC ON)

# PCL Configuration
set(PCL_ROOT "/Users/jonathan/PCL" CACHE PATH "PCL root directory")
set(PCL_INCLUDE_DIRS 
    "${PCL_ROOT}/include" 
    "${PCL_ROOT}/src/3rdparty"
    "${PCL_ROOT}/src/modules/file-formats"
    "${PCL_ROOT}/src/modules/processes/contrib/gviehoever/GradientDomain"  # Add gradient domain
    "${PCL_ROOT}/src"
)
set(PCL_LIBRARY_DIR "${PCL_ROOT}/src/pcl/macosx/g++/arm64/Release")

# Find PCL library
find_library(PCL_LIBRARY 
    NAMES PCL-pxi libPCL-pxi.a
    PATHS ${PCL_LIBRARY_DIR}
    NO_DEFAULT_PATH
)

if(NOT PCL_LIBRARY)
    message(FATAL_ERROR "PCL library not found in ${PCL_LIBRARY_DIR}")
endif()

message(STATUS "Found PCL library: ${PCL_LIBRARY}")

# Locate Gradient Domain Sources (but don't compile them yet due to compatibility issues)
set(GRADIENT_DOMAIN_DIR "${PCL_ROOT}/src/modules/processes/contrib/gviehoever/GradientDomain")

# List all the gradient domain source files
set(GRADIENT_SOURCES
    "${GRADIENT_DOMAIN_DIR}/GradientsBase.cpp"
    "${GRADIENT_DOMAIN_DIR}/GradientsHdrCompression.cpp"
    "${GRADIENT_DOMAIN_DIR}/GradientsHdrComposition.cpp"
    "${GRADIENT_DOMAIN_DIR}/GradientsHdrCompositionInstance.cpp"
    "${GRADIENT_DOMAIN_DIR}/GradientsHdrCompositionInterface.cpp"
    "${GRADIENT_DOMAIN_DIR}/GradientsHdrCompositionParameters.cpp"
    "${GRADIENT_DOMAIN_DIR}/GradientsHdrCompositionProcess.cpp"
    "${GRADIENT_DOMAIN_DIR}/GradientsHdrInstance.cpp"
    "${GRADIENT_DOMAIN_DIR}/GradientsHdrInterface.cpp"
    "${GRADIENT_DOMAIN_DIR}/GradientsHdrParameters.cpp"
    "${GRADIENT_DOMAIN_DIR}/GradientsHdrProcess.cpp"
    "${GRADIENT_DOMAIN_DIR}/GradientsMergeMosaic.cpp"
    "${GRADIENT_DOMAIN_DIR}/GradientsMergeMosaicInstance.cpp"
    "${GRADIENT_DOMAIN_DIR}/GradientsMergeMosaicInterface.cpp"
    "${GRADIENT_DOMAIN_DIR}/GradientsMergeMosaicParameters.cpp"
    "${GRADIENT_DOMAIN_DIR}/GradientsMergeMosaicProcess.cpp"
    "${GRADIENT_DOMAIN_DIR}/GradientsModule.cpp"
    "${GRADIENT_DOMAIN_DIR}/RgbPreserve.cpp"
)

# Check that all source files exist
foreach(source_file ${GRADIENT_SOURCES})
    if(NOT EXISTS ${source_file})
        message(WARNING "Gradient source not found: ${source_file}")
    else()
        message(STATUS "Found gradient source: ${source_file}")
    endif()
endforeach()

# Create gradient library with actual sources
add_library(pcl_gradients STATIC ${GRADIENT_SOURCES})

set_target_properties(pcl_gradients PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

# Add include directories and definitions for gradient library
target_include_directories(pcl_gradients PUBLIC
    ${PCL_INCLUDE_DIRS}
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_definitions(pcl_gradients PRIVATE
    __PCL_MACOSX
    __PCL_NO_PERFORMANCE_CRITICAL_MATH_ROUTINES
)

# Compiler flags
target_compile_options(pcl_gradients PRIVATE
    -Wno-dangling-else
    -Wno-extern-c-compat
    -Wno-asm-operand-widths
    -O0 -g
)

# Create SHA library explicitly
message(STATUS "Setting up PCL SHA sources...")

set(SHA1_FILE "${PCL_ROOT}/src/3rdparty/RFC6234/sha1.c")
set(SHA224_256_FILE "${PCL_ROOT}/src/3rdparty/RFC6234/sha224-256.c") 
set(SHA384_512_FILE "${PCL_ROOT}/src/3rdparty/RFC6234/sha384-512.c")

foreach(sha_file ${SHA1_FILE} ${SHA224_256_FILE} ${SHA384_512_FILE})
    if(NOT EXISTS ${sha_file})
        message(FATAL_ERROR "SHA source not found: ${sha_file}")
    else()
        message(STATUS "Found SHA source: ${sha_file}")
    endif()
endforeach()

# Create static library for SHA functions
add_library(pcl_sha STATIC
    ${SHA1_FILE}
    ${SHA224_256_FILE}
    ${SHA384_512_FILE}
)

set_target_properties(pcl_sha PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

target_include_directories(pcl_sha PUBLIC
    "${PCL_ROOT}/src/3rdparty/RFC6234"
    "${PCL_ROOT}/src/3rdparty"
)

# Add PCL Mock Library
add_library(pcl_mock STATIC
    PCLMockAPI.cpp
    PCLThreadMock.cpp
)

set_target_properties(pcl_mock PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

target_include_directories(pcl_mock PUBLIC
    ${PCL_INCLUDE_DIRS}
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_definitions(pcl_mock PRIVATE
    __PCL_MACOSX
    __PCL_NO_PERFORMANCE_CRITICAL_MATH_ROUTINES
)

# Third-party libraries
find_library(LZ4_LIBRARY lz4 PATHS /opt/homebrew/lib)
find_library(ZSTD_LIBRARY zstd PATHS /opt/homebrew/lib)
find_library(LCMS2_LIBRARY lcms2 PATHS /opt/homebrew/lib)
find_library(CRYPTO_LIBRARY crypto PATHS /opt/homebrew/lib)
find_library(CFITSIO_LIBRARY cfitsio PATHS /opt/homebrew/lib)

# Check for required libraries
foreach(lib LZ4_LIBRARY ZSTD_LIBRARY LCMS2_LIBRARY CRYPTO_LIBRARY)
    if(NOT ${lib})
        message(FATAL_ERROR "${lib} not found. Install with: brew install lz4 zstd little-cms2 openssl")
    endif()
endforeach()

# CFITSIO is optional but recommended for FITS support
if(NOT CFITSIO_LIBRARY)
    message(WARNING "CFITSIO library not found. Install with: brew install cfitsio")
endif()

# Include directories
set(PROJECT_INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

# Define sources - Updated without gradient domain headers for now
set(SOURCES
    main.cpp
    MainWindow.cpp
    SimplifiedXISFWriter.cpp
    ImageReader.cpp
    ImageDisplayWidget.cpp
    ImageStatistics.cpp
    BackgroundExtractor.cpp
    BackgroundExtractionWidget.cpp
    ${PCL_ROOT}/src/modules/file-formats/FITS/FITS.cpp
)

set(HEADERS
    MainWindow.h
    SimplifiedXISFWriter.h
    ImageReader.h
    ImageDisplayWidget.h
    ImageStatistics.h
    PCLMockAPI.h
    PCLThreadMock.h
    BackgroundExtractor.h
    BackgroundExtractionWidget.h
)

# Create test executable (minimal)
add_executable(gradient_test gradient_test.cpp)

target_include_directories(gradient_test PRIVATE
    ${PCL_INCLUDE_DIRS}
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

# No special compile definitions needed for basic test

# No special libraries needed for basic test

# Create executable
add_executable(XISFTestCreator ${SOURCES} ${HEADERS})

# Set target properties
set_target_properties(XISFTestCreator PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER com.stellina.xisfcreator
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    WIN32_EXECUTABLE TRUE
)

# Include directories
target_include_directories(XISFTestCreator PRIVATE
    ${PCL_INCLUDE_DIRS}
    ${PROJECT_INCLUDE_DIRS}
    /opt/homebrew/include
)

# Link libraries - Add gradient library and pcl_mock BEFORE PCL library
target_link_libraries(XISFTestCreator PRIVATE
    Qt6::Core
    Qt6::Widgets
    pcl_gradients     # Add gradient library first
    pcl_mock          # Add mock library
    ${PCL_LIBRARY}
    pcl_sha
    ${LZ4_LIBRARY}
    ${ZSTD_LIBRARY} 
    ${LCMS2_LIBRARY}
    ${CRYPTO_LIBRARY}
    z
)

# Add CFITSIO if available
if(CFITSIO_LIBRARY)
    target_link_libraries(XISFTestCreator PRIVATE ${CFITSIO_LIBRARY})
endif()

# macOS-specific frameworks (required for FITS support)
if(APPLE)
    target_link_libraries(XISFTestCreator PRIVATE
        "-framework CoreFoundation"
        "-framework Foundation"
    )
endif()

# Compiler definitions
target_compile_definitions(XISFTestCreator PRIVATE
    __PCL_MACOSX
    __PCL_NO_PERFORMANCE_CRITICAL_MATH_ROUTINES
    # Remove USE_PIFFT to avoid redefinition with gradient domain
)

# Compiler flags
target_compile_options(XISFTestCreator PRIVATE
    -Wno-dangling-else
    -Wno-extern-c-compat
    -Wno-asm-operand-widths
    -O0 -g
)

# Compiler definitions
target_compile_definitions(gradient_test PRIVATE
    __PCL_MACOSX
    __PCL_NO_PERFORMANCE_CRITICAL_MATH_ROUTINES
    # Remove USE_PIFFT to avoid redefinition with gradient domain
)

target_compile_options(gradient_test PRIVATE
    -Wno-dangling-else
    -Wno-extern-c-compat
    -Wno-asm-operand-widths
    -O0 -g
)

target_link_libraries(gradient_test PRIVATE
    pcl_gradients     # Gradient domain library
    pcl_mock          # PCL mock library
    ${PCL_LIBRARY}    # Main PCL library
    pcl_sha           # SHA library
    ${LZ4_LIBRARY}    # LZ4 compression
    ${ZSTD_LIBRARY}   # ZSTD compression 
    ${LCMS2_LIBRARY}  # LCMS2 color management (this was missing!)
    ${CRYPTO_LIBRARY} # OpenSSL crypto
    z                 # zlib compression
)

# macOS-specific frameworks (required for FITS support)
if(APPLE)
    target_link_libraries(gradient_test PRIVATE
        "-framework CoreFoundation"
        "-framework Foundation"
    )
endif()

# Add compiler flags (same as main target)
target_compile_options(gradient_test PRIVATE
    -Wno-dangling-else
    -Wno-extern-c-compat
    -Wno-asm-operand-widths
    -O0 -g
)

# Debug output
message(STATUS "PCL Include Dirs: ${PCL_INCLUDE_DIRS}")
message(STATUS "PCL Library: ${PCL_LIBRARY}")
message(STATUS "Qt6 Version: ${Qt6_VERSION}")
message(STATUS "Gradient Domain Dir: ${GRADIENT_DOMAIN_DIR}")

# Install target
install(TARGETS XISFTestCreator
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

# Add custom target to run gradient test
add_custom_target(test_gradients
    COMMAND gradient_test
    DEPENDS gradient_test
    COMMENT "Running gradient domain integration test"
)
