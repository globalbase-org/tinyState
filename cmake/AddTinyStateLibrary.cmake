

function(add_tinystate_library)
  set(options)
  set(oneValueArgs NAME)
  set(multiValueArgs SOURCES DARWIN_SOURCES ARCH_SOURCES DEPENDS_CODEGEN LINK_LIBRARIES)

  cmake_parse_arguments(TS
    "${options}"
    "${oneValueArgs}"
    "${multiValueArgs}"
    ${ARGN}
  )

  list(TRANSFORM TS_SOURCES
  	PREPEND ${SRC}/
  )
  list(TRANSFORM TS_DARWIN_SOURCES
  	PREPEND ${SRC}/
  )
  list(TRANSFORM TS_ARCH_SOURCES
    PREPEND ${ARCH_OVERLAY}/
  )

  if(NOT TS_NAME)
    message(FATAL_ERROR "add_tinystate_library: NAME is required")
  endif()

  set(STAMP_DIR
	  ${CMAKE_BINARY_DIR}/stamps)

  file(MAKE_DIRECTORY ${STAMP_DIR})

  set(STAMPS)

  foreach(src ${TS_SOURCES} ${TS_ARCH_SOURCES})
  	      # src のパスを .t 相当の stamp に変換
    	string(REPLACE "/" "_" stamp ${src})
	set(stamp_file ${STAMP_DIR}/${stamp}.t)

	# tscpp2 emits ${CMAKE_BINARY_DIR}/_ts2/c++/<basename>_.h and _pb.h for each
	# source (utils/tscpp2 genOutputName: <baseheader>/<header>/c++/<basename>).
	# Declare those generated headers as REAL OUTPUTs — not just the stamp.  The
	# compiler depfiles already record which _.h each object includes (e.g. a
	# subclass .o includes its base's _.h for the layout), so once the build graph
	# knows codegen *produces* those headers, a base-class member change rebuilds
	# every subclass in ONE incremental pass.  With the stamp-only OUTPUT the
	# headers were invisible generated files: cross-class edits went stale and
	# produced silent wrong-layout binaries (needed a clean/second build).
	# Basenames are project-unique (no duplicate-output collisions across libs).
	get_filename_component(gen_base ${src} NAME_WLE)
	set(gen_priv ${CMAKE_BINARY_DIR}/_ts2/c++/${gen_base}_.h)
	set(gen_pub  ${CMAKE_BINARY_DIR}/_ts2/c++/${gen_base}_pb.h)

	add_custom_command(
		OUTPUT ${stamp_file} ${gen_priv} ${gen_pub}
		COMMAND ${STLCPP}
		    file
		    ${src}
            	    --baseheader=${CMAKE_BINARY_DIR}
            	    --header=_ts2
		COMMAND ${CMAKE_COMMAND} -E touch ${stamp_file}
    		DEPENDS ${src}
    		COMMENT "STLCPP processing ${src}"
  				)

	list(APPEND STAMPS ${stamp_file})
  endforeach()

  # フェーズ1完了ターゲット（Makefile の TSS_TARGET 相当）
  set(codegen_target ${TS_NAME}_codegen)
  add_custom_target(${codegen_target}
	DEPENDS ${STAMPS}
	)

  # SHARED ビルドか STATIC ビルドかを選択
  if(TINYSTATE_BUILD_SHARED)
    set(lib_type SHARED)
  else()
    set(lib_type STATIC)
  endif()

  add_library(${TS_NAME} ${lib_type})

  # PIC ビルド (既定 ON)。静的ライブラリのままだが、consumer が .a を
  # 共有ライブラリ (dlopen されるモジュール等) へ埋め込めるようにする。
  # ELF 以外 (PE/COFF) では no-op。
  set_target_properties(${TS_NAME} PROPERTIES
    POSITION_INDEPENDENT_CODE ${TINYSTATE_BUILD_PIC}
  )

  add_dependencies(${TS_NAME} ${codegen_target})

  # 他ライブラリの codegen 出力 (生成ヘッダ等) に依存する場合
  foreach(dep ${TS_DEPENDS_CODEGEN})
    add_dependencies(${TS_NAME} ${dep}_codegen)
  endforeach()

# 共通ソース
  target_sources(${TS_NAME} PRIVATE
    ${TS_SOURCES}
  )

  # arch_overlay 由来ソース (OS 依存、arch overlay から解決)
  if(TS_ARCH_SOURCES)
    target_sources(${TS_NAME} PRIVATE
      ${TS_ARCH_SOURCES}
    )
  endif()

  # Darwin 固有
  if(APPLE AND TS_DARWIN_SOURCES)
    target_sources(${TS_NAME} PRIVATE
      ${TS_DARWIN_SOURCES}
    )
  endif()

  # 共通 include
  target_include_directories(${TS_NAME}
    PRIVATE
	${CMAKE_BINARY_DIR}
    PUBLIC
      ${SRC}/src/h
  )

  # ライブラリリンク（GMP/MPFR など外部ライブラリ）
  if(TS_LINK_LIBRARIES)
    target_link_libraries(${TS_NAME} PUBLIC ${TS_LINK_LIBRARIES})
  endif()

  # ライブラリ
  if(TINYSTATE_BUILD_SHARED)
    # SHARED: .so/.dylib は LIBRARY、Windows の .dll は RUNTIME、そして PE の
    # インポートライブラリ (.dll.a) は ARCHIVE として扱われる。ARCHIVE を省くと
    # Windows で DLL に対してリンクする手段が無くなる (ELF/Mach-O では no-op)。
    install(TARGETS ${TS_NAME}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
  else()
    # STATIC: ARCHIVE のみ
    install(TARGETS ${TS_NAME}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
  endif()
endfunction()


