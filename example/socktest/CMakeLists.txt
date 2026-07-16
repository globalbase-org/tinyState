cmake_minimum_required(VERSION 3.16)
project(socktest LANGUAGES CXX)

# Requires tinyState to be installed first (cmake --install of proj/cmake).
# If the prefix is not a standard system location, point CMake at it:
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/prefix
find_package(tinyState REQUIRED)

# TCP loopback PING/PONG (server + connect in one state machine).
add_tinystate_example(NAME socktest
  SOURCES
    src/main/main.cpp
    src/classes/hw/c++/hwSockPoke.cpp
    src/classes/hw/c++/hwSockTest.cpp)
