cmake_minimum_required(VERSION 3.20)

cmake_policy(SET CMP0135 NEW)

project(SpectreTilesCompression
    VERSION 1.0.0
    DESCRIPTION "Einstein Tiles Compression Algorithm"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

find_package(OpenMP QUIET)

if(NOT OpenMP_FOUND AND MINGW)
    message(STATUS "find_package(OpenMP) failed, checking MSYS2 locations...")
    
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(MSYS2_PREFIX "C:/msys64/mingw64" "D:/msys64/mingw64" "E:/msys64/mingw64" "D:/cpp")
    else()
        set(MSYS2_PREFIX "C:/msys64/mingw32" "D:/msys64/mingw32" "E:/msys64/mingw32" "D:/cpp")
    endif()
    
    find_library(LIBOMP_LIBRARY 
        NAMES omp libomp
        PATHS ${MSYS2_PREFIX}
        PATH_SUFFIXES lib
        NO_DEFAULT_PATH
    )
    
    find_path(LIBOMP_INCLUDE_DIR
        NAMES omp.h
        PATHS ${MSYS2_PREFIX}
        PATH_SUFFIXES include
        NO_DEFAULT_PATH
    )
    
    if(LIBOMP_LIBRARY AND LIBOMP_INCLUDE_DIR)
        message(STATUS "✓ Found LLVM OpenMP in MSYS2: ${LIBOMP_LIBRARY}")
        add_library(OpenMP::OpenMP_CXX UNKNOWN IMPORTED)
        set_target_properties(OpenMP::OpenMP_CXX PROPERTIES
            IMPORTED_LOCATION "${LIBOMP_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${LIBOMP_INCLUDE_DIR}"
            INTERFACE_COMPILE_OPTIONS -fopenmp
        )
        set(OpenMP_FOUND TRUE)
    endif()
endif()

if(NOT OpenMP_FOUND)
    message(STATUS "")
    message(STATUS "OpenMP not found - multithreading will be disabled")
    message(STATUS "")
    message(STATUS "To enable multithreading for better performance, install OpenMP:")
    message(STATUS "  - Windows (MSYS2):  pacman -S mingw-w64-x86_64-llvm-openmp OR pacman -S mingw-w64-ucrt-x86_64-llvm-openmp if ucrt")
    message(STATUS "  - Windows (Visual Studio): Already included in Visual Studio")
    message(STATUS "  - macOS:            brew install libomp")
    message(STATUS "  - Linux (Ubuntu):   apt install libomp-dev")
    message(STATUS "  - Linux (Fedora):   dnf install libomp-devel")
    message(STATUS "")
    message(STATUS "Compilation will continue in single-threaded mode...")
    message(STATUS "")
else()
    message(STATUS "OpenMP found - multithreading ENABLED")
    message(STATUS "Use --threads N option in CLI to control thread count")
endif()


FetchContent_Declare(
    zlib
    URL https://github.com/madler/zlib/archive/refs/tags/v1.3.1.tar.gz
    URL_HASH SHA256=17e88863f3600672ab49182f217281b6fc4d3c762bde361935e436a95214d05c
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(zlib)

set(ZLIB_INCLUDE_DIR ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
set(ZLIB_LIBRARY zlibstatic)
set(ZLIB_FOUND TRUE)

if(NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB ALIAS zlibstatic)
endif()

FetchContent_Declare(
    libpng
    URL https://github.com/glennrp/libpng/archive/refs/tags/v1.6.43.tar.gz
    URL_HASH SHA256=fecc95b46cf05e8e3fc8a414750e0ba5aad00d89e9fdf175e94ff041caf1a03a
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

set(PNG_SHARED OFF CACHE BOOL "" FORCE)
set(PNG_STATIC ON CACHE BOOL "" FORCE)
set(PNG_TESTS OFF CACHE BOOL "" FORCE)
set(PNG_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(libpng)

function(set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
    endif()
endfunction()

set(SPECTRE_SOURCES
    src/spectre_tile.cpp
    src/spectre_tree.cpp
    src/color_data.cpp
    src/variance_calculator.cpp
    src/tile_inflater.cpp
    src/hierarchical_address.cpp
    src/compressor.cpp
    src/decompressor.cpp
    src/spectrum_analyzer.cpp
    src/image_io.cpp
    src/etca_format.cpp
    src/entropy_coding.cpp
)

add_library(libetca STATIC ${SPECTRE_SOURCES})
target_include_directories(libetca PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(libetca PRIVATE ${libpng_SOURCE_DIR} ${libpng_BINARY_DIR})

target_link_libraries(libetca PUBLIC ZLIB::ZLIB png_static)

if(OpenMP_FOUND)
    target_link_libraries(libetca PUBLIC OpenMP::OpenMP_CXX)
    target_compile_definitions(libetca PUBLIC ETCA_OPENMP=1)
    message(STATUS "OpenMP enabled for libetca")
else()
    target_compile_definitions(libetca PUBLIC ETCA_OPENMP=0)
    message(STATUS "OpenMP NOT available - compiling in single-threaded mode")
endif()

set_warnings(libetca)
set_property(TARGET libetca PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

add_executable(spectre_compression src/main.cpp)
target_link_libraries(spectre_compression PRIVATE libetca ZLIB::ZLIB png_static)
if(OpenMP_FOUND)
    target_link_libraries(spectre_compression PRIVATE OpenMP::OpenMP_CXX)
    target_compile_definitions(spectre_compression PRIVATE ETCA_OPENMP=1)
else()
    target_compile_definitions(spectre_compression PRIVATE ETCA_OPENMP=0)
endif()
target_include_directories(spectre_compression PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_warnings(spectre_compression)
set_property(TARGET spectre_compression PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

add_executable(etca src/etca_cli.cpp)
target_link_libraries(etca PRIVATE libetca ZLIB::ZLIB png_static)
if(OpenMP_FOUND)
    target_link_libraries(etca PRIVATE OpenMP::OpenMP_CXX)
    target_compile_definitions(etca PRIVATE ETCA_OPENMP=1)
else()
    target_compile_definitions(etca PRIVATE ETCA_OPENMP=0)
endif()
target_include_directories(etca PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_warnings(etca)
set_property(TARGET etca PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
