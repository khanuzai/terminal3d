cmake_minimum_required(VERSION 3.16)
project(term3d LANGUAGES CXX)

# Require plain C++17 (no compiler-specific extensions like gnu++17).
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to an optimized build; a software renderer is slow without -O2.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Turn on the useful warnings so the compiler can catch mistakes for us.
set(TERM3D_WARNINGS -Wall -Wextra -Wpedantic)

# Everything except main() is built once as a library. The viewer and the
# unit tests both link against it, so the tests check the exact same code.
add_library(term3d_core STATIC
    src/obj_loader.cpp
    src/mesh.cpp
    src/renderer.cpp
    src/framebuffer.cpp
    src/terminal.cpp
    src/input.cpp
    src/controls.cpp
    src/options.cpp
)
target_include_directories(term3d_core PUBLIC src)
target_compile_options(term3d_core PRIVATE ${TERM3D_WARNINGS})

add_executable(term3d src/main.cpp)
target_link_libraries(term3d PRIVATE term3d_core)
target_compile_options(term3d PRIVATE ${TERM3D_WARNINGS})

# Unit tests: build with the project by default, run with `ctest`.
# Turn off with -DTERM3D_BUILD_TESTS=OFF.
option(TERM3D_BUILD_TESTS "Build the unit tests" ON)
if(TERM3D_BUILD_TESTS)
    enable_testing()
    add_executable(term3d_tests
        tests/test_main.cpp
        tests/test_math.cpp
        tests/test_obj_loader.cpp
        tests/test_mesh.cpp
        tests/test_input.cpp
        tests/test_options.cpp
        tests/test_controls.cpp
    )
    target_link_libraries(term3d_tests PRIVATE term3d_core)
    target_compile_options(term3d_tests PRIVATE ${TERM3D_WARNINGS})
    # Lets the tests load the models shipped in models/.
    target_compile_definitions(term3d_tests PRIVATE
        TERM3D_MODELS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/models")
    add_test(NAME unit_tests COMMAND term3d_tests)
endif()
