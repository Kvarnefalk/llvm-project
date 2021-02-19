set(LLVM_LINK_COMPONENTS support)

add_clang_executable(clang-index-dump
  ClangIndexDump.cpp
  sqlite3.c
  sqlite3.h
  )
target_link_libraries(clang-index-dump
  PRIVATE
  clangIndex
  clangBasic
)

