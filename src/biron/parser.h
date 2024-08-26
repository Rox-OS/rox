#ifndef BIRON_PARSER_H
#define BIRON_PARSER_H
#include <biron/lexer.h>
#include <biron/pool.h>
#include <biron/ast_unit.h>
#include <biron/util/format.inl>

#include <biron/cg.h>

namespace Biron {

// Forward declarations
struct AstNode;

struct AstAttr;

struct AstExpr;
struct AstTupleExpr;
struct AstIntExpr;
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
	[[nodiscard]] Bool add_fn(AstTopFn* fn) noexcept;
	[[nodiscard]] Bool add_let(AstLetStmt* let) noexcept;
	[[nodiscard]] Scope* prev() const noexcept { return m_prev; }
private:
	Array<AstLetStmt*> m_lets;
	Array<AstTopFn*> m_fns;
	Scope* m_prev;
};

struct Parser {
	constexpr Parser(Lexer& lexer, Cg& cg) noexcept
		: m_lexer{lexer}
		, m_last_range{0, 0}
		, m_scope{nullptr}
		, m_caches{cg.allocator}
		, m_nodes{cg.allocator}
		, m_allocator{cg.allocator}
		, m_cg{cg}
	{
	}

	~Parser() noexcept;

	// Biron Expression
	[[nodiscard]] AstExpr*         parse_expr() noexcept;
	[[nodiscard]] AstTupleExpr*    parse_tuple_expr() noexcept;
	[[nodiscard]] AstIntExpr*      parse_int_expr() noexcept;
	[[nodiscard]] AstStrExpr*      parse_str_expr() noexcept;
	[[nodiscard]] AstBoolExpr*     parse_bool_expr() noexcept;

	// Types
	[[nodiscard]] AstType*         parse_type() noexcept;
	[[nodiscard]] AstTupleType*    parse_tuple_type() noexcept;
	[[nodiscard]] AstIdentType*    parse_ident_type() noexcept;
	[[nodiscard]] AstVarArgsType*  parse_varargs_type() noexcept;
	[[nodiscard]] AstPtrType*      parse_ptr_type() noexcept;
	[[nodiscard]] AstFnType*       parse_fn_type() noexcept;

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
	AstTopFn* parse_top_fn(Maybe<Array<AstAttr*>>&& attrs) noexcept;
	AstTopType* parse_top_type(Maybe<Array<AstAttr*>>&& attrs) noexcept;

	Maybe<AstUnit> parse() noexcept;

private:
	Bool has_symbol(StringView name) const noexcept;

	template<typename... Ts>
	void error(Range range, const char *message, Ts&&... args) {
		if constexpr (sizeof...(Ts) == 0) {
			diagnostic(range, message);
		} else if (auto fmt = format(m_allocator, message, forward<Ts>(args)...)) {
			diagnostic(range, fmt->data());
		} else {
			diagnostic(range, "Out of memory");
		}
	}
	template<typename... Ts>
	void error(const char *message, Ts&&... args) {
		error(m_this_token.range, message, forward<Ts>(args)...);
	}
	void diagnostic(Range range, const char *message);
	AstExpr* parse_primary_expr() noexcept;
	AstExpr* parse_postfix_expr() noexcept;
	AstExpr* parse_agg_expr(AstExpr* type) noexcept;
	AstExpr* parse_unary_expr() noexcept;
	AstExpr* parse_ident_expr() noexcept;
	AstExpr* parse_type_expr() noexcept;
	AstExpr* parse_index_expr(AstExpr* operand) noexcept;
	AstExpr* parse_binop_rhs(int expr_prec, AstExpr* lhs) noexcept;
	AstType* parse_bracket_type() noexcept;
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
			if (auto addr = cache.allocate()) {
				auto node = new (addr, Nat{}) T{forward<Ts>(args)...};
				(void)m_nodes.push_back(node);
				return node;
			} else {
				return nullptr;
			}
		}
		if (!m_caches.emplace_back(m_allocator, sizeof(T), Ulen(1024))) {
			return nullptr;
		}
		return new_node<T>(forward<Ts>(args)...);
	}

	Lexer& m_lexer;
	Token m_this_token;
	Token m_last_token;
	Range m_last_range;
	Maybe<Token> m_peek_token;
	Scope* m_scope;
	Array<Cache> m_caches;
	Array<AstNode*> m_nodes;
	Allocator& m_allocator;
	Cg& m_cg;
};

} // namespace Biron

#endif // BIRON_PARSER_H