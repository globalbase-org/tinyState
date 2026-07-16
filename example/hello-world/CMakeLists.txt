cmake_minimum_required(VERSION 3.16)
project(hello-world LANGUAGES CXX)

# Requires tinyState to be installed first (cmake --install of proj/cmake).
# Point CMake at the install prefix if it is not a standard system location:
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/prefix
find_package(tinyState REQUIRED)

# Sources under src/classes/ are tinyState classes (tscpp2-generated); main.cpp
# is plain.  Compile flags / include dirs / link libs come from the package.
add_tinystate_example(NAME hello-world
  SOURCES
    src/main/main.cpp
    src/classes/hw/c++/hwHello.cpp)
