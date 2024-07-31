#ifndef BIRON_AST_H
#define BIRON_AST_H
#include <biron/util/traits/is_base_of.inl>

#include <biron/util/array.inl>
#include <biron/util/string.inl>
#include <biron/util/numeric.inl>

#include <stdio.h>
#include <biron/llvm.h>
#include <biron/codegen.h>

namespace Biron {

struct AstType;

// Compile time constants
struct Const {
	Uint32 as_u32;
};

struct AstNode {
	enum class Kind : Uint8 { TYPE, EXPR, STMT, FN, ASM };
	constexpr AstNode(Kind kind) noexcept
		: kind{kind}
	{
	}
	virtual ~AstNode() noexcept = default;
	template<DerivedFrom<AstNode> T>
	constexpr Bool is_node() const noexcept {
		return kind = T::KIND;
	}
	Kind kind;
};

struct AstExpr : AstNode {
	static inline constexpr auto KIND = Kind::EXPR;
	enum class Kind : Uint8 {
		TUPLE, CALL, TYPE, VAR, INT, STR, BIN, UNARY, INDEX, ASM
	};
	constexpr AstExpr(Range range, Kind kind) noexcept
		: AstNode{KIND}
		, range{range}
		, kind{kind}
	{
	}
	virtual ~AstExpr() noexcept = default;
	virtual void dump(StringBuilder& builder) const noexcept = 0;
	[[nodiscard]] virtual Maybe<Value> codegen(Codegen& gen) noexcept;
	[[nodiscard]] virtual Maybe<Value> address(Codegen& gen) noexcept;
	[[nodiscard]] virtual Maybe<Const> eval() noexcept { return None {}; }
	template<DerivedFrom<AstExpr> T>
	constexpr Bool is_expr() const noexcept {
		return kind == T::KIND;
	}
	Range range;
	Kind kind;
};

struct AstTupleExpr : AstExpr {
	static inline constexpr auto KIND = Kind::TUPLE;
	constexpr AstTupleExpr(Range range, Array<AstExpr*>&& exprs) noexcept
		: AstExpr{range, KIND}
		, exprs{move(exprs)}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	[[nodiscard]] virtual Maybe<Value> codegen(Codegen& gen) noexcept override;
	[[nodiscard]] Maybe<Array<Value>> explode(Codegen& gen) noexcept;
	Array<AstExpr*> exprs;
};

struct AstCallExpr : AstExpr {
	static inline constexpr auto KIND = Kind::CALL;
	constexpr AstCallExpr(Range range, AstExpr* callee, AstTupleExpr* args, Bool c) noexcept
		: AstExpr{range, KIND}
		, callee{callee}
		, args{args}
		, c{c}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	[[nodiscard]] virtual Maybe<Value> codegen(Codegen& gen) noexcept override;
	AstExpr*      callee;
	AstTupleExpr* args;
	Bool          c; // C ABI
};

struct AstTypeExpr : AstExpr {
	static inline constexpr const auto KIND = Kind::TYPE;
	constexpr AstTypeExpr(Range range, AstType* type) noexcept
		: AstExpr{range, KIND}
		, type{type}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	[[nodiscard]] virtual Maybe<Value> codegen(Codegen& gen) noexcept override;
	AstType* type;
};

struct AstVarExpr : AstExpr {
	static inline constexpr const auto KIND = Kind::VAR;
	constexpr AstVarExpr(Range range, StringView name) noexcept
		: AstExpr{range, KIND}
		, name{name}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	[[nodiscard]] virtual Maybe<Value> codegen(Codegen& gen) noexcept override;
	[[nodiscard]] virtual Maybe<Value> address(Codegen& gen) noexcept override;
	StringView name;
};

struct AstIntExpr : AstExpr {
	static inline constexpr const auto KIND = Kind::INT;
	constexpr AstIntExpr(Range range, Uint32 value) noexcept
		: AstExpr{range, Kind::INT}
		, value{value}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	[[nodiscard]] virtual Maybe<Value> codegen(Codegen& gen) noexcept override;
	[[nodiscard]] virtual Maybe<Const> eval() noexcept override;
	Uint32 value;
};

struct AstStrExpr : AstExpr {
	static inline constexpr const auto KIND = Kind::STR;
	constexpr AstStrExpr(Range range, StringView literal) noexcept
		: AstExpr{range, Kind::STR}
		, literal{literal}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	[[nodiscard]] virtual Maybe<Value> codegen(Codegen& gen) noexcept override;
	StringView literal;
};

struct AstBinExpr : AstExpr {
	static inline constexpr auto KIND = Kind::BIN;
	enum class Operator {
		ADD, SUB, MUL, EQEQ, NEQ, GT, GTE, LT, LTE, AS, LOR, LAND, BOR, BAND, LSHIFT, RSHIFT
	};
	constexpr AstBinExpr(Range range, Operator op, AstExpr* lhs, AstExpr* rhs) noexcept
		: AstExpr{range, Kind::BIN}
		, op{op}
		, lhs{lhs}
		, rhs{rhs}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	[[nodiscard]] virtual Maybe<Value> codegen(Codegen& gen) noexcept override;
	Operator op;
	AstExpr* lhs;
	AstExpr* rhs;
};

struct AstUnaryExpr : AstExpr {
	static inline constexpr auto KIND = Kind::UNARY;
	enum class Operator {
		NEG, NOT, DEREF, ADDROF
	};
	constexpr AstUnaryExpr(Range range, Operator op, AstExpr *operand) noexcept
		: AstExpr{range, Kind::UNARY}
		, op{op}
		, operand{operand}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	[[nodiscard]] virtual Maybe<Value> codegen(Codegen& gen) noexcept override;
	Operator op;
	AstExpr* operand;
};

struct AstIndexExpr : AstExpr {
	static inline constexpr auto KIND = Kind::INDEX;
	constexpr AstIndexExpr(Range range, AstExpr* operand, AstExpr* index) noexcept
		: AstExpr{range, Kind::INDEX}
		, operand{operand}
		, index{index}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	[[nodiscard]] virtual Maybe<Value> codegen(Codegen& gen) noexcept override;
	[[nodiscard]] virtual Maybe<Value> address(Codegen& gen) noexcept override;
	AstExpr* operand;
	AstExpr* index;
};

struct AstAsmExpr : AstExpr {
	static inline constexpr auto KIND = Kind::ASM;
	enum class Kind { REG, IMM, MEM, SUB };
	constexpr AstAsmExpr(Range range, Kind kind)
		: AstExpr{range, KIND}
		, kind{kind}
	{
	}
	virtual ~AstAsmExpr() noexcept = default;
	virtual void dump(StringBuilder& builder) const noexcept = 0;
	Kind kind;
};

struct AstAsmRegExpr : AstAsmExpr {
	static inline constexpr auto KIND = Kind::REG;
	constexpr AstAsmRegExpr(Range range, StringView name, AstAsmExpr* segment) noexcept
		: AstAsmExpr{range, KIND}
		, name{name}
		, segment{segment}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	StringView  name;
	AstAsmExpr* segment;
};

struct AstAsmImmExpr : AstAsmExpr {
	static inline constexpr auto KIND = Kind::IMM;
	constexpr AstAsmImmExpr(Range range, Bool neg, AstIntExpr* expr) noexcept
		: AstAsmExpr{range, KIND}
		, neg{neg}
		, expr{expr}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	Bool        neg;
	AstIntExpr* expr;
};

struct AstAsmMemExpr : AstAsmExpr {
	static inline constexpr auto KIND = Kind::MEM;
	constexpr AstAsmMemExpr(Range range, AstAsmExpr* base, AstAsmExpr* offset, AstAsmExpr* index, AstAsmExpr* size) noexcept
		: AstAsmExpr{range, KIND}
		, base{base}
		, offset{offset}
		, index{index}
		, size{size}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	AstAsmExpr* base;
	AstAsmExpr* offset;
	AstAsmExpr* index;
	AstAsmExpr* size;
};

struct AstAsmSubExpr : AstAsmExpr {
	static inline constexpr auto KIND = Kind::SUB;
	constexpr AstAsmSubExpr(Range range, StringView selector, StringView field, Maybe<StringView>&& modifier) noexcept
		: AstAsmExpr{range, KIND}
		, selector{selector}
		, field{field}
		, modifier{move(modifier)}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	StringView        selector; // 'src' or 'dst'
	StringView        field;
	Maybe<StringView> modifier;
};

struct AstType : AstNode {
	static inline constexpr auto KIND = Kind::TYPE;
	enum class Kind {
		TUPLE, IDENT, VARARGS, PTR, ARRAY, SLICE
	};
	constexpr AstType(Kind kind) noexcept
		: AstNode{AstNode::Kind::TYPE}
		, kind{kind}
	{
	}
	virtual ~AstType() noexcept = default;
	virtual Maybe<Type> codegen(Codegen& gen) noexcept = 0;
	virtual void dump(StringBuilder& builder) const noexcept = 0;
	template<DerivedFrom<AstType> T>
	constexpr Bool is_type() const noexcept {
		return kind == T::KIND;
	}
	Kind kind;
};

struct AstTupleType : AstType {
	static inline constexpr auto KIND = Kind::TUPLE;
	struct Elem {
		Maybe<StringView> name;
		AstType*          type;
		constexpr Elem(Maybe<StringView>&& name, AstType* type) noexcept
			: name{move(name)}
			, type{type}
		{
		}
		constexpr Elem(Elem&& other) noexcept
			: name{move(other.name)}
			, type{exchange(other.type, nullptr)}
		{
		}
	};
	constexpr AstTupleType(Array<Elem>&& elems) noexcept
		: AstType{Kind::TUPLE}
		, elems{move(elems)}
	{
	}
	struct Explode {
		Explode(Maybe<StringView> name, Type type) noexcept
			: name{name}
			, type{type}
		{
		}
		Maybe<StringView> name;
		Type              type;
	};
	[[nodiscard]] virtual Maybe<Type> codegen(Codegen& gen) noexcept override;
	[[nodiscard]] Maybe<Array<Explode>> exploded(Codegen& gen) noexcept;
	virtual void dump(StringBuilder& builder) const noexcept override;
	Array<Elem> elems;
};

struct AstIdentType : AstType {
	static inline constexpr auto KIND = Kind::IDENT;
	StringView ident;
	constexpr AstIdentType(StringView ident) noexcept
		: AstType{Kind::IDENT}
		, ident{ident}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	[[nodiscard]] virtual Maybe<Type> codegen(Codegen& gen) noexcept override;
};

struct AstVarArgsType : AstType {
	static inline constexpr auto KIND = Kind::VARARGS;
	constexpr AstVarArgsType() noexcept
		: AstType{Kind::VARARGS}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	[[nodiscard]] virtual Maybe<Type> codegen(Codegen& gen) noexcept override;
};

struct AstPtrType : AstType {
	static inline constexpr auto Kind = Kind::PTR;
	constexpr AstPtrType(AstType* type) noexcept
		: AstType{Kind::PTR}
		, type{type}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	[[nodiscard]] virtual Maybe<Type> codegen(Codegen& gen) noexcept override;
	AstType* type;
};

struct AstArrayType : AstType {
	static inline constexpr auto Kind = Kind::ARRAY;
	constexpr AstArrayType(AstType* type, AstExpr* extent) noexcept
		: AstType{Kind::ARRAY}
		, type{type}
		, extent{extent}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	[[nodiscard]] virtual Maybe<Type> codegen(Codegen& gen) noexcept override;
	AstType* type;
	AstExpr* extent;
};

struct AstSliceType : AstType {
	static inline constexpr auto Kind = Kind::SLICE;
	constexpr AstSliceType(AstType* type) noexcept
		: AstType{Kind::SLICE}
		, type{type}
	{
	}
	virtual void dump(StringBuilder& builder) const noexcept override;
	[[nodiscard]] virtual Maybe<Type> codegen(Codegen& gen) noexcept override;
	AstType* type;
};

struct AstStmt : AstNode {
	static inline constexpr auto KIND = Kind::STMT;
	enum class Kind {
		BLOCK, RETURN, DEFER, IF, LET, FOR, EXPR, ASSIGN, ASM
	};
	constexpr AstStmt(Kind kind) noexcept
		: AstNode{AstNode::Kind::STMT}
		, kind{kind}
	{
	}
	virtual ~AstStmt() noexcept = default;
	virtual void dump(StringBuilder& builder, int depth) const noexcept = 0;
	[[nodiscard]] virtual Bool codegen(Codegen& gen) noexcept = 0;
	template<DerivedFrom<AstStmt> T>
	constexpr Bool is_stmt() const noexcept {
		return kind == T::KIND;
	}
	Kind kind;
};

struct AstBlockStmt : AstStmt {
	static inline constexpr auto KIND = Kind::BLOCK;
	Array<AstStmt*> stmts;
	constexpr AstBlockStmt(Array<AstStmt*>&& stmts) noexcept
		: AstStmt{Kind::BLOCK}
		, stmts{move(stmts)}
	{
	}
	virtual void dump(StringBuilder& builder, int depth) const noexcept override;
	[[nodiscard]] virtual Bool codegen(Codegen& gen) noexcept override;
};

struct AstReturnStmt : AstStmt {
	static inline constexpr auto KIND = Kind::RETURN;
	constexpr AstReturnStmt(AstExpr* expr) noexcept
		: AstStmt{Kind::RETURN}
		, expr{expr}
	{
	}
	virtual void dump(StringBuilder& builder, int depth) const noexcept override;
	[[nodiscard]] virtual Bool codegen(Codegen& gen) noexcept override;
	AstExpr* expr; // Optional
};

struct AstDeferStmt : AstStmt {
	static inline constexpr auto KIND = Kind::DEFER;
	constexpr AstDeferStmt(AstStmt* stmt) noexcept
		: AstStmt{Kind::DEFER}
		, stmt{stmt} 
	{
	}
	virtual void dump(StringBuilder& builder, int depth) const noexcept override;
	virtual Bool codegen(Codegen& gen) noexcept override;
	AstStmt* stmt;
};

struct AstLetStmt;

struct AstIfStmt : AstStmt {
	static inline constexpr auto KIND = Kind::IF;
	constexpr AstIfStmt(AstLetStmt* init, AstExpr* expr, AstBlockStmt* then, AstStmt* elif) noexcept 
		: AstStmt{Kind::IF}
		, init{init}
		, expr{expr}
		, then{then}
		, elif{elif}
	{
	}
	virtual void dump(StringBuilder& builder, int depth) const noexcept override;
	[[nodiscard]] virtual Bool codegen(Codegen& gen) noexcept override;
	AstLetStmt*   init;
	AstExpr*      expr;
	AstBlockStmt* then;
	AstStmt*      elif; // Either IfStmt or BlockStmt
};

struct AstLetStmt : AstStmt {
	static inline constexpr auto KIND = Kind::LET;
	constexpr AstLetStmt(StringView name, AstExpr* init) noexcept
		: AstStmt{Kind::LET}
		, name{name}
		, init{init}
	{
	}
	virtual void dump(StringBuilder& builder, int depth) const noexcept override;
	[[nodiscard]] virtual Bool codegen(Codegen& gen) noexcept override;
	StringView name;
	AstExpr*   init;
};

struct AstForStmt : AstStmt {
	static inline constexpr auto KIND = Kind::FOR;
	constexpr AstForStmt(AstLetStmt* init, AstExpr* expr, AstStmt* post, AstBlockStmt* body) noexcept
		: AstStmt{Kind::FOR}
		, init{init}
		, expr{expr}
		, post{post}
		, body{body}
	{
	}
	virtual void dump(StringBuilder& builder, int depth) const noexcept override;
	[[nodiscard]] virtual Bool codegen(Codegen& gen) noexcept override;
	AstLetStmt*   init;
	AstExpr*      expr;
	AstStmt*      post;
	AstBlockStmt* body;
};

struct AstExprStmt : AstStmt {
	static inline constexpr auto KIND = Kind::EXPR;
	constexpr AstExprStmt(AstExpr* expr) noexcept
		: AstStmt{Kind::EXPR}
		, expr{expr}
	{
	}
	virtual void dump(StringBuilder& builder, int depth) const noexcept override;
	[[nodiscard]] virtual Bool codegen(Codegen& gen) noexcept override;
	AstExpr* expr;
};

struct AstAssignStmt : AstStmt {
	static inline constexpr auto KIND = Kind::ASSIGN;
	enum class StoreOp { WR, };
	constexpr AstAssignStmt(AstExpr* dst, AstExpr* src, StoreOp op) noexcept
		: AstStmt{Kind::ASSIGN}
		, dst{dst}
		, src{src}
		, op{op}
	{
	}
	virtual void dump(StringBuilder& builder, int depth) const noexcept override;
	[[nodiscard]] virtual Bool codegen(Codegen& gen) noexcept override;
	AstExpr* dst;
	AstExpr* src;
	StoreOp  op;
};

struct AstAsmStmt : AstStmt {
	static inline constexpr auto KIND = Kind::ASM;
	constexpr AstAsmStmt(StringView mnemonic, Array<AstAsmExpr*>&& operands) noexcept
		: AstStmt{KIND}
		, mnemonic{mnemonic}
		, operands{move(operands)}
	{
	}
	virtual void dump(StringBuilder& builder, int depth) const noexcept override;
	[[nodiscard]] virtual Bool codegen(Codegen& gen) noexcept override;
	StringView         mnemonic;
	Array<AstAsmExpr*> operands;
};

struct AstFn : AstNode {
	static inline constexpr auto KIND = Kind::FN;
	constexpr AstFn(AstTupleType* generic, StringView name, AstTupleType* type, AstType* rtype, AstBlockStmt* body) noexcept
		: AstNode{KIND}
		, generic{generic}
		, name{name}
		, type{type}
		, rtype{rtype}
		, body{body}
	{
	}
	[[nodiscard]] Bool codegen(Codegen& gen) noexcept;
	void dump(StringBuilder& builder, int depth) const noexcept;
	AstTupleType* generic;
	StringView    name;
	AstTupleType* type;
	AstType*      rtype;
	AstBlockStmt* body;
};

struct AstAsm : AstNode {
	static inline constexpr auto KIND = Kind::ASM;
	constexpr AstAsm(StringView name, AstTupleType* type, AstType* clobbers, Array<AstAsmStmt*>&& stmts) noexcept
		: AstNode{KIND}
		, name{name}
		, type{type}
		, clobbers{clobbers}
		, stmts{move(stmts)}
	{
	}
	void dump(StringBuilder& builder) const noexcept;
	StringView         name;
	AstTupleType*      type;
	AstType*           clobbers;
	Array<AstAsmStmt*> stmts;
};

} // namespace Biron

#endif // BIRON_AST_H