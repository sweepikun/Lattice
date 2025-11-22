include(FetchContent)

# Download and build spdlog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.12.0
)
FetchContent_MakeAvailable(spdlog)

add_library(lattice_core STATIC
    threadpool.hpp
    task_scheduler.cpp
    task_scheduler.hpp
    worldgen/terrain_generator.cpp
    worldgen/terrain_generator.hpp
    redstone/paper_compatible_redstone_engine.cpp
    redstone/paper_compatible_redstone_engine.hpp
)

target_include_directories(lattice_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(lattice_core PUBLIC spdlog::spdlog)
