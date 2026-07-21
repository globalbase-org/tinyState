cmake_minimum_required(VERSION 3.16)
project(riobench LANGUAGES CXX)

# Requires tinyState to be installed first (cmake --install of the repo build).
# If the prefix is not a standard system location, point CMake at it:
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/prefix
find_package(tinyState REQUIRED)

# Datagram throughput / burst-loss harness (sendmmsg/recvmmsg; RIO recv-depth knob).
# Winsock link libs and -static come from tinyState::tinyState2 on Windows automatically.
add_tinystate_example(NAME riobench
  SOURCES
    src/main/main.cpp
    src/classes/hw/c++/hwRioBench.cpp)
