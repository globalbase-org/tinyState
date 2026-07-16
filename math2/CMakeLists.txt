include(${PROJECT_SOURCE_DIR}/cmake/AddTinyStateLibrary.cmake)

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
)
