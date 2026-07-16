cmake_minimum_required(VERSION 3.16)

if ( APPLE )
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
endif()

project(tinyState VERSION 2.0.0 LANGUAGES C CXX)

include(GNUInstallDirs)

# ---- version / revision -----------------------------------------------------
# The semantic version is the single source of truth: project(... VERSION ...).
# It flows into tinyState_config.h (TS_VERSION*) and the CMake package version
# file (find_package(tinyState 2.0) becomes version-aware).
#
# In addition, a `git describe` string (nearest tag + commits-since + short
# hash + -dirty) is captured at *configure* time as build metadata and embedded
# as TS_REVISION.  This replaces the old SVN-based utils/version.pl.  Outside a
# git checkout (release tarball) it degrades to "unknown" and the build still
# works.  Note: the snapshot is taken at configure, so commits/edits made after
# `cmake -B build .` are not reflected until the next configure.
find_package(Git QUIET)
set(TS_REVISION "unknown")
if(GIT_FOUND AND EXISTS ${PROJECT_SOURCE_DIR}/.git)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty --long
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    OUTPUT_VARIABLE TS_REVISION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
  if(TS_REVISION STREQUAL "")
    set(TS_REVISION "unknown")
  endif()
endif()
message(STATUS "tinyState ${PROJECT_VERSION} (${TS_REVISION})")

if ( APPLE )
  set(CMAKE_OBJCXX_COMPILER clang++)
  enable_language(OBJCXX)
endif()


find_package(CGAL QUIET)
find_package(Threads REQUIRED QUIET)
set(HAVE_PTHREAD 1)

include(CheckIncludeFile)
if(CYGWIN)
  set(HAVE_SIGNAL 1)
  set(HAVE_POSIX_SIGNAL 0)
elseif(UNIX)
  set(HAVE_SIGNAL 1)
  set(HAVE_POSIX_SIGNAL 1)
else()
  set(HAVE_SIGNAL 0)
endif()

set(HAVE_CGAL ${CGAL_FOUND})
message(STATUS "CGAL_FOUND=${CGAL_FOUND}")
message(STATUS "HAVE_CGAL=${HAVE_CGAL}")

include(CheckStructHasMember)
check_struct_has_member(
  "struct sockaddr"
  sa_len
  "sys/types.h;sys/socket.h"
  HAVE_SA_LEN
)


# Darwin 専用（最初の一発目）
#if(NOT APPLE)
#  message(FATAL_ERROR "This CMake setup is for Darwin only")
#endif()

# 未指定なら Debug (-g 付き、最適化なし)。-DCMAKE_BUILD_TYPE=Release などで上書き可
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()


# 出力先（Makefile の bin/lib 相当）
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY
    ${CMAKE_BINARY_DIR}/lib)

# universal binary
add_compile_options(
  	$<$<COMPILE_LANGUAGE:CXX>:-frtti>
	$<$<COMPILE_LANGUAGE:CXX>:-std=gnu++2a>
	$<$<COMPILE_LANGUAGE:CXX>:-Wreturn-type>
	$<$<COMPILE_LANGUAGE:CXX>:-D__EXTENSIONS__>
)

set(SRC ${PROJECT_SOURCE_DIR}/src)
set(WORK ${CMAKE_BINARY_DIR})
set(STLCPP ${PROJECT_SOURCE_DIR}/utils/tscpp2)

configure_file(
  ${PROJECT_SOURCE_DIR}/cmake/config.h.in
  ${CMAKE_BINARY_DIR}/tinyState_config.h
)

# ---- arch overlay ----
# OS / ディストリごとに散在する arch/<layer>/src/{h,classes} を symlink で
# build/arch_overlay/ に重ね合わせる。後段の layer が前段を上書きする。
# 旧来の「src/h 配下に symlink を張る」方式の代替で、ソースツリーは無変更。
set(ARCH_DIR ${PROJECT_SOURCE_DIR}/arch)
set(ARCH_OVERLAY ${CMAKE_BINARY_DIR}/arch_overlay)

