add_library(lattice_native SHARED
    native_interface.cpp
)

target_link_libraries(lattice_native
    PRIVATE
    lattice_core
    ${JNI_LIBRARIES}
)

target_include_directories(lattice_native
    PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/native
    ${JNI_INCLUDE_DIRS}
)
