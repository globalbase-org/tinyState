cmake_minimum_required(VERSION 3.16)
project(mmsgtest LANGUAGES CXX)

# Requires tinyState to be installed first (cmake --install of proj/cmake).
# If the prefix is not a standard system location, point CMake at it:
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/prefix
find_package(tinyState REQUIRED)

# Throughput smoke test (recvmmsg/sendmmsg). Winsock link libs and -static come
# from tinyState::tinyState2 on Windows automatically.
add_tinystate_example(NAME mmsgtest
  SOURCES
    src/main/main.cpp
    src/classes/hw/c++/hwMmsgTest.cpp)
