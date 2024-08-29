#ifndef BIRON_AST_UNIT_H
#define BIRON_AST_UNIT_H
#include <biron/ast.h>

#include <biron/util/string.inl>

namespace Biron {

struct AstType;
struct AstTupleType;

struct AstStmt;
struct AstAttr;
struct Cg;

struct AstTopFn : AstNode {
	static inline constexpr auto KIND = Kind::FN;
	constexpr AstTopFn(StringView name, AstTupleType* args, AstTupleType* rets, AstStmt* body, Maybe<Array<AstAttr*>>&& attrs, Range range) noexcept
		: AstNode{KIND, range}
		, m_name{name}
		, m_args{args}
		, m_rets{rets}
		, m_body{body}
		, m_attrs{move(attrs)}
	{
	}
	void dump(StringBuilder& builder, int depth) const noexcept;
	[[nodiscard]] Bool codegen(Cg& cg) const noexcept;
	[[nodiscard]] Bool prepass(Cg& cg) const noexcept;
	[[nodiscard]] constexpr StringView name() const noexcept { return m_name; }
	[[nodiscard]] constexpr const AstTupleType* args() const noexcept { return m_args; }
private:
	StringView             m_name;
	AstTupleType*          m_args;
	AstTupleType*          m_rets;
	AstStmt*               m_body;
	Maybe<Array<AstAttr*>> m_attrs;
};

struct AstTopType : AstNode {
	static inline constexpr auto KIND = Kind::TYPE;
	constexpr AstTopType(StringView name, AstType* type, Maybe<Array<AstAttr*>>&& attrs, Range range) noexcept
		: AstNode{KIND, range}
		, m_name{name}
		, m_type{type}
		, m_attrs{move(attrs)}
	{
	}
	[[nodiscard]] Bool codegen(Cg& cg) const noexcept;
private:
	StringView             m_name;
	AstType*               m_type;
	Maybe<Array<AstAttr*>> m_attrs;
};

struct AstLetStmt;

struct AstUnit {
	constexpr AstUnit(Allocator& allocator) noexcept
		: m_fns{allocator}
		, m_types{allocator}
		, m_lets{allocator}
	{
	}
	[[nodiscard]] Bool add_fn(AstTopFn* fn) noexcept {
		return m_fns.push_back(fn);
	}
	[[nodiscard]] Bool add_let(AstLetStmt* let) noexcept {
		return m_lets.push_back(let);
	}
	[[nodiscard]] Bool add_typedef(AstTopType* type) noexcept {
		return m_types.push_back(type);
	}
	[[nodiscard]] Bool codegen(Cg& cg) const noexcept;
	[[nodiscard]] Bool dump() const noexcept;
private:
	Array<AstTopFn*> m_fns;
	Array<AstTopType*> m_types;
	Array<AstLetStmt*> m_lets;
};

} // namespace Biron

#endif // BIRON_AST_UNIT_H