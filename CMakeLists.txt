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

add_executable(term3d
    src/main.cpp
    src/obj_loader.cpp
    src/mesh.cpp
    src/renderer.cpp
    src/framebuffer.cpp
    src/terminal.cpp
    src/input.cpp
    src/controls.cpp
    src/options.cpp
)

# Turn on the useful warnings so the compiler can catch mistakes for us.
target_compile_options(term3d PRIVATE -Wall -Wextra -Wpedantic)
