cmake_minimum_required(VERSION 3.16)
project(regextest LANGUAGES CXX)

# Regression test for stdRx / the platform regex engine: asserts POSIX ERE
# leftmost-longest semantics (the property that distinguishes a true POSIX
# engine from std::regex's leftmost-first).  Runs on every platform, so it
# also guards the MinGW vendored-TRE path.
find_package(tinyState REQUIRED)

add_tinystate_example(NAME regextest
  SOURCES
    src/main/main.cpp
    src/classes/hw/c++/hwRegexTest.cpp)
