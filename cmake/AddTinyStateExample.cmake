# AddTinyStateExample.cmake — build a tinyState application/example against an
# installed tinyState prefix.  Installed alongside tinyStateConfig.cmake and
# pulled in automatically by find_package(tinyState).  Windows-port design memo.
#
#   add_tinystate_example(NAME <target>
#                         SOURCES <main.cpp> <classes/.../foo.cpp> ...)
#
# Sources whose path contains "/classes/" define tinyState classes and are run
# through the tscpp2 code generator first (producing <build>/_ts2/c++/*.h);
# the rest (e.g. src/main/main.cpp) are compiled as-is.  All platform include
# dirs / link libs / compile flags are inherited from tinyState::tinyState2.

function(add_tinystate_example)
  cmake_parse_arguments(TSE "" "NAME" "SOURCES" ${ARGN})

  if(NOT TSE_NAME)
    message(FATAL_ERROR "add_tinystate_example: NAME is required")
  endif()
  if(NOT TSE_SOURCES)
    message(FATAL_ERROR "add_tinystate_example: SOURCES is required")
  endif()

  set(_stamp_dir ${CMAKE_CURRENT_BINARY_DIR}/stamps)
  file(MAKE_DIRECTORY ${_stamp_dir})

  # tscpp2 codegen for every tinyState class source (path under /classes/).
  set(_stamps)
  foreach(src ${TSE_SOURCES})
    get_filename_component(_abs ${src} ABSOLUTE)
    if(_abs MATCHES "/classes/")
      string(REPLACE "/" "_" _stamp_name ${_abs})
      set(_stamp ${_stamp_dir}/${_stamp_name}.t)
      # Declare tscpp2's generated headers as real OUTPUTs (not just the stamp) so a
      # base-class edit rebuilds dependent subclass objects in ONE incremental pass
      # (same fix / rationale as AddTinyStateLibrary.cmake).  tscpp2 writes
      # <baseheader>/_ts2/c++/<basename>_{,pb}.h.
      get_filename_component(_gen_base ${_abs} NAME_WLE)
      set(_gen_priv ${CMAKE_CURRENT_BINARY_DIR}/_ts2/c++/${_gen_base}_.h)
      set(_gen_pub  ${CMAKE_CURRENT_BINARY_DIR}/_ts2/c++/${_gen_base}_pb.h)
      add_custom_command(
        OUTPUT ${_stamp} ${_gen_priv} ${_gen_pub}
        COMMAND ${TINYSTATE_TSCPP2} file ${_abs}
                --baseheader=${CMAKE_CURRENT_BINARY_DIR}
                --header=_ts2
        COMMAND ${CMAKE_COMMAND} -E touch ${_stamp}
        DEPENDS ${_abs}
        COMMENT "tscpp2: generating headers for ${src}"
        VERBATIM)
      list(APPEND _stamps ${_stamp})
    endif()
  endforeach()

  add_custom_target(${TSE_NAME}_codegen DEPENDS ${_stamps})

  add_executable(${TSE_NAME} ${TSE_SOURCES})
  add_dependencies(${TSE_NAME} ${TSE_NAME}_codegen)

  target_include_directories(${TSE_NAME} PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}                 # this example's generated _ts2/
    ${CMAKE_CURRENT_SOURCE_DIR}/src/h)          # this example's own headers
  target_link_libraries(${TSE_NAME} PRIVATE tinyState::tinyState2)

  # By default an imported target's INTERFACE includes reach consumers as
  # -isystem.  On macOS clang searches /usr/local/include (a default system dir)
  # BEFORE CMake's -isystem dirs, so a stale /usr/local tinyState install would
  # shadow this prefix's headers.  NO_SYSTEM_FROM_IMPORTED makes tinyState's
  # includes plain -I (searched before /usr/local), so the freshly-installed
  # headers always win.
  set_target_properties(${TSE_NAME} PROPERTIES NO_SYSTEM_FROM_IMPORTED TRUE)
endfunction()