set(arch_layers posix)
if(APPLE)
  list(APPEND arch_layers posix_Darwin posix_Darwin_OSX)
elseif(CYGWIN)
  list(APPEND arch_layers posix_Cygwin)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  list(APPEND arch_layers posix_Linux)
  if(EXISTS /etc/debian_version)
    list(APPEND arch_layers posix_Linux_Debian)
  elseif(EXISTS /etc/redhat-release)
    list(APPEND arch_layers posix_Linux_Redhat)
  endif()
elseif(WIN32)
  # MinGW (MSYS2 mingw-w64). CYGWIN is caught by the branch above, so WIN32
  # here means the native-ish MinGW target. See Windows-port design memo.
  list(APPEND arch_layers posix_MinGW)
endif()
message(STATUS "arch overlay layers: ${arch_layers}")

# ts2System child-exit detection mode (MinGW only, Windows-port design memo).
#   1 = (default, primary) callback drives eventHandler directly on the OS
#       threadpool thread; reactor kept alive via fwIO::addRefio/delRefio.
#   2 = callback only PostQueuedCompletionStatus; eventHandler re-runs on the
#       fwIO main-loop thread (kept for observation; delete once 1 is settled).
# Both are correct on real Windows (1000/1000).  Switch: -DTS2SYS_CHILDWAIT_MODE=2
if(WIN32)
  set(TS2SYS_CHILDWAIT_MODE 1 CACHE STRING
      "ts2System child-exit mode: 1=direct+addRefio (default), 2=PQCS+mainloop")
  set_property(CACHE TS2SYS_CHILDWAIT_MODE PROPERTY STRINGS 1 2)
  add_compile_definitions(TS2SYS_CHILDWAIT_MODE=${TS2SYS_CHILDWAIT_MODE})
  message(STATUS "ts2System child-exit mode: ${TS2SYS_CHILDWAIT_MODE}")
endif()

