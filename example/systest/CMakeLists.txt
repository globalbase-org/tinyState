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

# A *tinyState* child that reads stdin via s2IOstd (overlapped) — used by
# systest Phase E (SYSTEST_BIG_EXE) to probe parent->child delivery to a child
# doing framework I/O rather than plain _read.
add_tinystate_example(NAME tschild
  SOURCES
    src/main/tschild_main.cpp
    src/classes/hw/c++/hwTsChild.cpp)

# teardown-race stress: spawn N ts2System children, mass-destroy() them at once
# then FIN the app — a concurrent-teardown trigger.
add_tinystate_example(NAME stress
  SOURCES
    src/main/stress_main.cpp
    src/classes/hw/c++/hwStress.cpp)
