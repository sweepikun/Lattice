add_library(lattice_core STATIC
    threadpool.hpp
    task_scheduler.cpp
    task_scheduler.hpp
    worldgen/terrain_generator.cpp
    worldgen/terrain_generator.hpp
)

target_include_directories(lattice_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Add spdlog for logging
find_package(spdlog REQUIRED)
target_link_libraries(lattice_core PUBLIC spdlog::spdlog)
