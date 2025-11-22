cmake_minimum_required(VERSION 3.18)
project(LatticeChunkIO LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找libdeflate（比zlib更快的压缩库）
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBDEFLATE libdeflate)
endif()

if(NOT LIBDEFLATE_FOUND)
    # 手动查找libdeflate
    find_library(LIBDEFLATE_LIBRARY deflate)
    find_path(LIBDEFLATE_INCLUDE_DIR libdeflate.h)
    
    if(LIBDEFLATE_LIBRARY AND LIBDEFLATE_INCLUDE_DIR)
        set(LIBDEFLATE_FOUND TRUE)
        set(LIBDEFLATE_LIBRARIES ${LIBDEFLATE_LIBRARY})
        set(LIBDEFLATE_INCLUDE_DIRS ${LIBDEFLATE_INCLUDE_DIR})
    endif()
endif()

if(NOT LIBDEFLATE_FOUND)
    message(FATAL_ERROR "libdeflate library not found. Please install libdeflate-dev")
endif()

# 查找Java 21 JNI
set(JAVA_HOME $ENV{JAVA_HOME})
if(NOT JAVA_HOME)
    # 尝试常见Java安装路径
    set(JAVA_HOME /workspace/jdk-21.0.2)
endif()

if(EXISTS ${JAVA_HOME}/include/jni.h)
    set(JAVA_INCLUDE_PATH ${JAVA_HOME}/include)
    set(JAVA_INCLUDE_PATH2 ${JAVA_HOME}/include/linux)
    set(JAVA_LIBRARIES ${JAVA_HOME}/lib/server/libjvm.so)
else()
    message(FATAL_ERROR "Java 21 development headers not found in ${JAVA_HOME}")
endif()

# 设置JNI包含路径
include_directories(${JAVA_INCLUDE_PATH})
include_directories(${JAVA_INCLUDE_PATH2})

# 创建统一的JNI桥接库
add_library(lattice_chunk_io SHARED
    jni/ChunkIOBridge.cpp
    jni/ChunkIOBridge.h
    core/io/anvil_format.cpp
    core/io/anvil_format.hpp
    core/io/io_types.hpp
    core/io/async_chunk_io.cpp
    core/io/async_chunk_io.hpp
    core/net/native_compressor.hpp
    core/net/memory_arena.hpp
)

# 链接libdeflate库
target_link_libraries(lattice_chunk_io ${LIBDEFLATE_LIBRARIES})

# 链接Java JNI库
target_link_libraries(lattice_chunk_io ${JAVA_LIBRARIES})

# 设置包含目录
target_include_directories(lattice_chunk_io PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LIBDEFLATE_INCLUDE_DIRS}
)

# 添加编译器标志
target_compile_options(lattice_chunk_io PRIVATE 
    -Wall -Wextra -O3
    -std=c++20
    -fPIC
    -fexceptions
    -pthread
)

# 支持Android NDK
if(ANDROID)
    target_compile_options(lattice_chunk_io PRIVATE 
        -DANDROID
        -D__ANDROID__
        -fvisibility=hidden
    )
endif()

# 平台特定优化
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lattice_chunk_io PRIVATE 
        -march=native
        -mtune=native
        -flto
    )
endif()

# 创建测试可执行文件
add_executable(test_chunk_io 
    test_chunk_io.cpp
)

# 链接测试程序到桥接库
target_link_libraries(test_chunk_io lattice_chunk_io)

# 设置测试程序的包含目录
target_include_directories(test_chunk_io PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LIBDEFLATE_INCLUDE_DIRS}
)

# 编译测试程序
target_compile_options(test_chunk_io PRIVATE 
    -Wall -Wextra -O2
    -std=c++20
    -pthread
    -fexceptions
)

# 打印配置信息
message(STATUS "=== Lattice ChunkIO Configuration ===")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "libdeflate Found: ${LIBDEFLATE_FOUND}")
message(STATUS "libdeflate Include: ${LIBDEFLATE_INCLUDE_DIRS}")
message(STATUS "libdeflate Library: ${LIBDEFLATE_LIBRARIES}")
message(STATUS "Java 21 Home: ${JAVA_HOME}")
message(STATUS "Java Include Path: ${JAVA_INCLUDE_PATH}")
message(STATUS "Java Include Path2: ${JAVA_INCLUDE_PATH2}")
message(STATUS "Java Libraries: ${JAVA_LIBRARIES}")
message(STATUS "=== Configuration Complete ===")

# 安装规则
install(TARGETS lattice_chunk_io
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION include
)

# 创建Android APK构建支持
if(ANDROID)
    add_library(chunk_io ALIAS lattice_chunk_io)
endif()