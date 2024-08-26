#ifndef BIRON_AST_TYPE_H
#define BIRON_AST_TYPE_H
#include <biron/ast.h>

#include <biron/util/array.inl>
#include <biron/util/string.inl>

namespace Biron {

struct Cg;
struct CgType;

struct AstExpr;

struct AstType : AstNode {
	static inline constexpr auto KIND = Kind::TYPE;
	enum class Kind {
		TUPLE, IDENT, BOOL, VARARGS, PTR, ARRAY, SLICE, FN
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
private:
	Kind m_kind;
};

struct AstIdentType : AstType {
	static inline constexpr auto KIND = Kind::IDENT;
	constexpr AstIdentType(StringView ident, Range range) noexcept
		: AstType{KIND, range}
		, m_ident{ident}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	virtual CgType* codegen(Cg& cg) const noexcept override;
private:
	StringView m_ident;
};

struct AstBoolType : AstType {
	static inline constexpr auto KIND = Kind::BOOL;
	constexpr AstBoolType(Range range) noexcept
		: AstType{KIND, range}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	virtual CgType* codegen(Cg& cg) const noexcept override;
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
	constexpr AstTupleType(Array<Elem>&& elems, Range range) noexcept
		: AstType{KIND, range}
		, m_elems{move(elems)}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	virtual CgType* codegen(Cg& cg) const noexcept override;
	[[nodiscard]] constexpr const Array<Elem>& elems() const noexcept {
		return m_elems;
	}
private:
	Array<Elem> m_elems;
};

struct AstVarArgsType : AstType {
	static inline constexpr auto KIND = Kind::VARARGS;
	constexpr AstVarArgsType(Range range) noexcept
		: AstType{KIND, range}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	virtual CgType* codegen(Cg& cg) const noexcept override;
};

struct AstPtrType : AstType {
	static inline constexpr auto KIND = Kind::PTR;
	constexpr AstPtrType(AstType* type, Range range) noexcept
		: AstType{KIND, range}
		, m_type{type}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	virtual CgType* codegen(Cg& cg) const noexcept override;
private:
	AstType* m_type;
};

struct AstArrayType : AstType {
	static inline constexpr auto KIND = Kind::ARRAY;
	constexpr AstArrayType(AstType* type, AstExpr* extent, Range range) noexcept
		: AstType{KIND, range}
		, m_type{type}
		, m_extent{extent}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	virtual CgType* codegen(Cg& cg) const noexcept override;
private:
	AstType* m_type;
	AstExpr* m_extent;
};

struct AstSliceType : AstType {
	static inline constexpr auto KIND = Kind::SLICE;
	constexpr AstSliceType(AstType* type, Range range) noexcept
		: AstType{KIND, range}
		, m_type{type}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	virtual CgType* codegen(Cg& cg) const noexcept override;
private:
	AstType* m_type;
};

struct AstFnType : AstType {
	static inline constexpr auto KIND = Kind::FN;
	constexpr AstFnType(AstTupleType* args, AstTupleType* rets, Range range) noexcept
		: AstType{KIND, range}
		, m_args{args}
		, m_rets{rets}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	virtual CgType* codegen(Cg& cg) const noexcept override;
private:
	AstTupleType* m_args;
	AstTupleType* m_rets;
};

} // namespace Biron

#endif // BIRON_AST_TYPE_H