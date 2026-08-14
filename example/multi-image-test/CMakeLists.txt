# 「exe + 共有ライブラリ」の両方が sCallSection::key に触れ、同じ実体を見ているかを
# 確認する。tinyState を共有ライブラリとして使う構成でのみ意味があるので、
# TINYSTATE_BUILD_SHARED=ON のときだけ定義する。
#
#   cmake -B build -DTINYSTATE_BUILD_SHARED=ON .
#   cmake --build build --target multi_image_test
#   ./build/example/multi-image-test/multi_image_test
#
# ELF では修正前から通る (STB_GNU_UNIQUE が統一するため)。差が出るのは PE なので、
# 意味のある検証は Windows 実機で行う。docs/GOTCHAS.md §13。

if(NOT TINYSTATE_BUILD_SHARED)
  return()
endif()

add_library(multi_image_lib SHARED src/lib.cpp)
target_link_libraries(multi_image_lib PRIVATE tinyState2)

add_executable(multi_image_test src/main.cpp)
target_link_libraries(multi_image_test PRIVATE multi_image_lib tinyState2)
