# Cross-compile toolchain: Linux host -> x86_64-w64-mingw32 (MinGW-w64).
#
# tinyState uses the pthread API (TS_THREAD / sThreadMutex etc.), so the
# *POSIX* thread model (winpthreads) is required. The -posix suffixed drivers
# select it explicitly, independent of update-alternatives. See Windows-port design memo §4.1.
#
# Usage (from the repository root):
#   cmake -B build-mingw -DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/toolchain-mingw.cmake .
#   cmake --build build-mingw

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)

set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}-gcc-posix)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++-posix)
set(CMAKE_RC_COMPILER  ${TOOLCHAIN_PREFIX}-windres)

# Look for host programs on the host, but target libraries/headers only in the
# MinGW sysroot (avoids accidentally picking up Linux libs).
set(CMAKE_FIND_ROOT_PATH /usr/${TOOLCHAIN_PREFIX})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
