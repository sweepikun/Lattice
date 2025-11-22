# Lattice I/O JNI Module
cmake_minimum_required(VERSION 3.14)
project(lattice_io_jni)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JNI include directories
find_package(JNI REQUIRED)
include_directories(${JNI_INCLUDE_DIRS})

# Source files
file(GLOB_RECURSE SOURCES
    "*.cpp"
    "*.hpp"
)

# Create JNI library
add_library(lattice_io_jni SHARED ${SOURCES})

# Link with I/O core and other Lattice modules
target_link_libraries(lattice_io_jni 
    PRIVATE 
    lattice_io
    lattice_core
    ${JNI_LIBRARIES}
)

# Platform-specific libraries
if(UNIX AND NOT APPLE)
    target_link_libraries(lattice_io_jni PRIVATE liburing)
elseif(APPLE)
    target_link_libraries(lattice_io_jni PRIVATE "-framework CoreFoundation" "-framework Dispatch")
elseif(WIN32)
    target_link_libraries(lattice_io_jni PRIVATE ws2_32)
endif()