cmake_minimum_required(VERSION 3.16)
project(unixtest LANGUAGES CXX)

# Requires tinyState to be installed first (cmake --install of proj/cmake).
# If the prefix is not a standard system location, point CMake at it:
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/prefix
find_package(tinyState REQUIRED)

# AF_UNIX client/server test (afunix.h on Windows 10+).
add_tinystate_example(NAME unixtest
  SOURCES
    src/main/main.cpp
    src/classes/hw/c++/hwUnixTest.cpp)
