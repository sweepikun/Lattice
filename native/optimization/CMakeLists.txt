# CMakeLists.txt for Lattice Optimization Module
# Unified optimization framework for all Lattice native components

cmake_minimum_required(VERSION 3.16)
project(LatticeOptimization)

# C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimization flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# SIMD compilation flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -mtune=native")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx -mavx2 -mavx512f")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse2")
endif()

# Thread support
find_package(Threads REQUIRED)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

# Find Java
find_package(Java COMPONENTS Development)
if(NOT Java_FOUND)
    message(FATAL_ERROR "Java Development not found. Please install JDK.")
endif()

# Java includes
include_directories(${Java_INCLUDE_DIRS})

# Source files for optimization module
set(OPTIMIZATION_SOURCES
    optimization/lattice_optimization.hpp
)

# JNI optimized modules sources
set(JNI_OPTIMIZED_SOURCES
    jni/io/async_chunk_io_optimized_jni.cpp
    jni/io/nbt_serializer_optimized_jni.cpp
    jni/net/memory_arena_optimized_jni.cpp
    jni/net/async_compressor_optimized_jni.cpp
    jni/net/entity_tracker_optimized_jni.cpp
    jni/world/light_engine_optimized_jni.cpp
    jni/world/pathfinder_optimized_jni.cpp
    jni/entity/biological_ai_optimized_jni.cpp
)

# Header files
set(JNI_OPTIMIZED_HEADERS
    jni/io/async_chunk_io_optimized_jni.hpp
    jni/io/nbt_serializer_optimized_jni.hpp
    jni/net/memory_arena_optimized_jni.hpp
    jni/net/async_compressor_optimized_jni.hpp
    jni/net/entity_tracker_optimized_jni.hpp
    jni/world/light_engine_optimized_jni.hpp
    jni/world/pathfinder_optimized_jni.hpp
    jni/entity/biological_ai_optimized_jni.hpp
    jni/safe_memory_manager.hpp
)

# Create optimization library
add_library(lattice_optimization STATIC ${OPTIMIZATION_SOURCES} ${JNI_OPTIMIZED_SOURCES} ${JNI_OPTIMIZED_HEADERS})
target_link_libraries(lattice_optimization Threads::Threads)

# Create JNI optimized modules library
add_library(lattice_optimization_jni SHARED ${JNI_OPTIMIZED_SOURCES} ${JNI_OPTIMIZED_HEADERS})
target_link_libraries(lattice_optimization_jni lattice_optimization Threads::Threads)
target_include_directories(lattice_optimization_jni PRIVATE ${Java_INCLUDE_DIRS})

# Set library properties
set_target_properties(lattice_optimization PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    OUTPUT_NAME "lattice_optimization"
)

# Install target
install(TARGETS lattice_optimization
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES optimization/lattice_optimization.hpp
    DESTINATION include/lattice/optimization
)

# Testing
if(BUILD_TESTING)
    enable_testing()
    
    # Create a simple test executable
    add_executable(test_optimization test_optimization.cpp)
    target_link_libraries(test_optimization lattice_optimization)
    target_include_directories(test_optimization PRIVATE ${CMAKE_SOURCE_DIR})
    
    # Add test
    add_test(NAME optimization_basic_test COMMAND test_optimization)
endif()

# Print configuration summary
message(STATUS "Lattice Optimization Module Configuration:")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Optimization Level: ${CMAKE_BUILD_TYPE}")
message(STATUS "  SIMD Support: AVX512, AVX2, AVX, SSE2")
message(STATUS "  Thread Support: ${Threads_FOUND}")
message(STATUS "  Java Development: ${Java_FOUND}")