#ifndef BIRON_PARSER_H
#define BIRON_PARSER_H
#include <biron/lexer.h>
#include <biron/ast_unit.h>
#include <biron/diagnostic.h>
#include <biron/util/pool.h>
#include <biron/cg.h>

namespace Biron {

// Forward declarations
struct AstNode;

struct AstAttr;

struct AstExpr;
struct AstTupleExpr;
struct AstIntExpr;
struct AstFltExpr;
struct AstStrExpr;
struct AstBoolExpr;

struct AstType;
struct AstTupleType;
struct AstArgsType;
struct AstGroupType;
struct AstIdentType;
struct AstVarArgsType;
struct AstPtrType;
struct AstAtomType;
struct AstFnType;

struct AstStmt;
struct AstBlockStmt;
struct AstReturnStmt;
struct AstDeferStmt;
struct AstBreakStmt;
struct AstContinueStmt;
struct AstIfStmt;
struct AstLetStmt;
struct AstUsingStmt;
struct AstForStmt;
struct AstExprStmt;
struct AstAssignStmt;

struct Parser {
	constexpr Parser(Lexer& lexer, Diagnostic& diagnostic, Allocator& allocator) noexcept
		: m_arena{allocator}
		, m_lexer{lexer}
		, m_in_defer{false}
		, m_ast{allocator}
		, m_diagnostic{diagnostic}
	{
	}

	~Parser() noexcept = default;

	// Biron Expression
	[[nodiscard]] AstExpr*         parse_expr() noexcept;
	[[nodiscard]] AstTupleExpr*    parse_tuple_expr() noexcept;
	[[nodiscard]] AstIntExpr*      parse_int_expr() noexcept;
	[[nodiscard]] AstFltExpr*      parse_flt_expr() noexcept;
	[[nodiscard]] AstStrExpr*      parse_str_expr() noexcept;
	[[nodiscard]] AstBoolExpr*     parse_bool_expr() noexcept;
	[[nodiscard]] AstIntExpr*      parse_chr_expr() noexcept;

	// Types
	[[nodiscard]] AstType*         parse_type() noexcept;
	[[nodiscard]] AstTupleType*    parse_tuple_type(Maybe<Array<AstAttr*>>&& attrs) noexcept;
	[[nodiscard]] AstArgsType*     parse_args_type() noexcept;
	[[nodiscard]] AstGroupType*    parse_group_type() noexcept;
	[[nodiscard]] AstIdentType*    parse_ident_type(Array<AstAttr*>&& attrs) noexcept;
	[[nodiscard]] AstVarArgsType*  parse_varargs_type(Array<AstAttr*>&& attrs) noexcept;
	[[nodiscard]] AstPtrType*      parse_ptr_type(Array<AstAttr*>&& attrs) noexcept;
	[[nodiscard]] AstAtomType*     parse_atom_type(Array<AstAttr*>&& attrs) noexcept;
	[[nodiscard]] AstFnType*       parse_fn_type(Array<AstAttr*>&& attrs) noexcept;

	// Statements
	[[nodiscard]] AstStmt*         parse_stmt() noexcept;
	[[nodiscard]] AstBlockStmt*    parse_block_stmt() noexcept;
	[[nodiscard]] AstReturnStmt*   parse_return_stmt() noexcept;
	[[nodiscard]] AstDeferStmt*    parse_defer_stmt() noexcept;
	[[nodiscard]] AstBreakStmt*    parse_break_stmt() noexcept;
	[[nodiscard]] AstContinueStmt* parse_continue_stmt() noexcept;
	[[nodiscard]] AstIfStmt*       parse_if_stmt() noexcept;
	[[nodiscard]] AstStmt*         parse_let_stmt(Maybe<Array<AstAttr*>>&& attrs, Bool global) noexcept;
	[[nodiscard]] AstUsingStmt*    parse_using_stmt() noexcept;
	[[nodiscard]] AstForStmt*      parse_for_stmt() noexcept;
	[[nodiscard]] AstStmt*         parse_expr_stmt(Bool semi) noexcept;

	// Attributes
	[[nodiscard]] Maybe<Array<AstAttr*>>  parse_attrs() noexcept;

	// Top-level elements
	[[nodiscard]] AstFn*           parse_fn(Array<AstAttr*>&& attrs) noexcept;
	[[nodiscard]] AstTypedef*      parse_typedef(Array<AstAttr*>&& attrs) noexcept;
	[[nodiscard]] AstModule*       parse_module() noexcept;
	[[nodiscard]] AstImport*       parse_import() noexcept;
	[[nodiscard]] AstEffect*       parse_effect() noexcept;

	Maybe<Ast> parse() noexcept;

private:
	template<typename... Ts>
	Error error(Range range, StringView message, Ts&&... args) const noexcept {
		m_diagnostic.error(range, message, forward<Ts>(args)...);
		return {};
	}
	template<typename... Ts>
	Error fatal(Range range, StringView message, Ts&&... args) const noexcept {
		m_diagnostic.fatal(range, message, forward<Ts>(args)...);
		return {};
	}
	template<typename... Ts>
	Error error(StringView message, Ts&&... args) const noexcept {
		return error(m_this_token.range, message, forward<Ts>(args)...);
	}
	template<typename... Ts>
	Error fatal(StringView message, Ts&&... args) const noexcept {
		return fatal(m_this_token.range, message, forward<Ts>(args)...);
	}

	Error oom() const noexcept {
		return fatal("Out of memory while parsing");
	}

	[[nodiscard]] AstExpr* parse_primary_expr() noexcept;
	[[nodiscard]] AstExpr* parse_postfix_expr() noexcept;
	[[nodiscard]] AstExpr* parse_unary_expr() noexcept;
	[[nodiscard]] AstExpr* parse_var_expr() noexcept;
	[[nodiscard]] AstExpr* parse_selector_expr() noexcept;
	[[nodiscard]] AstExpr* parse_agg_expr(AstExpr* type) noexcept;
	[[nodiscard]] AstExpr* parse_type_expr() noexcept;
	[[nodiscard]] AstExpr* parse_index_expr(AstExpr* operand) noexcept;
	[[nodiscard]] AstExpr* parse_call_expr(AstExpr* operand) noexcept;
	[[nodiscard]] AstExpr* parse_binop_rhs(int expr_prec, AstExpr* lhs) noexcept;
	[[nodiscard]] AstType* parse_bracket_type(Array<AstAttr*>&& attrs) noexcept;
	[[nodiscard]] AstType* parse_enum_type(Array<AstAttr*>&& attrs) noexcept;

	Token next() noexcept {
		m_last_token = m_this_token;
		if (m_peek_token) {
			auto token = *m_peek_token;
			m_peek_token.reset();
			m_this_token = token;
		} else {
			m_this_token = m_lexer.next();
		}
		return m_this_token;
	}
	Token peek() noexcept {
		// We can only peek LR(1)
		m_peek_token = next();
		return *m_peek_token;
	}

	template<typename T, typename... Ts>
	[[nodiscard]] T* new_node(Ts&&... args) noexcept {
		return m_ast.new_node<T>(forward<Ts>(args)...);
	}

	ArenaAllocator m_arena;
	Lexer& m_lexer;
	Token m_this_token;
	Token m_last_token;
	Maybe<Token> m_peek_token;
	Bool m_in_defer;
	Ast m_ast;
	Diagnostic& m_diagnostic;
};

} // namespace Biron

#endif // BIRON_PARSER_H