file(REMOVE_RECURSE ${ARCH_OVERLAY})
file(MAKE_DIRECTORY ${ARCH_OVERLAY})
foreach(layer ${arch_layers})
  set(layer_src ${ARCH_DIR}/${layer}/src)
  if(IS_DIRECTORY ${layer_src})
    file(GLOB_RECURSE files RELATIVE ${layer_src} ${layer_src}/*)
    foreach(rel ${files})
      set(dst ${ARCH_OVERLAY}/${rel})
      get_filename_component(dst_dir ${dst} DIRECTORY)
      file(MAKE_DIRECTORY ${dst_dir})
      file(REMOVE ${dst})
      if(WIN32)
        # NTFS symlink needs SeCreateSymbolicLinkPrivilege (dev-mode / admin).
        # Hard link works with no privilege; overlay is same-volume, per-file
        # (GLOB_RECURSE), so the hard-link constraints are satisfied. Windows-port design memo §6.2.
        file(CREATE_LINK ${layer_src}/${rel} ${dst})
      else()
        file(CREATE_LINK ${layer_src}/${rel} ${dst} SYMBOLIC)
      endif()
    endforeach()
  endif()
endforeach()

# 共通 include
include_directories(
  ${CMAKE_BINARY_DIR}
  ${ARCH_OVERLAY}/h
  ${SRC}/h
)
if(APPLE)
  include_directories(/opt/homebrew/include)
endif()

# ---- MinGW: vendored TRE for POSIX regex ----
# mingw-w64 に POSIX <regex.h> が無い。stdRx/stdString は regcomp/regexec を使うので
# MinGW ターゲットに限り POSIX 準拠 (leftmost-longest / 線形時間) の TRE を bundle。
# WIN32 && !CYGWIN (= MinGW) のみ。Linux/macOS/Cygwin は system regex。Windows-port design memo。
if(WIN32 AND NOT CYGWIN)
  set(TRE_DIR ${ARCH_DIR}/posix_MinGW/vendor/tre)
  file(GLOB TRE_SOURCES ${TRE_DIR}/*.c)
  add_library(tre_mingw OBJECT ${TRE_SOURCES})
  target_include_directories(tre_mingw PRIVATE ${TRE_DIR} ${TRE_DIR}/include)
  target_compile_definitions(tre_mingw PRIVATE HAVE_CONFIG_H)
  # stdRx の `#include <regex.h>` を vendored TRE の regex.h に解決させる。
  include_directories(${TRE_DIR}/include)
endif()

# サブライブラリ
add_subdirectory(ts2)
add_subdirectory(math2)

# MinGW: TRE オブジェクトを libtinyState2 に取り込む (consumer は追加リンク不要)。
if(WIN32 AND NOT CYGWIN)
  target_sources(tinyState2 PRIVATE $<TARGET_OBJECTS:tre_mingw>)
endif()


# ---- install ----

# 1. 共通ヘッダ (src/h 配下、OS 非依存)
install(DIRECTORY ${SRC}/h/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")

# 2. arch overlay (OS 依存ヘッダ) を上書き install
#    arch_overlay/h 配下は symlink なので、FOLLOW_SYMLINK_CHAIN で実体コピー
install(CODE "
  file(COPY \"${ARCH_OVERLAY}/h/\"
       DESTINATION \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}\"
       FOLLOW_SYMLINK_CHAIN
       FILES_MATCHING PATTERN \"*.h\" PATTERN \"*.hpp\")
")

# 2b. MinGW: vendored TRE の公開ヘッダを install し、consumer の `#include <regex.h>`
#     ( stdRx.h 経由 ) を解決可能にする。Linux/macOS/Cygwin は system regex なので不要。
if(WIN32 AND NOT CYGWIN)
  install(FILES
    ${TRE_DIR}/include/regex.h
    ${TRE_DIR}/include/tre.h
    ${TRE_DIR}/include/tre-config.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()

# 3. tscpp2 が生成したヘッダ (build/_ts2/)
install(DIRECTORY ${CMAKE_BINARY_DIR}/_ts2
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# 4. configure_file 生成の tinyState_config.h
install(FILES ${CMAKE_BINARY_DIR}/tinyState_config.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/std2)

# 5. Perl スクリプト群 (proj/utils/)
install(PROGRAMS
  ${PROJECT_SOURCE_DIR}/utils/tscpp2
  ${PROJECT_SOURCE_DIR}/utils/tslink2
  ${PROJECT_SOURCE_DIR}/utils/tsjslink2
  ${PROJECT_SOURCE_DIR}/utils/tinyState2
  ${PROJECT_SOURCE_DIR}/utils/gtt2
  ${PROJECT_SOURCE_DIR}/utils/tspig2
  ${PROJECT_SOURCE_DIR}/utils/jscopy2
  DESTINATION ${CMAKE_INSTALL_BINDIR})

# On Windows, ship a tscpp2.bat next to tscpp2 so cmd.exe callers (ninja's
# `cmd /C ...`) resolve it via PATHEXT and run the perl script.  Windows-port design memo.
if(WIN32)
  install(PROGRAMS ${PROJECT_SOURCE_DIR}/utils/tscpp2.bat
          DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# 6. CMake package config: lets applications consume the installed prefix with
#    find_package(tinyState) + add_tinystate_example().  Relocatable (locates
#    the prefix relative to itself).  Same for Linux / Cygwin / MinGW. Windows-port design memo.
#    The version file makes find_package(tinyState 2.0 REQUIRED) version-aware
#    (SameMajorVersion: any 2.x satisfies a 2.y request, 3.x does not).
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  ${CMAKE_BINARY_DIR}/tinyStateConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)
install(FILES
  ${PROJECT_SOURCE_DIR}/cmake/tinyStateConfig.cmake
  ${PROJECT_SOURCE_DIR}/cmake/AddTinyStateExample.cmake
  ${CMAKE_BINARY_DIR}/tinyStateConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tinyState)
