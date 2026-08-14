cmake_minimum_required(VERSION 3.16)
project(riorace LANGUAGES CXX)

find_package(tinyState REQUIRED)

# RIO teardown-vs-completion churn harness.
add_tinystate_example(NAME riorace
  SOURCES
    src/main/main.cpp
    src/classes/hw/c++/hwRioRace.cpp)
