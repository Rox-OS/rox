#ifndef BIRON_AST_TYPE_H
#define BIRON_AST_TYPE_H
#include <biron/ast.h>

#include <biron/util/array.inl>
#include <biron/util/string.inl>

namespace Biron {

struct Cg;
struct CgType;

struct AstExpr;
struct AstAttr;

struct AstType : AstNode {
	static inline constexpr auto KIND = Kind::TYPE;
	enum class Kind {
		TUPLE, UNION, IDENT, BOOL, VARARGS, PTR, ARRAY, SLICE, FN
	};
	constexpr AstType(Kind kind, Range range) noexcept
		: AstNode{KIND, range}
		, m_kind{kind}
	{
	}
	virtual ~AstType() noexcept = default;
	virtual void dump(StringBuilder& builder) const noexcept = 0;
	template<DerivedFrom<AstType> T>
	[[nodiscard]] constexpr Bool is_type() const noexcept {
		return m_kind == T::KIND;
	}
	virtual CgType* codegen(Cg& cg) const noexcept = 0;
	virtual CgType* codegen_named(Cg& cg, StringView name) const noexcept;
private:
	Kind m_kind;
};

struct AstIdentType : AstType {
	static inline constexpr auto KIND = Kind::IDENT;
	constexpr AstIdentType(StringView ident, Maybe<Array<AstAttr*>>&& attrs, Range range) noexcept
		: AstType{KIND, range}
		, m_ident{ident}
		, m_attrs{move(attrs)}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	virtual CgType* codegen(Cg& cg) const noexcept override;
private:
	StringView             m_ident;
	Maybe<Array<AstAttr*>> m_attrs;
};

struct AstBoolType : AstType {
	static inline constexpr auto KIND = Kind::BOOL;
	constexpr AstBoolType(Maybe<Array<AstAttr*>>&& attrs, Range range) noexcept
		: AstType{KIND, range}
		, m_attrs{move(attrs)}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	virtual CgType* codegen(Cg& cg) const noexcept override;
private:
	Maybe<Array<AstAttr*>> m_attrs;
};

struct AstTupleType : AstType {
	struct Elem {
		constexpr Elem(Maybe<StringView>&& name, AstType* type) noexcept
			: m_name{move(name)}
			, m_type{type}
		{
		}
		Elem(Elem&& other) noexcept
			: m_name{move(other.m_name)}
			, m_type{exchange(other.m_type, nullptr)}
		{
		}
		[[nodiscard]] const Maybe<StringView>& name() const noexcept { return m_name; }
		[[nodiscard]] constexpr AstType* type() const noexcept { return m_type; }
	private:
		Maybe<StringView> m_name;
		AstType*          m_type;
	};

	static inline constexpr auto KIND = Kind::TUPLE;
	constexpr AstTupleType(Array<Elem>&& elems, Maybe<Array<AstAttr*>>&& attrs, Range range) noexcept
		: AstType{KIND, range}
		, m_elems{move(elems)}
		, m_attrs{move(attrs)}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	virtual CgType* codegen(Cg& cg) const noexcept override; 
	virtual CgType* codegen_named(Cg& cg, StringView name) const noexcept override; 
	[[nodiscard]] constexpr const Array<Elem>& elems() const noexcept {
		return m_elems;
	}
private:
	Array<Elem>            m_elems;
	Maybe<Array<AstAttr*>> m_attrs;
};

struct AstUnionType : AstType {
	static inline constexpr auto KIND = Kind::UNION;
	constexpr AstUnionType(Array<AstType*>&& types, Maybe<Array<AstAttr*>>&& attrs, Range range) noexcept
		: AstType{KIND, range}
		, m_types{move(types)}
		, m_attrs{move(attrs)}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	virtual CgType* codegen(Cg& cg) const noexcept override;
	[[nodiscard]] constexpr const Array<AstType*>& types() const noexcept {
		return m_types;
	}
private:
	Array<AstType*>        m_types;
	Maybe<Array<AstAttr*>> m_attrs;
};

struct AstVarArgsType : AstType {
	static inline constexpr auto KIND = Kind::VARARGS;
	constexpr AstVarArgsType(Maybe<Array<AstAttr*>>&& attrs, Range range) noexcept
		: AstType{KIND, range}
		, m_attrs{move(attrs)}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	virtual CgType* codegen(Cg& cg) const noexcept override;
private:
	Maybe<Array<AstAttr*>> m_attrs;
};

struct AstPtrType : AstType {
	static inline constexpr auto KIND = Kind::PTR;
	constexpr AstPtrType(AstType* type, Maybe<Array<AstAttr*>>&& attrs, Range range) noexcept
		: AstType{KIND, range}
		, m_type{type}
		, m_attrs{move(attrs)}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	virtual CgType* codegen(Cg& cg) const noexcept override;
private:
	AstType*               m_type;
	Maybe<Array<AstAttr*>> m_attrs;
};

struct AstArrayType : AstType {
	static inline constexpr auto KIND = Kind::ARRAY;
	constexpr AstArrayType(AstType* type, AstExpr* extent, Maybe<Array<AstAttr*>>&& attrs, Range range) noexcept
		: AstType{KIND, range}
		, m_type{type}
		, m_extent{extent}
		, m_attrs{move(attrs)}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	virtual CgType* codegen(Cg& cg) const noexcept override;
private:
	AstType*               m_type;
	AstExpr*               m_extent;
	Maybe<Array<AstAttr*>> m_attrs;
};

struct AstSliceType : AstType {
	static inline constexpr auto KIND = Kind::SLICE;
	constexpr AstSliceType(AstType* type, Maybe<Array<AstAttr*>>&& attrs, Range range) noexcept
		: AstType{KIND, range}
		, m_type{type}
		, m_attrs{move(attrs)}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	virtual CgType* codegen(Cg& cg) const noexcept override;
private:
	AstType*               m_type;
	Maybe<Array<AstAttr*>> m_attrs;
};

struct AstFnType : AstType {
	static inline constexpr auto KIND = Kind::FN;
	constexpr AstFnType(AstTupleType* args, AstTupleType* rets, Maybe<Array<AstAttr*>>&& attrs, Range range) noexcept
		: AstType{KIND, range}
		, m_args{args}
		, m_rets{rets}
		, m_attrs{move(attrs)}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	virtual CgType* codegen(Cg& cg) const noexcept override;
private:
	AstTupleType*          m_args;
	AstTupleType*          m_rets;
	Maybe<Array<AstAttr*>> m_attrs;
};

} // namespace Biron

#endif // BIRON_AST_TYPE_H