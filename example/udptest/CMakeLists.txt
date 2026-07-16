cmake_minimum_required(VERSION 3.16)
project(udptest LANGUAGES CXX)

# Requires tinyState to be installed first (cmake --install of proj/cmake).
# If the prefix is not a standard system location, point CMake at it:
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/prefix
find_package(tinyState REQUIRED)

# UDP loopback datagram test (WSASendTo/WSARecvFrom on Windows).
add_tinystate_example(NAME udptest
  SOURCES
    src/main/main.cpp
    src/classes/hw/c++/hwUdpTest.cpp)
