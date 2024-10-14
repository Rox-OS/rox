#ifndef BIRON_AST_TYPE_H
#define BIRON_AST_TYPE_H
#include <biron/ast.h>
#include <biron/util/string.h>

#include <biron/util/array.inl>

namespace Biron {

struct Cg;
struct CgType;

struct AstExpr;
struct AstAttr;

using AttrArray = Array<AstAttr*>;

struct AstType : AstNode {
	static inline constexpr auto const KIND = Kind::TYPE;
	enum class Kind : Uint8 {
		TUPLE,   // '{' <Type> (',' <Type>)* '}'
		ARGS,    // '(' <Type> (',' <Type>)* ')' -- Special compiler metatype only
		GROUP,   // '(' <Type> ')'
		UNION,   // <Type> ('|' <Type>)+
		ENUM,    // '[' ('.' <Ident> ('=' <Expr>)?)+ ']'
		PTR,     // '*' <Type>
		ATOM,    // '@' <Type>
		ARRAY,   // '[' <Expr> ']' <Type>
		SLICE,   // '[]' <Type>
		IDENT,   // <Ident>
		VARARGS, // '...'
		FN,      // 'fn' <ArgsType>? <ArgsType> <Effects>? ('->' <Type>)?
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
	template<DerivedFrom<AstType> T>
	[[nodiscard]] constexpr T* to_type() noexcept {
		return is_type<T>() ? static_cast<T*>(this) : nullptr;
	}
	template<DerivedFrom<AstType> T>
	[[nodiscard]] constexpr const T* to_type() const noexcept {
		return is_type<T>() ? static_cast<const T*>(this) : nullptr;
	}
	virtual CgType* codegen(Cg& cg, Maybe<StringView> name) const noexcept = 0;
private:
	Kind m_kind;
};

struct AstIdentType : AstType {
	static inline constexpr auto const KIND = Kind::IDENT;
	constexpr AstIdentType(StringView ident, Array<AstAttr*>&& attrs, Range range) noexcept
		: AstType{KIND, range}
		, m_ident{ident}
		, m_attrs{move(attrs)}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	virtual CgType* codegen(Cg& cg, Maybe<StringView> name) const noexcept override;
	[[nodiscard]] constexpr StringView name() const noexcept { return m_ident; }
private:
	StringView      m_ident;
	Array<AstAttr*> m_attrs;
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

	static inline constexpr auto const KIND = Kind::TUPLE;
	constexpr AstTupleType(Array<Elem>&& elems, Array<AstAttr*>&& attrs, Range range) noexcept
		: AstType{KIND, range}
		, m_elems{move(elems)}
		, m_attrs{move(attrs)}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	virtual CgType* codegen(Cg& cg, Maybe<StringView> name) const noexcept override; 
	[[nodiscard]] constexpr const Array<Elem>& elems() const noexcept {
		return m_elems;
	}
private:
	Array<Elem>     m_elems;
	Array<AstAttr*> m_attrs;
};

struct AstArgsType : AstType {
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

