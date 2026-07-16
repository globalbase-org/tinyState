cmake_minimum_required(VERSION 3.16)
project(naming-conventions LANGUAGES CXX)

# Requires tinyState to be installed first (cmake --install of proj/cmake).
# If the prefix is not a standard system location, point CMake at it:
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/prefix
find_package(tinyState REQUIRED)

# Uses the "ss" naming prefix instead of "hw"; add_tinystate_example still
# runs tscpp2 on any source under a /classes/ directory.
add_tinystate_example(NAME naming-conventions
  SOURCES
    src/main/main.cpp
    src/classes/ss/c++/ssObject.cpp
    src/classes/ss/c++/ssApplication.cpp)
