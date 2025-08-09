cmake_minimum_required(VERSION 3.16)
project(origin_tiff_solver)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable Qt MOC processing
set(CMAKE_AUTOMOC ON)

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(TIFF REQUIRED)

# Try to find StellarSolver
find_package(StellarSolver QUIET)
if(NOT StellarSolver_FOUND)
    # Fallback: try to find it via pkg-config
    pkg_check_modules(STELLARSOLVER stellarsolver)
    if(NOT STELLARSOLVER_FOUND)
        message(WARNING "StellarSolver not found via CMake or pkg-config")
        message(STATUS "Attempting to find StellarSolver manually...")
        
        # Manual search in common locations
        find_path(STELLARSOLVER_INCLUDE_DIR 
            NAMES stellarsolver.h
            PATHS 
                /usr/local/include
                /usr/local/include/libstellarsolver
                /opt/homebrew/include
                /usr/include
        )
        
        find_library(STELLARSOLVER_LIBRARY
            NAMES stellarsolver
            PATHS
                /usr/local/lib/
                /opt/homebrew/lib
                /usr/lib
        )
        
        if(STELLARSOLVER_INCLUDE_DIR AND STELLARSOLVER_LIBRARY)
            set(STELLARSOLVER_FOUND TRUE)
            set(STELLARSOLVER_LIBRARIES ${STELLARSOLVER_LIBRARY})
            set(STELLARSOLVER_INCLUDE_DIRS ${STELLARSOLVER_INCLUDE_DIR})
            message(STATUS "Found StellarSolver manually: ${STELLARSOLVER_LIBRARY}")
        else()
            message(FATAL_ERROR "StellarSolver not found! Please install StellarSolver or set CMAKE_PREFIX_PATH")
        endif()
    endif()
endif()

# Origin TIFF solver sources
set(ORIGIN_TIFF_SOURCES
    OriginMetadataExtractor.h
    OriginMetadataExtractor.cpp
    OriginStellarSolverInterface.h
    OriginStellarSolverInterface.cpp
    OriginTIFFSolver.h
    main.cpp
)

# Create executable FIRST
add_executable(origin_tiff_solver ${ORIGIN_TIFF_SOURCES})

# Set target properties
set_target_properties(origin_tiff_solver PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    AUTOMOC ON
    INSTALL_RPATH "/usr/local/lib;/opt/homebrew/lib"
    BUILD_RPATH "/usr/local/lib;/opt/homebrew/lib"
    MACOSX_RPATH ON
)

# Link libraries
target_link_libraries(origin_tiff_solver
    Qt6::Core
    TIFF::TIFF
    -lstellarsolver6
)

# Handle StellarSolver linking based on how it was found
if(TARGET StellarSolver::StellarSolver)
    # Modern CMake target
    target_link_libraries(origin_tiff_solver StellarSolver::StellarSolver)
    message(STATUS "Linking with StellarSolver CMake target")
elseif(STELLARSOLVER_FOUND)
    # pkg-config or manual find
    target_link_libraries(origin_tiff_solver ${STELLARSOLVER_LIBRARIES})
    target_include_directories(origin_tiff_solver PRIVATE ${STELLARSOLVER_INCLUDE_DIRS})
    message(STATUS "Linking with StellarSolver libraries: ${STELLARSOLVER_LIBRARIES}")
endif()

# Include directories
target_include_directories(origin_tiff_solver PRIVATE
    /usr/local/include/libstellarsolver
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${TIFF_INCLUDE_DIRS}
)

# Compiler definitions for Qt
target_compile_definitions(origin_tiff_solver PRIVATE
    QT_NO_DEBUG_OUTPUT  # Remove in debug builds if needed
)

target_compile_options(origin_tiff_solver PRIVATE
    -Wno-dangling-else
    -Wno-extern-c-compat
    -Wno-asm-operand-widths
    -O3 -g
)

# Platform-specific settings
if(APPLE)
    # macOS specific settings
    find_library(COREFOUNDATION_LIBRARY CoreFoundation)
    target_link_libraries(origin_tiff_solver ${COREFOUNDATION_LIBRARY})
    
    # Handle macOS frameworks if needed
    target_compile_definitions(origin_tiff_solver PRIVATE
        MACOS_BUILD
    )
    
    message(STATUS "Configured for macOS build")
endif()

# Debug information
message(STATUS "=== Build Configuration ===")
message(STATUS "Qt6 version: ${Qt6_VERSION}")
message(STATUS "TIFF library: ${TIFF_LIBRARIES}")
message(STATUS "TIFF include: ${TIFF_INCLUDE_DIRS}")

if(STELLARSOLVER_FOUND)
    message(STATUS "StellarSolver libraries: ${STELLARSOLVER_LIBRARIES}")
    message(STATUS "StellarSolver includes: ${STELLARSOLVER_INCLUDE_DIRS}")
else()
    message(STATUS "StellarSolver: Using CMake target")
endif()

message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Optional: Add install target
install(TARGETS origin_tiff_solver
    RUNTIME DESTINATION bin
)

# Optional: Create a simple test
enable_testing()
add_test(NAME version_test 
         COMMAND origin_tiff_solver --help)

# Print final status
message(STATUS "=== Configuration Complete ===")
message(STATUS "Run 'make' to build origin_tiff_solver")
