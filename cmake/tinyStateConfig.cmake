# tinyStateConfig.cmake — CMake package config for the installed tinyState2
# framework.  Consumed by applications/examples via find_package(tinyState).
#
#   find_package(tinyState REQUIRED)          # point at prefix with CMAKE_PREFIX_PATH
#   add_tinystate_example(NAME app SOURCES src/main/main.cpp src/classes/.../foo.cpp)
#
# This file is installed to <prefix>/lib/cmake/tinyState/ and locates the rest
# of the install *relative to itself*, so the prefix is fully relocatable
# (move/copy/scp the whole prefix and it still works).

get_filename_component(TINYSTATE_PREFIX "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)

include(CMakeFindDependencyMacro)
if(NOT WIN32)
  # POSIX / Cygwin link against pthread (Threads::Threads).  Native Windows
  # (MinGW) uses winsock instead and needs no Threads module.
  find_dependency(Threads)
endif()

# --- tscpp2 code generator ---------------------------------------------------
# tscpp2 is a #!/usr/bin/perl script invoked as-is.  On Windows a sibling
# tscpp2.bat (installed next to it) is picked up by cmd.exe via PATHEXT and
# runs the script through perl; MSYS2/Cygwin bash uses the shebang directly.
# Either way callers invoke plain `tscpp2`.  Windows-port design memo.
set(TINYSTATE_TSCPP2 "${TINYSTATE_PREFIX}/bin/tscpp2"
    CACHE FILEPATH "tinyState code generator (tscpp2)")

# --- imported library targets ----------------------------------------------
# The framework is a *static* library, so the platform link deps (winsock on
# Windows, pthread/rt/dl on Linux) are the consumer's responsibility and are
# carried here as INTERFACE properties so applications inherit them for free.
if(NOT TARGET tinyState::tinyState2)
  add_library(tinyState::tinyState2 STATIC IMPORTED)
  set_target_properties(tinyState::tinyState2 PROPERTIES
    IMPORTED_LOCATION "${TINYSTATE_PREFIX}/lib/libtinyState2.a"
    INTERFACE_INCLUDE_DIRECTORIES "${TINYSTATE_PREFIX}/include;${TINYSTATE_PREFIX}/include/std2"
    INTERFACE_COMPILE_OPTIONS "$<$<COMPILE_LANGUAGE:CXX>:-std=gnu++2a>;$<$<COMPILE_LANGUAGE:CXX>:-frtti>;$<$<COMPILE_LANGUAGE:CXX>:-D__EXTENSIONS__>")

  if(WIN32)
    # MinGW native PE (MSYS2) or MinGW cross.  CYGWIN is *not* WIN32, so it
    # falls through to the POSIX branch below.
    set_target_properties(tinyState::tinyState2 PROPERTIES
      INTERFACE_LINK_LIBRARIES "ws2_32;mswsock;bcrypt"
      INTERFACE_LINK_OPTIONS "-static;-static-libgcc;-static-libstdc++")
  else()
    set_property(TARGET tinyState::tinyState2 PROPERTY
      INTERFACE_LINK_LIBRARIES Threads::Threads m)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
      set_property(TARGET tinyState::tinyState2 APPEND PROPERTY
        INTERFACE_LINK_LIBRARIES rt dl)
    endif()
  endif()
endif()

if(NOT TARGET tinyState::tinyState2Math)
  add_library(tinyState::tinyState2Math STATIC IMPORTED)
  set_target_properties(tinyState::tinyState2Math PROPERTIES
    IMPORTED_LOCATION "${TINYSTATE_PREFIX}/lib/libtinyState2Math.a"
    INTERFACE_INCLUDE_DIRECTORIES "${TINYSTATE_PREFIX}/include;${TINYSTATE_PREFIX}/include/std2"
    INTERFACE_LINK_LIBRARIES tinyState::tinyState2)
endif()

# --- application/example build helper --------------------------------------
include("${CMAKE_CURRENT_LIST_DIR}/AddTinyStateExample.cmake")
