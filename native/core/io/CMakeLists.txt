# Lattice I/O Core Module
cmake_minimum_required(VERSION 3.14)
project(lattice_io)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find ZLIB for Minecraft compression support (Anvil format requires ZLIB)
find_package(ZLIB REQUIRED)

# Source files (excluding test files)
file(GLOB_RECURSE SOURCES
    "*.cpp"
    "*.hpp"
)

# Exclude test files
set(TEST_FILES
    test_*.cpp
    anvil_format_test.cpp
)

foreach(TEST_FILE ${TEST_FILES})
    list(REMOVE_ITEM SOURCES ${TEST_FILE})
endforeach()

# Create library
add_library(lattice_io STATIC ${SOURCES})

# Platform-specific dependencies
if(UNIX AND NOT APPLE)
    # Linux io_uring support
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBURING REQUIRED liburing)
    target_include_directories(lattice_io PRIVATE ${LIBURING_INCLUDE_DIRS})
    target_link_libraries(lattice_io PRIVATE ${LIBURING_LIBRARIES})
elseif(APPLE)
    # macOS Grand Central Dispatch
    find_library(CORE_FOUNDATION_FRAMEWORK CoreFoundation)
    find_library(DISPATCH_FRAMEWORK Dispatch)
    target_link_libraries(lattice_io PRIVATE ${CORE_FOUNDATION_FRAMEWORK} ${DISPATCH_FRAMEWORK})
elseif(WIN32)
    # Windows IOCP (built-in)
    target_link_libraries(lattice_io PRIVATE ws2_32)
endif()

# Link with existing Lattice modules and ZLIB for Anvil compression
target_link_libraries(lattice_io PRIVATE lattice_core ZLIB::ZLIB)

# Compile definitions for platform detection
if(APPLE)
    target_compile_definitions(lattice_io PRIVATE APPLE_PLATFORM)
elseif(UNIX AND NOT APPLE)
    target_compile_definitions(lattice_io PRIVATE LINUX_PLATFORM)
elseif(WIN32)
    target_compile_definitions(lattice_io PRIVATE WINDOWS_PLATFORM)
endif()