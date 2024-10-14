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
		BLOCK,    // '{' <Stmt>* '}'
		RETURN,   // 'return' <ExprStmt>?
		DEFER,    // 'defer' <Stmt>
		BREAK,    // 'break' ';'
		CONTINUE, // 'continue' ';'
		IF,       // 'if' <LLetStmt>? <Expr> <BlockStmt> ('else' <BlockStmt>)?
		LLET,     // 'let' <Ident> '=' <Expr> ';'
		GLET,     // 'let' <Ident> '=' <Expr> ';'
		USING,    // 'using' <Ident> '=' <Expr> ';'
		FOR,      // 'for' <LLetStmt>? <ExprStmt> <Expr>? <BlockStmt> ('else' <BlockStmt>)?
		EXPR,     // <Expr> ';'
		ASSIGN    // <Expr> '=' <Expr> ';'
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
	template<DerivedFrom<AstStmt> T>
	[[nodiscard]] constexpr const T* to_stmt() const noexcept {
		return is_stmt<T>() ? static_cast<const T*>(this) : nullptr;
	}
	template<DerivedFrom<AstStmt> T>
	[[nodiscard]] constexpr T* to_stmt() noexcept {
		return is_stmt<T>() ? static_cast<T*>(this) : nullptr;
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

struct AstLLetStmt;

struct AstIfStmt : AstStmt {
	static inline constexpr auto KIND = Kind::IF;
	constexpr AstIfStmt(AstLLetStmt* init, AstExpr* expr, AstBlockStmt* then, AstStmt* elif, Range range) noexcept 
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
	AstLLetStmt*  m_init;
	AstExpr*      m_expr;
	AstBlockStmt* m_then;
	AstStmt*      m_elif; // Either IfStmt or BlockStmt
};

struct AstLLetStmt : AstStmt {
	static inline constexpr auto KIND = Kind::LLET;
	constexpr AstLLetStmt(StringView name, AstExpr* init, Array<AstAttr*>&& attrs, Range range) noexcept
		: AstStmt{KIND, range}
		, m_name{name}
		, m_init{init}
		, m_attrs{move(attrs)}
	{
	}
	virtual void dump(StringBuilder& builder, int depth) const noexcept override;
	[[nodiscard]] constexpr StringView name() const noexcept { return m_name; }
	[[nodiscard]] virtual Bool codegen(Cg& cg) const noexcept override;
private:
	StringView      m_name;
	AstExpr*        m_init;
	Array<AstAttr*> m_attrs;
};

struct AstGLetStmt : AstStmt {
	static inline constexpr auto KIND = Kind::GLET;
	constexpr AstGLetStmt(StringView name, AstExpr* init, Array<AstAttr*>&& attrs, Range range) noexcept
		: AstStmt{KIND, range}
		, m_name{name}
		, m_init{init}
		, m_attrs{move(attrs)}
	{
	}
	virtual void dump(StringBuilder& builder, int depth) const noexcept override;
	[[nodiscard]] constexpr StringView name() const noexcept { return m_name; }
	[[nodiscard]] virtual Bool codegen(Cg& cg) const noexcept override;
private:
	StringView      m_name;
	AstExpr*        m_init;
	Array<AstAttr*> m_attrs;
};

struct AstUsingStmt : AstStmt {
	static inline constexpr auto KIND = Kind::USING;
	constexpr AstUsingStmt(StringView name, AstExpr* init, Range range)
		: AstStmt{KIND, range}
		, m_name{name}
		, m_init{init}
	{
	}
	virtual void dump(StringBuilder& builder, int depth) const noexcept override;
	[[nodiscard]] constexpr StringView name() const noexcept { return m_name; }
	[[nodiscard]] virtual Bool codegen(Cg& cg) const noexcept override;
private:
	StringView m_name;
	AstExpr*   m_init;
};

struct AstForStmt : AstStmt {
	static inline constexpr auto KIND = Kind::FOR;
	constexpr AstForStmt(AstLLetStmt* init, AstExpr* expr, AstStmt* post, AstBlockStmt* body, AstBlockStmt* elze, Range range) noexcept
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
	AstLLetStmt*  m_init;
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