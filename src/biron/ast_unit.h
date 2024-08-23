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

struct AstFn : AstNode {
	static inline constexpr auto KIND = Kind::FN;
	constexpr AstFn(StringView name, AstTupleType* args, AstTupleType* rets, AstStmt* body, Maybe<Array<AstAttr*>>&& attrs, Range range) noexcept
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

struct AstStruct : AstNode {
	static inline constexpr auto KIND = Kind::STRUCT;
	struct Elem {
		StringView name;
		AstType*   type;
	};
	constexpr AstStruct(StringView name, Array<Elem>&& elems, Range range)
		: AstNode{KIND, range}
		, m_name{name}
		, m_elems{move(elems)}
	{
	}
	[[nodiscard]] Bool codegen(Cg& cg) const noexcept;
private:
	StringView m_name;
	Array<Elem> m_elems;
};

struct AstLetStmt;

struct AstUnit {
	constexpr AstUnit(Allocator& allocator) noexcept
		: m_fns{allocator}
		, m_structs{allocator}
		, m_lets{allocator}
	{
	}
	[[nodiscard]] Bool add_fn(AstFn* fn) noexcept {
		return m_fns.push_back(fn);
	}
	[[nodiscard]] Bool add_struct(AstStruct* record) noexcept {
		return m_structs.push_back(record);
	}
	[[nodiscard]] Bool add_let(AstLetStmt* let) noexcept {
		return m_lets.push_back(let);
	}
	[[nodiscard]] Bool codegen(Cg& cg) const noexcept;
private:
	Array<AstFn*> m_fns;
	Array<AstStruct*> m_structs;
	Array<AstLetStmt*> m_lets;
};

} // namespace Biron

#endif // BIRON_AST_UNIT_H