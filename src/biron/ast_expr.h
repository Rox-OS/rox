#ifndef BIRON_AST_EXPR_H
#define BIRON_AST_EXPR_H
#include <biron/ast.h>
#include <biron/util/array.inl>
#include <biron/util/string.inl>

namespace Biron {

struct Cg;
struct CgValue;
struct CgAddr;

struct AstType;
struct AstConst;

struct AstExpr : AstNode {
	static inline constexpr auto KIND = Kind::EXPR;
	enum class Kind : Uint8 { TUPLE, CALL, TYPE, VAR, INT, BOOL, STR, AGG, BIN, UNARY, INDEX, EXPLODE };
	[[nodiscard]] const char *name() const noexcept;
	constexpr AstExpr(Kind kind, Range range) noexcept
		: AstNode{KIND, range}
		, m_kind{kind}
	{
	}
	virtual ~AstExpr() noexcept = default;
	virtual void dump(StringBuilder& builder) const noexcept = 0;
	template<DerivedFrom<AstExpr> T>
	[[nodiscard]] constexpr Bool is_expr() const noexcept {
		return m_kind == T::KIND;
	}
	[[nodiscard]] virtual Maybe<AstConst> eval(Cg&) const noexcept;
	[[nodiscard]] virtual Maybe<CgAddr> gen_addr(Cg& cg) const noexcept;
	[[nodiscard]] virtual Maybe<CgValue> gen_value(Cg& cg) const noexcept;
private:
	Kind m_kind;
};

struct AstTupleExpr : AstExpr {
	static inline constexpr auto KIND = Kind::TUPLE;
	constexpr AstTupleExpr(Array<AstExpr*>&& exprs, Range range) noexcept
		: AstExpr{KIND, range}
		, m_exprs{move(exprs)}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	[[nodiscard]] Ulen length() const noexcept { return m_exprs.length(); }
	[[nodiscard]] virtual Maybe<AstConst> eval(Cg&) const noexcept override;
	[[nodiscard]] virtual Maybe<CgAddr> gen_addr(Cg& cg) const noexcept override;
	[[nodiscard]] virtual Maybe<CgValue> gen_value(Cg& cg) const noexcept override;
	AstExpr* at(Ulen i) const noexcept {
		BIRON_ASSERT(i < m_exprs.length() && "Out of bounds");
		return m_exprs[i];
	}
private:
	Array<AstExpr*> m_exprs;
};

struct AstCallExpr : AstExpr {
	static inline constexpr auto KIND = Kind::CALL;
	constexpr AstCallExpr(AstExpr* callee, AstTupleExpr* args, Bool c, Range range) noexcept
		: AstExpr{KIND, range}
		, m_callee{callee}
		, m_args{args}
		, m_c{c}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	[[nodiscard]] Maybe<CgValue> gen_value(Cg& cg) const noexcept override;
private:
	AstExpr*      m_callee;
	AstTupleExpr* m_args;
	Bool          m_c; // C ABI
};

struct AstTypeExpr : AstExpr {
	static inline constexpr const auto KIND = Kind::TYPE;
	constexpr AstTypeExpr(AstType* type, Range range) noexcept
		: AstExpr{KIND, range}
		, m_type{type}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	AstType* type() const noexcept { return m_type; }
private:
	AstType* m_type;
};

struct AstVarExpr : AstExpr {
	static inline constexpr const auto KIND = Kind::VAR;
	constexpr AstVarExpr(StringView name, Range range) noexcept
		: AstExpr{KIND, range}
		, m_name{name}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	[[nodiscard]] Maybe<CgAddr> gen_addr(Cg& cg) const noexcept override;
	[[nodiscard]] Maybe<CgValue> gen_value(Cg& cg) const noexcept override;
	[[nodiscard]] StringView name() const noexcept { return m_name; }
private:
	StringView m_name;
};

struct AstIntExpr : AstExpr {
	static inline constexpr const auto KIND = Kind::INT;
	enum class Kind {
		U8, U16, U32, U64,
		S8, S16, S32, S64
	};
	constexpr AstIntExpr(Uint8 value, Range range) noexcept
		: AstExpr{KIND, range}, m_kind{Kind::U8}, m_as_u8{value} {}
	constexpr AstIntExpr(Uint16 value, Range range) noexcept
		: AstExpr{KIND, range}, m_kind{Kind::U16}, m_as_u16{value} {}
	constexpr AstIntExpr(Uint32 value, Range range) noexcept
		: AstExpr{KIND, range}, m_kind{Kind::U32}, m_as_u32{value} {}
	constexpr AstIntExpr(Uint64 value, Range range) noexcept
		: AstExpr{KIND, range}, m_kind{Kind::U64}, m_as_u64{value} {}
	constexpr AstIntExpr(Sint8 value, Range range) noexcept
		: AstExpr{KIND, range}, m_kind{Kind::S8}, m_as_s8{value} {}
	constexpr AstIntExpr(Sint16 value, Range range) noexcept
		: AstExpr{KIND, range}, m_kind{Kind::S16}, m_as_s16{value} {}
	constexpr AstIntExpr(Sint32 value, Range range) noexcept
		: AstExpr{KIND, range}, m_kind{Kind::S32}, m_as_s32{value} {}
	constexpr AstIntExpr(Sint64 value, Range range) noexcept
		: AstExpr{KIND, range}, m_kind{Kind::S64}, m_as_s64{value} {}
	virtual void dump(StringBuilder& builder) const noexcept override;
	[[nodiscard]] virtual Maybe<AstConst> eval(Cg&) const noexcept override;
	[[nodiscard]] virtual Maybe<CgValue> gen_value(Cg& cg) const noexcept override;
private:
	Kind m_kind;
	union {
		Uint8  m_as_u8;
		Uint16 m_as_u16;
		Uint32 m_as_u32;
		Uint64 m_as_u64;
		Sint8  m_as_s8;
		Sint16 m_as_s16;
		Sint32 m_as_s32;
		Sint64 m_as_s64;
	};
};

struct AstStrExpr : AstExpr {
	static inline constexpr const auto KIND = Kind::STR;
	constexpr AstStrExpr(StringView literal, Range range) noexcept
		: AstExpr{KIND, range}
		, m_literal{literal}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	[[nodiscard]] virtual Maybe<AstConst> eval(Cg&) const noexcept override;
	[[nodiscard]] Maybe<CgValue> gen_value(Cg& cg) const noexcept override;
private:
	StringView m_literal;
};

struct AstBoolExpr : AstExpr {
	static inline constexpr const auto KIND = Kind::BOOL;
	constexpr AstBoolExpr(Bool value, Range range) noexcept
		: AstExpr{KIND, range}
		, m_value{value}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	[[nodiscard]] constexpr Bool value() const noexcept { return m_value; }
	[[nodiscard]] virtual Maybe<AstConst> eval(Cg&) const noexcept override;
	[[nodiscard]] Maybe<CgValue> gen_value(Cg& cg) const noexcept override;
private:
	Bool m_value;
};

struct AstAggExpr : AstExpr {
	static inline constexpr const auto KIND = Kind::AGG;
	constexpr AstAggExpr(AstType* type, Array<AstExpr*>&& exprs, Range range) noexcept
		: AstExpr{KIND, range}
		, m_type{type}
		, m_exprs{move(exprs)}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	[[nodiscard]] virtual Maybe<AstConst> eval(Cg&) const noexcept override;
	[[nodiscard]] Maybe<CgAddr> gen_addr(Cg& cg) const noexcept override;
	[[nodiscard]] Maybe<CgValue> gen_value(Cg& cg) const noexcept override;
private:
	AstType*        m_type;
	Array<AstExpr*> m_exprs;
};

struct AstBinExpr : AstExpr {
	static inline constexpr auto KIND = Kind::BIN;
	enum class Op {
		ADD, SUB, MUL,
		EQ, NE, GT, GE, LT, LE,
		LOR, LAND,
		BOR, BAND,
		LSHIFT, RSHIFT,
		AS, DOT
	};
	constexpr AstBinExpr(Op op, AstExpr* lhs, AstExpr* rhs, Range range) noexcept
		: AstExpr{Kind::BIN, range}
		, m_op{op}
		, m_lhs{lhs}
		, m_rhs{rhs}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	[[nodiscard]] Maybe<AstConst> eval(Cg&) const noexcept override;
	[[nodiscard]] Maybe<CgAddr> gen_addr(Cg& cg) const noexcept override;
	[[nodiscard]] Maybe<CgValue> gen_value(Cg& cg) const noexcept override;
private:
	Op       m_op;
	AstExpr* m_lhs;
	AstExpr* m_rhs;
};

struct AstUnaryExpr : AstExpr {
	static inline constexpr auto KIND = Kind::UNARY;
	enum class Op {
		NEG, NOT, DEREF, ADDROF
	};
	constexpr AstUnaryExpr(Op op, AstExpr *operand, Range range) noexcept
		: AstExpr{Kind::UNARY, range}
		, m_op{op}
		, m_operand{operand}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	[[nodiscard]] Maybe<CgAddr> gen_addr(Cg& cg) const noexcept override;
	[[nodiscard]] Maybe<CgValue> gen_value(Cg& cg) const noexcept override;
private:
	Op       m_op;
	AstExpr* m_operand;
};

struct AstIndexExpr : AstExpr {
	static inline constexpr auto KIND = Kind::INDEX;
	constexpr AstIndexExpr(AstExpr* operand, AstExpr* index, Range range) noexcept
		: AstExpr{Kind::INDEX, range}
		, m_operand{operand}
		, m_index{index}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	[[nodiscard]] Maybe<CgAddr> gen_addr(Cg& cg) const noexcept override;
	[[nodiscard]] Maybe<CgValue> gen_value(Cg& cg) const noexcept override;
private:
	AstExpr* m_operand;
	AstExpr* m_index;
};

struct AstExplodeExpr : AstExpr {
	static inline constexpr auto KIND = Kind::EXPLODE;
	constexpr AstExplodeExpr(AstExpr* operand, Range range) noexcept
		: AstExpr{KIND, range}
		, m_operand{operand}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
private:
	AstExpr* m_operand;
};

} // namespace Biron

#endif // BIRON_AST_EXPR_H