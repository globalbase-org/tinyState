# whole-archive 検証ターゲット。
#
# 「.a は参照を解決しない」ため、どの .o も引き込まれない限り未定義参照は表に
# 出ない。実際 stdBalancedTree の未定義 virtual と macOS の darwin_*
# 実装欠落は、どちらもこの盲点で長期間潜在化していた。
#
# ここでは *ビルドツリーの* ライブラリを全取り込み (--whole-archive /
# -force_load) して共有ライブラリを作り、未解決参照をリンクエラーにする。
# ビルドが通ること自体が検証結果。install 済みの古いライブラリを見ないよう
# find_package は使わず、in-tree ターゲットを直接参照する。
#
#   cmake --build <dir> --target whole_archive_test_so
#
# EXCLUDE_FROM_ALL なので通常ビルドには含まれない。

add_library(whole_archive_test_so SHARED src/main.cpp)

# 通常のリンク。プラットフォーム依存 (winsock / pthread / Foundation) と
# GMP/MPFR は各ターゲットの usage requirement として引き継がれる。ここで明示的に
# 足さないことで、その宣言が実際に効いているかどうかも同時に検証している。
target_link_libraries(whole_archive_test_so PRIVATE
  tinyState2
  tinyState2Math
)

if(NOT TINYSTATE_BUILD_SHARED)
  # STATIC ビルド時のみ全取り込みが意味を持つ。SHARED では .so/.dylib 自体が
  # 既に完全リンク済みで、それがそのまま同じ検証になっている。
  if(APPLE)
    # Apple ld: --whole-archive 相当は -force_load。未解決シンボルは dylib では
    # 既定でエラーなので --no-undefined に相当する指定は不要。
    target_link_options(whole_archive_test_so PRIVATE
      -Wl,-force_load,$<TARGET_FILE:tinyState2>
      -Wl,-force_load,$<TARGET_FILE:tinyState2Math>
    )
  else()
    target_link_options(whole_archive_test_so PRIVATE
      -Wl,--whole-archive,$<TARGET_FILE:tinyState2>,$<TARGET_FILE:tinyState2Math>,--no-whole-archive
      -Wl,--no-undefined
    )
  endif()
endif()
