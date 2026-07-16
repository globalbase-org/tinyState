cmake_minimum_required(VERSION 3.16)
project(systest LANGUAGES CXX)

# Requires tinyState to be installed first (cmake --install of proj/cmake).
# If the prefix is not a standard system location, point CMake at it:
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/prefix
find_package(tinyState REQUIRED)

# ts2System child-process spawn/exit test (CreateProcess + IOCP pipes on Windows).
add_tinystate_example(NAME systest
  SOURCES
    src/main/main.cpp
    src/classes/hw/c++/hwSysTest.cpp)
