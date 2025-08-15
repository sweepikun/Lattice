cmake_minimum_required(VERSION 3.15)
project(lattice_native VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Find JNI
find_package(JNI REQUIRED)
include_directories(${JNI_INCLUDE_DIRS})

# Add core library
add_subdirectory(native/core)

# Add JNI library
add_subdirectory(native/jni)

# Add tests
enable_testing()
add_subdirectory(native/tests)
