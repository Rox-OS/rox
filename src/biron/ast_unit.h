#ifndef BIRON_AST_UNIT_H
#define BIRON_AST_UNIT_H
#include <biron/ast.h>
#include <biron/util/string.h>

namespace Biron {

struct AstType;
struct AstTupleType;
struct AstArgsType;
struct AstIdentType;

struct AstStmt;
struct AstAttr;
struct Cg;

struct AstModule : AstNode {
	static inline constexpr const auto KIND = Kind::MODULE;
	constexpr AstModule(StringView name, Range range)
		: AstNode{KIND, range}
		, m_name{name}
	{
	}
	void dump(StringBuilder& builder) const noexcept;
	[[nodiscard]] Bool codegen(Cg& cg) const noexcept;
private:
	StringView m_name;
};

struct AstImport : AstNode {
	static inline constexpr const auto KIND = Kind::IMPORT;
	constexpr AstImport(StringView name, Range range)
		: AstNode{KIND, range}
		, m_name{name}
	{
	}
	[[nodiscard]] StringView name() const noexcept { return m_name; }
	[[nodiscard]] Bool codegen(Cg& cg) const noexcept;
private:
	StringView m_name;
};

struct AstFn : AstNode {
	static inline constexpr auto KIND = Kind::FN;
	constexpr AstFn(StringView name, AstArgsType* objs, AstArgsType* args, Array<AstIdentType*>&& effects, AstType* ret, AstStmt* body, Array<AstAttr*>&& attrs, Range range) noexcept
		: AstNode{KIND, range}
		, m_name{name}
		, m_objs{objs}
		, m_args{args}
		, m_effects{move(effects)}
		, m_ret{ret}
		, m_body{body}
		, m_attrs{move(attrs)}
	{
	}
	void dump(StringBuilder& builder, int depth) const noexcept;
	[[nodiscard]] Bool codegen(Cg& cg) const noexcept;
	[[nodiscard]] Bool prepass(Cg& cg) const noexcept;
	[[nodiscard]] constexpr StringView name() const noexcept { return m_name; }
	[[nodiscard]] constexpr const AstArgsType* args() const noexcept { return m_args; }
	[[nodiscard]] constexpr const AstType* ret() const noexcept { return m_ret; }
private:
	StringView           m_name;
	AstArgsType*         m_objs;
	AstArgsType*         m_args;
	Array<AstIdentType*> m_effects;
	AstType*             m_ret;
	AstStmt*             m_body;
	Array<AstAttr*>      m_attrs;
};

struct AstTypedef : AstNode {
	static inline constexpr auto KIND = Kind::TYPE;
	constexpr AstTypedef(StringView name, AstType* type, Array<AstAttr*>&& attrs, Range range) noexcept
		: AstNode{KIND, range}
		, m_name{name}
		, m_type{type}
		, m_attrs{move(attrs)}
		, m_generated{false}
	{
	}
	[[nodiscard]] Bool codegen(Cg& cg) const noexcept;
	constexpr StringView name() const noexcept { return m_name; }
private:
	StringView      m_name;
	AstType*        m_type;
	Array<AstAttr*> m_attrs;
	mutable Bool    m_generated;
};

struct AstEffect : AstNode {
	static inline constexpr auto KIND = Kind::EFFECT;
	constexpr AstEffect(StringView name, AstType* type, Range range) noexcept
		: AstNode{KIND, range}
		, m_name{name}
		, m_type{type}
		, m_generated{false}
	{
	}
	[[nodiscard]] Bool codegen(Cg& cg) const noexcept;
	[[nodiscard]] constexpr StringView name() const noexcept { return m_name; }
	[[nodiscard]] constexpr const AstType* type() const noexcept { return m_type; }
private:
	StringView   m_name;
	AstType*     m_type;
	mutable Bool m_generated;
};

} // namespace Biron

#endif // BIRON_AST_UNIT_H