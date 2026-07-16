cmake_minimum_required(VERSION 3.16)
project(interval-timer LANGUAGES CXX)

# Requires tinyState to be installed first (cmake --install of proj/cmake).
# If the prefix is not a standard system location, point CMake at it:
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/prefix
find_package(tinyState REQUIRED)

add_tinystate_example(NAME interval-timer
  SOURCES
    src/main/main.cpp
    src/classes/hw/c++/hwIntervalTimer.cpp
    src/classes/hw/c++/hwPiCalc.cpp
    src/classes/hw/c++/hwMain.cpp)
