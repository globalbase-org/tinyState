include(${PROJECT_SOURCE_DIR}/cmake/AddTinyStateLibrary.cmake)

# GMP/MPFR 探索。pkg-config を使用（QUIET で find_module 不在による warning を抑える）
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(GMP QUIET gmp)
  pkg_check_modules(MPFR QUIET mpfr)
endif()

# Fallback: 標準的なライブラリ名で直接リンク
if(NOT GMP_FOUND)
  set(GMP_LIBRARIES gmp)
endif()
if(NOT MPFR_FOUND)
  set(MPFR_LIBRARIES mpfr)
endif()

message(STATUS "GMP_LIBRARIES=${GMP_LIBRARIES}")
message(STATUS "MPFR_LIBRARIES=${MPFR_LIBRARIES}")

set(MATH2_SOURCES
  classes/mth2/c++/mMatrix.cpp
  classes/mth2/c++/mVector.cpp
  classes/mth2/c++/mthMatrix.cpp
  classes/mth2/c++/mComplex.cpp
  classes/mth2/c++/mthFFT.cpp
  classes/mth2/c++/mLongitudeLatitude.cpp
  classes/mth2/c++/mthControlOptimization.cpp
  classes/mth2/c++/mEllipsoid.cpp
  classes/mth2/c++/mthLeastSquares.cpp
  classes/mth2/c++/mthProgress.cpp
  classes/mth2/c++/mTangentCoordinate.cpp
  classes/mth2/c++/mRange.cpp
  classes/mth2/c++/mObject.cpp)

if ( HAVE_CGAL )
  set(MATH2_SOURCES ${MATH2_SOURCES} 
  classes/mth2/c++/mthBox.cpp
  classes/mth2/c++/mthPrism.cpp
  classes/mth2/c++/mthSurfaceMesh.cpp
  classes/mth2/c++/co_mthSurfaceMesh_write_STL.cpp)
endif()

add_tinystate_library(
  NAME tinyState2Math
  SOURCES ${MATH2_SOURCES}
  DEPENDS_CODEGEN tinyState2
  # tinyState2 は実リンク依存 (mth* クラスは tinyState_ を継承する)。STATIC では
  # 使用要件として記録されるだけだが、SHARED では実際にリンクされる。macOS の
  # dylib は未解決シンボルを許さないので、これが無いと dylib が作れない。
  LINK_LIBRARIES tinyState2 ${GMP_LIBRARIES} ${MPFR_LIBRARIES}
)
