# SPDX-License-Identifier: MPL-2.0
# README.md

cmake_minimum_required(VERSION 3.20)
project(OGSimulationStandalone CXX)

option(OG_STANDALONE_CORE_ONLY "Build og-simulation, og-simulation-jolt and their tests only: og-net, og-sim-net and og-sim-session are not added" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
    add_compile_options(/Zc:preprocessor /utf-8 /bigobj)
endif()
add_compile_definitions(OG_STANDALONE_BUILD)
enable_testing()

set(OG_STANDALONE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../Source")
set(OG_SIMULATION_TESTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../Source/OGSimulationTests/extern/og-simulation-tests" CACHE PATH "Root of an og-simulation-tests checkout (the directory that holds Source/OGSimulationTests/)")
set(OGNET_CATCH2_DIR "" CACHE PATH "Directory holding catch_amalgamated.hpp and catch_amalgamated.cpp (Catch2 v3), e.g. the engine's Engine/Source/ThirdParty/Catch2/v3.4.0/extras")

if(NOT EXISTS "${OGNET_CATCH2_DIR}/catch_amalgamated.hpp" OR NOT EXISTS "${OGNET_CATCH2_DIR}/catch_amalgamated.cpp")
    message(FATAL_ERROR "og-standalone: set OGNET_CATCH2_DIR to a directory holding catch_amalgamated.hpp and catch_amalgamated.cpp (Catch2 v3)")
endif()
if(NOT EXISTS "${OG_SIMULATION_TESTS_DIR}/Source/OGSimulationTests/CMakeLists.txt")
    message(FATAL_ERROR "og-standalone: set OG_SIMULATION_TESTS_DIR to the root of an og-simulation-tests checkout")
endif()

add_library(Catch2 STATIC "${OGNET_CATCH2_DIR}/catch_amalgamated.cpp")
target_include_directories(Catch2 PUBLIC "${OGNET_CATCH2_DIR}")

add_subdirectory("${OG_STANDALONE_SOURCE}/OGSimulation/og-simulation/glm" glm)
add_subdirectory("${OG_STANDALONE_SOURCE}/OGSimulation/og-simulation/OGSimulation" og_simulation)
add_subdirectory("${OG_STANDALONE_SOURCE}/OGSimulationJolt/og-simulation-jolt" og_simulation_jolt)
add_subdirectory("${OG_SIMULATION_TESTS_DIR}/Source/OGSimulationTests" og_simulation_tests)

if(OG_STANDALONE_CORE_ONLY)
    message(STATUS "og-standalone: core only; og-net, og-sim-net and og-sim-session are not added")
else()
    add_subdirectory("${OG_STANDALONE_SOURCE}/OGNet/og-net" og_net)
    add_subdirectory("${OG_STANDALONE_SOURCE}/OGSimulationNet/og-sim-net" og_sim_net)
    add_subdirectory("${OG_STANDALONE_SOURCE}/OGSimulationSession/og-sim-session" og_sim_session)
endif()
