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
struct AstBoolType;
struct AstTupleType;
struct AstIdentType;
struct AstVarArgsType;
struct AstPtrType;
struct AstFnType;

struct AstStmt;
struct AstBlockStmt;
struct AstReturnStmt;
struct AstDeferStmt;
struct AstBreakStmt;
struct AstContinueStmt;
struct AstIfStmt;
struct AstLetStmt;
struct AstForStmt;
struct AstExprStmt;
struct AstAssignStmt;
struct AstAsmStmt;

struct Scope {
	constexpr Scope(Allocator& allocator, Scope* prev = nullptr) noexcept
		: m_lets{allocator}
		, m_fns{allocator}
		, m_prev{prev}
	{
	}
	[[nodiscard]] Bool find(StringView name) const noexcept;
	[[nodiscard]] Bool add_fn(AstFn* fn) noexcept;
	[[nodiscard]] Bool add_let(AstLetStmt* let) noexcept;
	[[nodiscard]] Scope* prev() const noexcept { return m_prev; }
private:
	Array<AstLetStmt*> m_lets;
	Array<AstFn*>      m_fns;
	Scope*             m_prev;
};

struct Parser {
	constexpr Parser(Lexer& lexer, Diagnostic& diagnostic, Allocator& allocator) noexcept
		: m_lexer{lexer}
		, m_scope{nullptr}
		, m_in_defer{false}
		, m_caches{allocator}
		, m_diagnostic{diagnostic}
		, m_allocator{allocator}
	{
	}

	~Parser() noexcept;

	// Biron Expression
	[[nodiscard]] AstExpr*         parse_expr(Bool simple) noexcept;
	[[nodiscard]] AstTupleExpr*    parse_tuple_expr() noexcept;
	[[nodiscard]] AstIntExpr*      parse_int_expr() noexcept;
	[[nodiscard]] AstFltExpr*      parse_flt_expr() noexcept;
	[[nodiscard]] AstStrExpr*      parse_str_expr() noexcept;
	[[nodiscard]] AstBoolExpr*     parse_bool_expr() noexcept;

	// Types
	[[nodiscard]] AstType*         parse_type() noexcept;
	[[nodiscard]] AstTupleType*    parse_tuple_type(Maybe<Array<AstAttr*>>&& attrs) noexcept;
	[[nodiscard]] AstIdentType*    parse_ident_type(Maybe<Array<AstAttr*>>&& attrs) noexcept;
	[[nodiscard]] AstVarArgsType*  parse_varargs_type(Maybe<Array<AstAttr*>>&& attrs) noexcept;
	[[nodiscard]] AstPtrType*      parse_ptr_type(Maybe<Array<AstAttr*>>&& attrs) noexcept;
	[[nodiscard]] AstFnType*       parse_fn_type(Maybe<Array<AstAttr*>>&& attrs) noexcept;

	// Statements
	[[nodiscard]] AstStmt*         parse_stmt() noexcept;
	[[nodiscard]] AstBlockStmt*    parse_block_stmt() noexcept;
	[[nodiscard]] AstReturnStmt*   parse_return_stmt() noexcept;
	[[nodiscard]] AstDeferStmt*    parse_defer_stmt() noexcept;
	[[nodiscard]] AstBreakStmt*    parse_break_stmt() noexcept;
	[[nodiscard]] AstContinueStmt* parse_continue_stmt() noexcept;
	[[nodiscard]] AstIfStmt*       parse_if_stmt() noexcept;
	[[nodiscard]] AstLetStmt*      parse_let_stmt(Maybe<Array<AstAttr*>>&& attrs) noexcept;
	[[nodiscard]] AstForStmt*      parse_for_stmt() noexcept;
	[[nodiscard]] AstStmt*         parse_expr_stmt(Bool semi) noexcept;

	// Attributes
	[[nodiscard]] Maybe<Array<AstAttr*>> parse_attrs() noexcept;

	// Top-level elements
	[[nodiscard]] AstFn*           parse_fn(Maybe<Array<AstAttr*>>&& attrs) noexcept;
	[[nodiscard]] AstTypedef*      parse_typedef(Maybe<Array<AstAttr*>>&& attrs) noexcept;
	[[nodiscard]] AstModule*       parse_module() noexcept;
	[[nodiscard]] AstImport*       parse_import() noexcept;

	Maybe<AstUnit> parse() noexcept;

private:
	Bool has_symbol(StringView name) const noexcept;

	template<typename... Ts>
	void error(Range range, const char* message, Ts&&... args) noexcept {
		m_diagnostic.error(range, message, forward<Ts>(args)...);
	}
	template<typename... Ts>
	void error(const char *message, Ts&&... args) noexcept {
		error(m_this_token.range, message, forward<Ts>(args)...);
	}

	[[nodiscard]] AstExpr* parse_primary_expr(Bool simple) noexcept;
	[[nodiscard]] AstExpr* parse_postfix_expr(Bool simple) noexcept;
	[[nodiscard]] AstExpr* parse_unary_expr(Bool simple) noexcept;
	[[nodiscard]] AstExpr* parse_ident_expr(Bool simple) noexcept;
	[[nodiscard]] AstExpr* parse_agg_expr(AstExpr* type) noexcept;
	[[nodiscard]] AstExpr* parse_type_expr() noexcept;
	[[nodiscard]] AstExpr* parse_index_expr(AstExpr* operand) noexcept;
	[[nodiscard]] AstExpr* parse_binop_rhs(Bool simple, int expr_prec, AstExpr* lhs) noexcept;
	[[nodiscard]] AstType* parse_bracket_type(Maybe<Array<AstAttr*>>&& attrs) noexcept;
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
		for (auto& cache : m_caches) {
			// Search for maching cache size
			if (cache.object_size() != sizeof(T)) {
				continue;
			}
			return cache.make<T>(forward<Ts>(args)...);
		}
		if (!m_caches.emplace_back(m_allocator, sizeof(T), Ulen(1024))) {
			return nullptr;
		}
		return new_node<T>(forward<Ts>(args)...);
	}

	Lexer& m_lexer;
	Token m_this_token;
	Token m_last_token;
	Maybe<Token> m_peek_token;
	Scope* m_scope;
	Bool m_in_defer;
	Array<Cache> m_caches;
	Diagnostic& m_diagnostic;
	Allocator& m_allocator;
};

} // namespace Biron

#endif // BIRON_PARSER_H