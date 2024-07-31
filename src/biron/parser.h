#ifndef BIRON_PARSER_H
#define BIRON_PARSER_H
#include <biron/lexer.h>
#include <biron/codegen.h>

#include <biron/util/array.inl>
#include <biron/util/string.inl>
#include <biron/util/format.inl>

namespace Biron {

// Forward declarations
struct AstNode;

struct AstExpr;
struct AstTupleExpr;
struct AstIntExpr;
struct AstStrExpr;

struct AstAsmExpr;
struct AstAsmRegExpr;
struct AstAsmImmExpr;
struct AstAsmMemExpr;
struct AstAsmSubExpr; 

struct AstType;
struct AstTupleType;
struct AstIdentType;
struct AstVarArgsType;
struct AstPtrType;

struct AstStmt;
struct AstBlockStmt;
struct AstReturnStmt;
struct AstDeferStmt;
struct AstIfStmt;
struct AstLetStmt;
struct AstForStmt;
struct AstExprStmt;
struct AstAssignStmt;
struct AstAsmStmt;

struct AstFn;
struct AstAsm;

struct Parser {
	constexpr Parser(StringView name, StringView data, Allocator& allocator) noexcept
		: m_lexer{name, data}
		, m_nodes{allocator}
		, m_allocator{allocator}
	{
	}

	~Parser() noexcept;

	// Biron Expression
	[[nodiscard]] AstExpr*        parse_expr() noexcept;
	[[nodiscard]] AstTupleExpr*   parse_tuple_expr() noexcept;
	[[nodiscard]] AstIntExpr*     parse_int_expr() noexcept;
	[[nodiscard]] AstStrExpr*     parse_str_expr() noexcept;

	// Asm Expression
	[[nodiscard]] AstAsmExpr*     parse_asm_expr() noexcept;
	[[nodiscard]] AstAsmRegExpr*  parse_asm_reg_expr() noexcept;
	[[nodiscard]] AstAsmImmExpr*  parse_asm_imm_expr() noexcept;
	[[nodiscard]] AstAsmMemExpr*  parse_asm_mem_expr() noexcept;
	[[nodiscard]] AstAsmSubExpr*  parse_asm_sub_expr() noexcept;

	// Types
	[[nodiscard]] AstType*        parse_type() noexcept;
	[[nodiscard]] AstTupleType*   parse_tuple_type() noexcept;
	[[nodiscard]] AstIdentType*   parse_ident_type() noexcept;
	[[nodiscard]] AstVarArgsType* parse_varargs_type() noexcept;
	[[nodiscard]] AstPtrType*     parse_ptr_type() noexcept;

	// Statements
	[[nodiscard]] AstStmt*        parse_stmt() noexcept;
	[[nodiscard]] AstBlockStmt*   parse_block_stmt() noexcept;
	[[nodiscard]] AstReturnStmt*  parse_return_stmt() noexcept;
	[[nodiscard]] AstDeferStmt*   parse_defer_stmt() noexcept;
	[[nodiscard]] AstIfStmt*      parse_if_stmt() noexcept;
	[[nodiscard]] AstLetStmt*     parse_let_stmt() noexcept;
	[[nodiscard]] AstForStmt*     parse_for_stmt() noexcept;
	[[nodiscard]] AstStmt*        parse_expr_stmt() noexcept;
	[[nodiscard]] AstAsmStmt*     parse_asm_stmt() noexcept;

	// Top-level elements
	AstFn*  parse_fn() noexcept;
	AstAsm* parse_asm() noexcept;

	Maybe<Unit> parse() noexcept;

private:
	template<typename... Ts>
	void error(Token token, const char *message, Ts&&... args) {
		if constexpr (sizeof...(Ts) == 0) {
			diagnostic(token, message);
		} else if (auto fmt = format(m_allocator, message, forward<Ts>(args)...)) {
			diagnostic(token, fmt->data());
		} else {
			diagnostic(token, "Out of memory");
		}
	}
	template<typename... Ts>
	void error(const char *message, Ts&&... args) {
		error(m_this_token, message, forward<Ts>(args)...);
	}
	void diagnostic(Token where, const char *message);
	AstExpr* parse_primary_expr() noexcept;
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
		BIRON_ASSERT(!m_peek_token && "LR(1) violation");
		m_peek_token = next();
		return *m_peek_token;
	}

	template<typename T, typename... Ts>
	[[nodiscard]] T* new_node(Ts&&... args) noexcept {
		Ulen offset = m_nodes.length();
		if (!m_nodes.reserve(offset + 1)) {
			return nullptr;
		}
		if (auto addr = m_allocator.allocate(sizeof(T))) {
			auto node = new(addr, Nat{}) T{forward<Ts>(args)...};
			(void)m_nodes.push_back(node); // Cannot fail
			return node;
		}
		return nullptr;
	}

	Lexer m_lexer;
	Token m_this_token;
	Token m_last_token;
	Maybe<Token> m_peek_token;
	Array<AstNode*> m_nodes;
	Allocator& m_allocator;
};

} // namespace Biron

#endif // BIRON_PARSER_H