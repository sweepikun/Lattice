cmake_minimum_required(VERSION 3.20)
project(lattice_redstone_engine_test)

# 设置C++标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 启用C++20特性
set(CMAKE_CXX_EXTENSIONS OFF)

# 设置编译器标志
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG_REDSTONE")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# 查找标准库
find_package(Threads REQUIRED)

# 源文件
set(REDSTONE_SOURCES
    redstone_engine.cpp
    redstone_components.hpp
    redstone_engine.hpp
)

# 测试可执行文件
add_executable(test_redstone_engine test_redstone_engine.cpp ${REDSTONE_SOURCES})

# 链接线程库
target_link_libraries(test_redstone_engine PRIVATE Threads::Threads)

# 设置包含目录
target_include_directories(test_redstone_engine PRIVATE .)

# 输出目录
set_target_properties(test_redstone_engine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 消息输出
message(STATUS "红石引擎测试构建完成")
message(STATUS "C++标准: ${CMAKE_CXX_STANDARD}")
message(STATUS "调试模式: ${CMAKE_BUILD_TYPE}")
message(STATUS "可执行文件输出: ${CMAKE_BINARY_DIR}/bin/test_redstone_engine")