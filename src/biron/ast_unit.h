#ifndef BIRON_AST_UNIT_H
#define BIRON_AST_UNIT_H
#include <biron/ast.h>
#include <biron/util/string.h>

namespace Biron {

struct AstType;
struct AstTupleType;

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
	constexpr AstFn(StringView name, AstTupleType* selfs, AstTupleType* args, AstTupleType* rets, AstStmt* body, Maybe<Array<AstAttr*>>&& attrs, Range range) noexcept
		: AstNode{KIND, range}
		, m_name{name}
		, m_selfs{selfs}
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
	AstTupleType*          m_selfs;
	AstTupleType*          m_args;
	AstTupleType*          m_rets;
	AstStmt*               m_body;
	Maybe<Array<AstAttr*>> m_attrs;
};

struct AstTypedef : AstNode {
	static inline constexpr auto KIND = Kind::TYPE;
	constexpr AstTypedef(StringView name, AstType* type, Maybe<Array<AstAttr*>>&& attrs, Range range) noexcept
		: AstNode{KIND, range}
		, m_name{name}
		, m_type{type}
		, m_attrs{move(attrs)}
		, m_generated{false}
	{
	}
	[[nodiscard]] Bool codegen(Cg& cg) const noexcept;
	StringView name() const noexcept { return m_name; }
private:
	StringView             m_name;
	AstType*               m_type;
	Maybe<Array<AstAttr*>> m_attrs;
	mutable Bool           m_generated;
};

struct AstLetStmt;

struct AstUnit {
	constexpr AstUnit(Allocator& allocator) noexcept
		: m_module{nullptr}
		, m_fns{allocator}
		, m_lets{allocator}
		, m_typedefs{allocator}
		, m_imports{allocator}
	{
	}
	[[nodiscard]] Bool add_fn(AstFn* fn) noexcept {
		return m_fns.push_back(fn);
	}
	[[nodiscard]] Bool add_let(AstLetStmt* let) noexcept {
		return m_lets.push_back(let);
	}
	[[nodiscard]] Bool add_typedef(AstTypedef* type) noexcept {
		return m_typedefs.push_back(type);
	}
	[[nodiscard]] Bool add_import(AstImport* import) noexcept;
	[[nodiscard]] Bool assign_module(AstModule* module) noexcept;
	[[nodiscard]] Bool codegen(Cg& cg) const noexcept;
	void dump(StringBuilder& builder) const noexcept;
private:
	friend struct AstIdentType;
	AstModule*         m_module;
	Array<AstFn*>      m_fns;
	Array<AstLetStmt*> m_lets;
	Array<AstTypedef*> m_typedefs;
	Array<AstImport*>  m_imports;
};

} // namespace Biron

#endif // BIRON_AST_UNIT_H