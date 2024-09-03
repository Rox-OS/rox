#if __cplusplus >= 202002L
#include <biron/util/allocator.cpp>
#include <biron/util/file.cpp>
#include <biron/util/format.cpp>
#include <biron/util/pool.cpp>
#include <biron/util/string.cpp>
#include <biron/ast_attr.cpp>
#include <biron/ast_const.cpp>
#include <biron/ast_expr.cpp>
#include <biron/ast_stmt.cpp>
#include <biron/ast_type.cpp>
#include <biron/ast_unit.cpp>
#include <biron/cg_const.cpp>
#include <biron/cg_expr.cpp>
#include <biron/cg_stmt.cpp>
#include <biron/cg_type.cpp>
#include <biron/cg_unit.cpp>
#include <biron/cg_value.cpp>
#include <biron/cg.cpp>
#include <biron/cpprt.cpp>
#include <biron/diagnostic.cpp>
#include <biron/lexer.cpp>
#include <biron/llvm.cpp>
#include <biron/main.cpp>
#include <biron/parser.cpp>
#if defined(_WIN32)
#error Please implement system_windows.cpp
#include <biron/system_windows.cpp>
#else
#include <biron/system_linux.cpp>
#endif
#else
#error C++20 compiler is required to build Biron
#endif
