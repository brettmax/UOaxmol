# Standalone build of the AxmolUO core and its tests (no engine required):
#   cmake -S uo -B build/uo && cmake --build build/uo && ctest --test-dir build/uo
# The Axmol client lives in uo/client and is configured as a regular Axmol project.
cmake_minimum_required(VERSION 3.22)
project(AxmolUO LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(core)

option(UO_BUILD_TESTS "Build AxmolUO core tests" ON)
if(UO_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# Offline asset converter (uo/tools/README.md). Needs zlib; skipped with a warning without it.
option(UO_BUILD_TOOLS "Build the uoconvert asset converter" ON)
if(UO_BUILD_TOOLS)
  add_subdirectory(tools/uoconvert)
endif()
