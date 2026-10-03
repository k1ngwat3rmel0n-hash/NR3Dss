cmake_minimum_required(VERSION 3.16)
project(nr3ds_core_smoketest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(nr3ds_core ../core/nr_physics.cpp ../core/nr_race.cpp ../core/nr_garage.cpp ../core/nr_world.cpp ../core/nr_source_geometry.cpp)
target_include_directories(nr3ds_core PUBLIC ../core)
if (MSVC)
    target_compile_options(nr3ds_core PRIVATE /W4 /O2)
else()
    target_compile_options(nr3ds_core PRIVATE -Wall -Wextra -Wpedantic -O2)
endif()

add_executable(nr3ds_sim sim_main.cpp)
target_link_libraries(nr3ds_sim PRIVATE nr3ds_core)

add_executable(nr3ds_tests tests.cpp)
target_link_libraries(nr3ds_tests PRIVATE nr3ds_core)

enable_testing()
add_test(NAME core_smoke COMMAND nr3ds_tests)
