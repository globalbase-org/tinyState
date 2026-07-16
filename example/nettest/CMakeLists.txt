cmake_minimum_required(VERSION 3.16)
project(nettest LANGUAGES CXX)

# Requires tinyState to be installed first (cmake --install of proj/cmake).
# If the prefix is not a standard system location, point CMake at it:
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/prefix
find_package(tinyState REQUIRED)

# Cross-machine TCP test; one binary, role chosen at runtime:
#   nettest server <port>          bind INADDR_ANY:port, accept, PING->PONG
#   nettest client <host> <port>   connect, send PING, expect PONG
add_tinystate_example(NAME nettest
  SOURCES
    src/main/main.cpp
    src/classes/hw/c++/hwNetTest.cpp)
