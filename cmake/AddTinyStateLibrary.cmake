

function(add_tinystate_library)
  set(options)
  set(oneValueArgs NAME)
  set(multiValueArgs SOURCES DARWIN_SOURCES ARCH_SOURCES DEPENDS_CODEGEN)

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

	add_custom_command(
		OUTPUT ${stamp_file}
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


  add_library(${TS_NAME} STATIC)

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

  # ライブラリ
  install(TARGETS ${TS_NAME}
          ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )
endfunction()


