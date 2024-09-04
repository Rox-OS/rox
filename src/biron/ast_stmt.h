#ifndef BIRON_AST_STMT_H
#define BIRON_AST_STMT_H
#include <biron/ast.h>
#include <biron/util/string.h>
#include <biron/util/array.inl>

namespace Biron {

struct AstExpr;
struct AstAttr;

struct Cg;

struct AstStmt : AstNode {
	static inline constexpr auto KIND = Kind::STMT;
	enum class Kind {
		BLOCK, RETURN, DEFER, BREAK, CONTINUE, IF, LET, FOR, EXPR, ASSIGN, ASM
	};
	[[nodiscard]] const char *name() const noexcept;
	constexpr AstStmt(Kind kind, Range range) noexcept
		: AstNode{KIND, range}
		, m_kind{kind}
	{
	}
	virtual ~AstStmt() noexcept = default;
	virtual void dump(StringBuilder& builder, int depth) const noexcept = 0;
	template<DerivedFrom<AstStmt> T>
	[[nodiscard]] constexpr Bool is_stmt() const noexcept {
		return m_kind == T::KIND;
	}
	[[nodiscard]] virtual Bool codegen(Cg& cg) const noexcept;
private:
	Kind m_kind;
};

struct AstBlockStmt : AstStmt {
	static inline constexpr auto KIND = Kind::BLOCK;
	constexpr AstBlockStmt(Array<AstStmt*>&& stmts, Range range) noexcept
		: AstStmt{KIND, range}
		, m_stmts{move(stmts)}
	{
	}
	virtual void dump(StringBuilder& builder, int depth) const noexcept override;
	[[nodiscard]] virtual Bool codegen(Cg& cg) const noexcept override;
private:
	Array<AstStmt*> m_stmts;
};

struct AstReturnStmt : AstStmt {
	static inline constexpr auto KIND = Kind::RETURN;
	constexpr AstReturnStmt(AstExpr* expr, Range range) noexcept
		: AstStmt{KIND, range}
		, m_expr{expr}
	{
	}
	virtual void dump(StringBuilder& builder, int depth) const noexcept override;
	[[nodiscard]] virtual Bool codegen(Cg& cg) const noexcept override;
private:
	AstExpr* m_expr; // Optional
};

struct AstDeferStmt : AstStmt {
	static inline constexpr auto KIND = Kind::DEFER;
	constexpr AstDeferStmt(AstStmt* stmt, Range range) noexcept
		: AstStmt{KIND, range}
		, m_stmt{stmt}
	{
	}
	virtual void dump(StringBuilder& builder, int depth) const noexcept override;
	[[nodiscard]] virtual Bool codegen(Cg& cg) const noexcept override;
private:
	AstStmt* m_stmt;
};

struct AstBreakStmt : AstStmt {
	static inline constexpr auto KIND = Kind::BREAK;
	constexpr AstBreakStmt(Range range) noexcept
		: AstStmt{KIND, range}
	{
	}
	virtual void dump(StringBuilder& builder, int depth) const noexcept override;
	[[nodiscard]] virtual Bool codegen(Cg& cg) const noexcept override;
};

struct AstContinueStmt : AstStmt {
	static inline constexpr auto KIND = Kind::CONTINUE;
	constexpr AstContinueStmt(Range range) noexcept
		: AstStmt{KIND, range}
	{
	}
	virtual void dump(StringBuilder& builder, int depth) const noexcept override;
	[[nodiscard]] virtual Bool codegen(Cg& cg) const noexcept override;
};

struct AstLetStmt;

struct AstIfStmt : AstStmt {
	static inline constexpr auto KIND = Kind::IF;
	constexpr AstIfStmt(AstLetStmt* init, AstExpr* expr, AstBlockStmt* then, AstStmt* elif, Range range) noexcept 
		: AstStmt{KIND, range}
		, m_init{init}
		, m_expr{expr}
		, m_then{then}
		, m_elif{elif}
	{
	}
	virtual void dump(StringBuilder& builder, int depth) const noexcept override;
	[[nodiscard]] virtual Bool codegen(Cg& cg) const noexcept override;
private:
	AstLetStmt*   m_init;
	AstExpr*      m_expr;
	AstBlockStmt* m_then;
	AstStmt*      m_elif; // Either IfStmt or BlockStmt
};

struct AstLetStmt : AstStmt {
	static inline constexpr auto KIND = Kind::LET;
	constexpr AstLetStmt(StringView name, AstExpr* init, Maybe<Array<AstAttr*>>&& attrs, Range range) noexcept
		: AstStmt{KIND, range}
		, m_name{name}
		, m_init{init}
		, m_attrs{move(attrs)}
	{
	}
	virtual void dump(StringBuilder& builder, int depth) const noexcept override;
	[[nodiscard]] constexpr StringView name() const noexcept { return m_name; }
	[[nodiscard]] virtual Bool codegen(Cg& cg) const noexcept override;
	Bool codegen_global(Cg& cg) const noexcept;
private:
	StringView             m_name;
	AstExpr*               m_init;
	Maybe<Array<AstAttr*>> m_attrs;
};

struct AstForStmt : AstStmt {
	static inline constexpr auto KIND = Kind::FOR;
	constexpr AstForStmt(AstLetStmt* init, AstExpr* expr, AstStmt* post, AstBlockStmt* body, AstBlockStmt* elze, Range range) noexcept
		: AstStmt{KIND, range}
		, m_init{init}
		, m_expr{expr}
		, m_post{post}
		, m_body{body}
		, m_else{elze}
	{
	}
	virtual void dump(StringBuilder& builder, int depth) const noexcept override;
	[[nodiscard]] virtual Bool codegen(Cg& cg) const noexcept override;
private:
	AstLetStmt*   m_init;
	AstExpr*      m_expr;
	AstStmt*      m_post;
	AstBlockStmt* m_body;
	AstBlockStmt* m_else;
};

struct AstExprStmt : AstStmt {
	static inline constexpr auto KIND = Kind::EXPR;
	constexpr AstExprStmt(AstExpr* expr, Range range) noexcept
		: AstStmt{KIND, range}
		, m_expr{expr}
	{
	}
	virtual void dump(StringBuilder& builder, int depth) const noexcept override;
	[[nodiscard]] virtual Bool codegen(Cg& cg) const noexcept override;
private:
	AstExpr* m_expr;
};

struct AstAssignStmt : AstStmt {
	static inline constexpr auto KIND = Kind::ASSIGN;
	enum class StoreOp { WR, ADD, SUB, MUL, DIV };
	constexpr AstAssignStmt(AstExpr* dst, AstExpr* src, StoreOp op, Range range) noexcept
		: AstStmt{KIND, range}
		, m_dst{dst}
		, m_src{src}
		, m_op{op}
	{
	}
	virtual void dump(StringBuilder& builder, int depth) const noexcept override;
	[[nodiscard]] virtual Bool codegen(Cg& cg) const noexcept override;
private:
	AstExpr* m_dst;
	AstExpr* m_src;
	StoreOp  m_op;
};

} // namespace Biron

#endif // BIRON_AST_STMT_H