	static inline constexpr auto const KIND = Kind::ARGS;
	constexpr AstArgsType(Array<Elem>&& elems, Range range) noexcept
		: AstType{KIND, range}
		, m_elems{move(elems)}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	virtual CgType* codegen(Cg& cg, Maybe<StringView> name) const noexcept override; 
	[[nodiscard]] constexpr const Array<Elem>& elems() const noexcept {
		return m_elems;
	}
private:
	Array<Elem> m_elems;
};

struct AstGroupType : AstType {
	static inline constexpr const auto KIND = Kind::GROUP;
	constexpr AstGroupType(AstType* type, Range range) noexcept
		: AstType{KIND, range}
		, m_type{type}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	virtual CgType* codegen(Cg& cg, Maybe<StringView> name) const noexcept override;
	[[nodiscard]] constexpr const AstType* type() const noexcept {
		return m_type;
	}
private:
	AstType* m_type;
};

struct AstUnionType : AstType {
	static inline constexpr auto const KIND = Kind::UNION;
	constexpr AstUnionType(Array<AstType*>&& types, Array<AstAttr*>&& attrs, Range range) noexcept
		: AstType{KIND, range}
		, m_types{move(types)}
		, m_attrs{move(attrs)}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	virtual CgType* codegen(Cg& cg, Maybe<StringView> name) const noexcept override;
	[[nodiscard]] constexpr const Array<AstType*>& types() const noexcept {
		return m_types;
	}
private:
	Array<AstType*> m_types;
	Array<AstAttr*> m_attrs;
};

struct AstVarArgsType : AstType {
	static inline constexpr auto const KIND = Kind::VARARGS;
	constexpr AstVarArgsType(Array<AstAttr*>&& attrs, Range range) noexcept
		: AstType{KIND, range}
		, m_attrs{move(attrs)}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	virtual CgType* codegen(Cg& cg, Maybe<StringView> name) const noexcept override;
private:
	Array<AstAttr*> m_attrs;
};

struct AstPtrType : AstType {
	static inline constexpr auto const KIND = Kind::PTR;
	constexpr AstPtrType(AstType* type, Array<AstAttr*>&& attrs, Range range) noexcept
		: AstType{KIND, range}
		, m_type{type}
		, m_attrs{move(attrs)}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	virtual CgType* codegen(Cg& cg, Maybe<StringView> name) const noexcept override;
private:
	AstType*        m_type;
	Array<AstAttr*> m_attrs;
};

struct AstArrayType : AstType {
	static inline constexpr auto const KIND = Kind::ARRAY;
	constexpr AstArrayType(AstType* base, AstExpr* extent, Array<AstAttr*>&& attrs, Range range) noexcept
		: AstType{KIND, range}
		, m_base{base}
		, m_extent{extent}
		, m_attrs{move(attrs)}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	virtual CgType* codegen(Cg& cg, Maybe<StringView> name) const noexcept override;
	[[nodiscard]] AstType* base() const noexcept { return m_base; }
	[[nodiscard]] AstExpr* extent() const noexcept { return m_extent; }
private:
	AstType*        m_base;
	AstExpr*        m_extent;
	Array<AstAttr*> m_attrs;
};

struct AstSliceType : AstType {
	static inline constexpr auto const KIND = Kind::SLICE;
	constexpr AstSliceType(AstType* type, Array<AstAttr*>&& attrs, Range range) noexcept
		: AstType{KIND, range}
		, m_type{type}
		, m_attrs{move(attrs)}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	virtual CgType* codegen(Cg& cg, Maybe<StringView> name) const noexcept override;
private:
	AstType*        m_type;
	Array<AstAttr*> m_attrs;
};

struct AstFnType : AstType {
	static inline constexpr auto const KIND = Kind::FN;
	constexpr AstFnType(AstArgsType* objs, AstArgsType* args, Array<AstIdentType*>&& effects, AstType* ret, Array<AstAttr*>&& attrs, Range range) noexcept
		: AstType{KIND, range}
		, m_objs{objs}
		, m_args{args}
		, m_effects{move(effects)}
		, m_ret{ret}
		, m_attrs{move(attrs)}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	virtual CgType* codegen(Cg& cg, Maybe<StringView> name) const noexcept override;
private:
	AstArgsType*         m_objs;
	AstArgsType*         m_args;
	Array<AstIdentType*> m_effects;
	AstType*             m_ret;
	Array<AstAttr*>      m_attrs;
};

struct AstAtomType : AstType {
	static inline constexpr const auto KIND = Kind::ATOM;
	constexpr AstAtomType(AstType* base, Array<AstAttr*>&& attrs, Range range) noexcept
		: AstType{KIND, range}
		, m_base{base}
		, m_attrs{move(attrs)}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	virtual CgType* codegen(Cg& cg, Maybe<StringView> name) const noexcept override;
private:
	AstType*        m_base;
	Array<AstAttr*> m_attrs;
};

struct AstEnumType : AstType {
	static inline constexpr const auto KIND = Kind::ATOM;
	struct Enumerator {
		constexpr Enumerator(StringView name, AstExpr* init) noexcept
			: name{name}
			, init{init}
		{
		}
		constexpr Enumerator(Enumerator&&) = default;
		StringView name;
		AstExpr*   init;
	};

	constexpr AstEnumType(Array<Enumerator>&& enums, Array<AstAttr*>&& attrs, Range range) noexcept
		: AstType{KIND, range}
		, m_enums{move(enums)}
		, m_attrs{move(attrs)}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	virtual CgType* codegen(Cg& cg, Maybe<StringView> name) const noexcept override;
private:
	Array<Enumerator> m_enums;
	Array<AstAttr*>   m_attrs;
};

} // namespace Biron

#endif // BIRON_AST_TYPE_H