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
set(PCL_INCLUDE_DIRS "${PCL_ROOT}/include" "${PCL_ROOT}/src/3rdparty")
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

# Third-party libraries
find_library(LZ4_LIBRARY lz4 PATHS /opt/homebrew/lib)
find_library(ZSTD_LIBRARY zstd PATHS /opt/homebrew/lib)
find_library(LCMS2_LIBRARY lcms2 PATHS /opt/homebrew/lib)
find_library(CRYPTO_LIBRARY crypto PATHS /opt/homebrew/lib)

# Check for required libraries
foreach(lib LZ4_LIBRARY ZSTD_LIBRARY LCMS2_LIBRARY CRYPTO_LIBRARY)
    if(NOT ${lib})
        message(FATAL_ERROR "${lib} not found. Install with: brew install lz4 zstd little-cms2 openssl")
    endif()
endforeach()

# Include directories
set(PROJECT_INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

# Define sources
set(SOURCES
    main.cpp
    MainWindow.cpp
    SimplifiedXISFWriter.cpp
)

set(HEADERS
    MainWindow.h
    SimplifiedXISFWriter.h
)

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

# Link libraries
target_link_libraries(XISFTestCreator PRIVATE
    Qt6::Core
    Qt6::Widgets
    ${PCL_LIBRARY}
    pcl_sha
    ${LZ4_LIBRARY}
    ${ZSTD_LIBRARY} 
    ${LCMS2_LIBRARY}
    ${CRYPTO_LIBRARY}
    z
)

# macOS-specific frameworks
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
)

# Compiler flags
target_compile_options(XISFTestCreator PRIVATE
    -Wno-dangling-else
    -Wno-extern-c-compat
)

# Debug output
message(STATUS "PCL Include Dirs: ${PCL_INCLUDE_DIRS}")
message(STATUS "PCL Library: ${PCL_LIBRARY}")
message(STATUS "Qt6 Version: ${Qt6_VERSION}")

# Install target
install(TARGETS XISFTestCreator
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)