add_library(lattice_native SHARED
    native_interface.cpp
    redstone/paper_compatible_redstone_jni.cpp
    redstone/paper_compatible_redstone_jni.hpp
    cache/hierarchical_cache_jni.cpp
    world/async_chunk_io_jni.cpp
    ai/adaptive_decision_engine_jni.cpp
    jni_helper.hpp
    safe_memory_manager.hpp
)

target_link_libraries(lattice_native
    PRIVATE
    lattice_core
    lattice_cache_io
    ${JNI_LIBRARIES}
    sqlite3
    ${CMAKE_DL_LIBS}
)

target_include_directories(lattice_native
    PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/native
    ${CMAKE_SOURCE_DIR}/native/jni
    ${JNI_INCLUDE_DIRS}
)

# Platform-specific libraries
if(LINUX)
    find_package(uring QUIET)
    if(uring_FOUND)
        target_link_libraries(lattice_native PRIVATE uring)
    endif()
    target_link_libraries(lattice_native PRIVATE pthread)
elseif(WIN32)
    target_link_libraries(lattice_native PRIVATE ws2_32 kernel32)
elseif(APPLE)
    target_link_libraries(lattice_native PRIVATE "-framework Foundation" "-framework Security")
endif()

# SIMD optimization flags
if(MSVC)
    target_compile_options(lattice_native PRIVATE /arch:AVX2)
else()
    target_compile_options(lattice_native PRIVATE -mavx2 -mfma)
endif()

# Enable modern C++ features
target_compile_features(lattice_native PRIVATE cxx_std_20)

# Additional compile definitions
target_compile_definitions(lattice_native
    PRIVATE
    JNIEXPORT=__attribute__((visibility("default")))
    JNIIMPORT=__attribute__((visibility("default")))
)
