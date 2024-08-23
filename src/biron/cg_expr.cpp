#include <biron/cg.h>
#include <biron/cg_value.h>

#include <biron/ast_expr.h>
#include <biron/ast_type.h>
#include <biron/ast_const.h>
#include <biron/ast_unit.h>

#include <biron/util/unreachable.inl>

namespace Biron {

Maybe<CgValue> AstExpr::gen_value(Cg&) const noexcept {
	fprintf(stderr, "Unsupported gen_value for AstExpr %s\n", name());
	return None{};
}

Maybe<CgAddr> AstExpr::gen_addr(Cg&) const noexcept {
	fprintf(stderr, "Unsupported gen_addr for AstExpr %s\n", name());
	return None{};
}

Maybe<AstConst> AstExpr::eval() const noexcept {
	fprintf(stderr, "Unsupported eval for AstExpr %s\n", name());
	return None{};
}

const char* AstExpr::name() const noexcept {
	switch (m_kind) {
	case Kind::TUPLE:   return "TUPLE";
	case Kind::CALL:    return "CALL";
	case Kind::TYPE:    return "TYPE";
	case Kind::VAR:     return "VAR";
	case Kind::INT:     return "INT";
	case Kind::BOOL:    return "BOOL";
	case Kind::STR:     return "STR";
	case Kind::AGG:     return "AGG";
	case Kind::BIN:     return "BIN";
	case Kind::UNARY:   return "UNARY";
	case Kind::INDEX:   return "INDEX";
	case Kind::EXPLODE: return "EXPLODE";
	}
	BIRON_UNREACHABLE();
}

Maybe<CgAddr> AstTupleExpr::gen_addr(Cg& cg) const noexcept {
	// When a tuple contains only a single element we detuple it and emit the
	// inner expression directly.
	if (length() == 1) {
		auto value = at(0)->gen_value(cg);
		if (!value) {
			return None{};
		}
		auto dst = cg.emit_alloca(value->type());
		if (!dst || !dst->store(cg, *value)) {
			return None{};
		}
		return dst;
	}

	// Otherwise we actually construct a tuple and emit stores for all the values.
	// We do some additional work for padding fields to ensure they're always
	// zeroed as well.
	Array<CgValue> values{cg.allocator};
	for (Ulen l = length(), i = 0; i < l; i++) {
		auto value = at(i)->gen_value(cg);
		if (!value || !values.push_back(move(*value))) {
			return None{};
		}
	}
	Array<CgType*> types{cg.allocator};
	for (Ulen l = length(), i = 0; i < l; i++) {
		if (!types.push_back(values[i].type())) {
			return None{};
		}
	}
	auto type = cg.types.alloc(CgType::RecordInfo { true, move(types) });
	if (!type) {
		return None{};
	}
	auto addr = cg.emit_alloca(type); // *(...)
	if (!addr) {
		return None{};
	}
	Ulen j = 0;
	for (Ulen l = type->length(), i = 0; i < l; i++) {
		auto dst_type = type->at(i);
		if (dst_type->is_padding()) {
			// Emit zeroinitializer for padding.
			auto padding_addr = addr->at(cg, i);
			auto padding_zero = CgValue::zero(dst_type, cg);
			if (!padding_addr || !padding_zero) {
				return None{};
			}
			if (!padding_addr->store(cg, *padding_zero)) {
				return None{};
			}
		} else {
			// We use j++ to index as values contains non-padding fields.
			auto dst = addr->at(cg, i);
			if (!dst || !dst->store(cg, values[j++])) {
				return None{};
			}
		}
	}
	return addr;
}

Maybe<CgValue> AstTupleExpr::gen_value(Cg& cg) const noexcept {
	if (auto addr = gen_addr(cg)) {
		return addr->load(cg);
	}
	return None{};
}

Maybe<CgValue> AstCallExpr::gen_value(Cg& cg) const noexcept {
	auto callee = m_callee->gen_addr(cg);
	if (!callee) {
		return None{};
	}

	Array<LLVM::ValueRef> values{cg.allocator};
	for (Ulen l = m_args->length(), i = 0; i < l; i++) {
		auto value = m_args->at(i)->gen_value(cg);
		if (!value) {
			return None{};
		}
		// When we're calling a C abi function we destructure the String type and
		// pick out the pointer to the string to pass along instead.
		LLVM::ValueRef ref = nullptr;
		if (m_c && value->type()->is_string()) {
			// We want to extract the 0th element of the String CgValue which contains
			// the raw string pointer. Since it's a pointer we do not need to create
			// an alloca to extract it.
			ref = cg.llvm.BuildExtractValue(cg.builder, value->ref(), 0, "");
		} else {
			ref = value->ref();
		}
		if (!values.push_back(ref)) {
			return None{};
		}
	}

	auto type = callee->type()->deref();
	auto value = cg.llvm.BuildCall2(cg.builder,
	                                type->ref(cg),
	                                callee->ref(),
	                                values.data(),
	                                values.length(),
	                                "");

	if (!value) {
		return None{};
	}

	// When the rets tuple only contains a single element we detuple it. This
	// changes the return type of the function from (T) to T.
	auto rets = type->at(1);
	if (rets->length() == 1) {
		return CgValue { rets->at(0), value };
	}
	
	return CgValue { rets, value };
}

Maybe<CgAddr> AstVarExpr::gen_addr(Cg& cg) const noexcept {
	// Search function locals
	for (const auto& var : cg.vars) {
		if (var.name() == m_name) {
			return var.addr();
		}
	}
	// Search module for functions.
	for (const auto& fn : cg.fns) {
		if (fn.name() == m_name) {
			return fn.addr();
		}
	}
	fprintf(stderr, "Could not find symbol '%.*s'\n", Sint32(m_name.length()), m_name.data());
	return None{};
}

Maybe<CgValue> AstVarExpr::gen_value(Cg& cg) const noexcept {
	if (auto addr = gen_addr(cg)) {
		return addr->load(cg);
	}

	return None{};
}

Maybe<AstConst> AstIntExpr::eval() const noexcept {
	switch (m_kind) {
	case Kind::U8:  return AstConst { range(), m_as_u8 };
	case Kind::U16: return AstConst { range(), m_as_u16 };
	case Kind::U32: return AstConst { range(), m_as_u32 };
	case Kind::U64: return AstConst { range(), m_as_u64 };
	case Kind::S8:  return AstConst { range(), m_as_s8 };
	case Kind::S16: return AstConst { range(), m_as_s16 };
	case Kind::S32: return AstConst { range(), m_as_s32 };
	case Kind::S64: return AstConst { range(), m_as_s64 };
	}
	return None{};
}

Maybe<CgValue> AstIntExpr::gen_value(Cg& cg) const noexcept {
	CgType* t = nullptr;
	LLVM::ValueRef v = nullptr;
	switch (m_kind) {
	/****/ case Kind::U8:  t = cg.types.u8(),  v = cg.llvm.ConstInt(t->ref(cg), m_as_u8, false);
	break; case Kind::U16: t = cg.types.u16(), v = cg.llvm.ConstInt(t->ref(cg), m_as_u16, false);
	break; case Kind::U32: t = cg.types.u32(), v = cg.llvm.ConstInt(t->ref(cg), m_as_u32, false);
	break; case Kind::U64: t = cg.types.u64(), v = cg.llvm.ConstInt(t->ref(cg), m_as_u64, false);
	break; case Kind::S8:  t = cg.types.s8(),  v = cg.llvm.ConstInt(t->ref(cg), m_as_s8, true);
	break; case Kind::S16: t = cg.types.s16(), v = cg.llvm.ConstInt(t->ref(cg), m_as_s16, true);
	break; case Kind::S32: t = cg.types.s32(), v = cg.llvm.ConstInt(t->ref(cg), m_as_s32, true);
	break; case Kind::S64: t = cg.types.s64(), v = cg.llvm.ConstInt(t->ref(cg), m_as_s64, true);
	}
	if (v) {
		return CgValue { t, v };
	}
	return None{};
}

Maybe<AstConst> AstStrExpr::eval() const noexcept {
	return AstConst { range(), m_literal };
}

Maybe<CgValue> AstStrExpr::gen_value(Cg& cg) const noexcept {
	// When building a string we need to escape it and add a NUL terminator. Biron
	// does not have NUL terminated strings but it always adds a NUL for literals
	// so they can be safely passed to C functions expecting NUL termination.
	StringBuilder builder{cg.allocator};
	for (auto it = m_literal.begin(); it != m_literal.end(); ++it) {
		if (*it != '\\') {
			builder.append(*it);
			continue;
		}
		switch (*++it) {
		/****/ case '\\': builder.append(*it);
		break; case 'n':  builder.append('\n');
		break; case 'r':  builder.append('\r');
		break; case 't':  builder.append('\t');
		}
	}
	builder.append('\0');
	if (!builder.valid()) {
		return None{};
	}
	auto ptr = cg.llvm.BuildGlobalString(cg.builder,
	                                     builder.data(),
	                                     "");
	auto len = cg.llvm.ConstInt(cg.types.u64()->ref(cg), m_literal.length(), false);
	if (!ptr || !len) {
		return None{};
	}
	LLVM::ValueRef values[2] = { ptr, len };
	auto t = cg.types.str();
	auto v = cg.llvm.ConstNamedStruct(t->ref(cg), values, 2);
	return CgValue { t, v };
}

Maybe<AstConst> AstBoolExpr::eval() const noexcept {
	return AstConst { range(), Bool32 { m_value } };
}

Maybe<CgValue> AstBoolExpr::gen_value(Cg& cg) const noexcept {
	// LLVM has an Int1 type which can store either a 0 or 1 value which is what
	// the IR uses for "boolean" like things. We map our Bool32 to this type.
	// except our booleans are typed.
	auto t = cg.types.b32();
	auto v = cg.llvm.ConstInt(t->ref(cg), m_value ? 1 : 0, false);
	return CgValue { t, v };
}

Maybe<CgAddr> AstAggExpr::gen_addr(Cg& cg) const noexcept {
	auto type = m_type->codegen(cg);
	if (!type) {
		return None{};
	}

	// When emitting an aggregate we need to stack allocate because the aggregate
	// likely cannot fit into a regsiter. This means an aggregate has an address.
	auto addr = cg.emit_alloca(type);
	if (!addr) {
		return None{};
	}

	auto count = type->is_array() ? type->extent() : type->length();

	// We now actually go over every index in the type.
	for (Ulen l = count, i = 0, j = 0; i < l; i++) {
		auto dst = addr->at(cg, i);
		if (!dst) {
			return None{};
		}
		// The 'dst' type will always be a pointer so dereference.
		auto type = dst->type()->deref();
		if (type->is_padding()) {
			// Write a zeroinitializer into padding at i'th.
			auto zero = CgValue::zero(type, cg);
			if (!zero || !dst->store(cg, *zero)) {
				return None{};
			}
		} else if (auto expr = m_exprs.at(j++)) {
			// Otherwise take the next expression and store it at i'th.
			auto value = (*expr)->gen_value(cg);
			if (!value) {
				return None{};
			}
			if (!dst->store(cg, *value)) {
				return None{};
			}
		} else {
			// No expression for that initializer so generate a zeroinitializer
			auto zero = CgValue::zero(type, cg);
			if (!zero || !dst->store(cg, *zero)) {
				return None{};
			}
		}
	}
	return addr;
}

Maybe<CgValue> AstAggExpr::gen_value(Cg& cg) const noexcept {
	if (auto addr = gen_addr(cg)) {
		return addr->load(cg);
	}
	return None{};
}

Maybe<CgAddr> AstBinExpr::gen_addr(Cg& cg) const noexcept {
	if (m_op != Op::DOT) {
		return None{};
	}

	auto lhs = m_lhs->gen_addr(cg);
	if (!lhs) {
		return None{};
	}
	auto type = lhs->type()->deref();
	Bool ptr = false;
	if (type->is_pointer()) {
		ptr = true;
		// We do the implicit dereference behavior so that we do not need a
		// '->' operator like C and C++ here
		type = type->deref();
	}
	if (!type->is_struct()) {
		fprintf(stderr, "Can only index structure types with '.' operator\n");
		return None{};
	}

	if (!m_rhs->is_expr<AstVarExpr>()) {
		fprintf(stderr, "Expected structure field on right-hand-side of '.' operator\n");
		return None{};
	}

	// TODO(dweiler): Work out the field index

	if (ptr) {
		auto load = lhs->load(cg);
		auto addr = CgAddr { load->type(), load->ref() };
		return addr.at(cg, 0);
	} else {
		return lhs->at(cg, 0);
	}
}

Maybe<CgValue> AstBinExpr::gen_value(Cg& cg) const noexcept {
	using IntPredicate = LLVM::IntPredicate;

	Maybe<CgValue> lhs;
	Maybe<CgValue> rhs;
	if (m_op != Op::DOT && m_op != Op::LOR && m_op != Op::LAND) {
 		lhs = m_lhs->gen_value(cg);
 		rhs = m_rhs->gen_value(cg);
		if (!lhs || !rhs) {
			return None{};
		}
	}

	switch (m_op) {
	case Op::ADD:
		return CgValue { lhs->type(), cg.llvm.BuildAdd(cg.builder, lhs->ref(), rhs->ref(), "") };
	case Op::SUB:
		return CgValue { lhs->type(), cg.llvm.BuildSub(cg.builder, lhs->ref(), rhs->ref(), "") };
	case Op::MUL:
		return CgValue { lhs->type(), cg.llvm.BuildMul(cg.builder, lhs->ref(), rhs->ref(), "") };
	case Op::EQ:
		return CgValue { cg.types.b32(), cg.llvm.BuildICmp(cg.builder, IntPredicate::EQ, lhs->ref(), rhs->ref(), "") };
	case Op::NE:
		return CgValue { cg.types.b32(), cg.llvm.BuildICmp(cg.builder, IntPredicate::NE, lhs->ref(), rhs->ref(), "") };
	case Op::GT:
		if (lhs->type()->is_sint()) {
			return CgValue { lhs->type(), cg.llvm.BuildICmp(cg.builder, IntPredicate::SGT, lhs->ref(), rhs->ref(), "") };
		} else if (lhs->type()->is_uint()) {
			return CgValue { lhs->type(), cg.llvm.BuildICmp(cg.builder, IntPredicate::UGT, lhs->ref(), rhs->ref(), "") };
		}
		break;
	case Op::GE:
		if (lhs->type()->is_sint()) {
			return CgValue { lhs->type(), cg.llvm.BuildICmp(cg.builder, IntPredicate::SGE, lhs->ref(), rhs->ref(), "") };
		} else if (lhs->type()->is_uint()) {
			return CgValue { lhs->type(), cg.llvm.BuildICmp(cg.builder, IntPredicate::UGE, lhs->ref(), rhs->ref(), "") };
		}
		break;
	case Op::LT:
		if (lhs->type()->is_sint()) {
			return CgValue { lhs->type(), cg.llvm.BuildICmp(cg.builder, IntPredicate::SLT, lhs->ref(), rhs->ref(), "") };
		} else if (lhs->type()->is_uint()) {
			return CgValue { lhs->type(), cg.llvm.BuildICmp(cg.builder, IntPredicate::ULT, lhs->ref(), rhs->ref(), "") };
		}
		break;
	case Op::LE:
		if (lhs->type()->is_sint()) {
			return CgValue { lhs->type(), cg.llvm.BuildICmp(cg.builder, IntPredicate::SLE, lhs->ref(), rhs->ref(), "") };
		} else if (lhs->type()->is_uint()) {
			return CgValue { lhs->type(), cg.llvm.BuildICmp(cg.builder, IntPredicate::ULE, lhs->ref(), rhs->ref(), "") };
		}
		break;
	case Op::AS:
		// TODO(dweiler): Implement
		break;
	case Op::LOR:
		{
			// CBB
			//   %0 = <lhs>
			//   cond br %0, %on_lhs_true, %on_lhs_false
			// on_lhs_true:
			//   br on_exit -> phi true
			// on_lhs_false:
			//   %1 = <rhs>
			//   cond br %1, %on_rhs_true, %on_rhs_false
			// on_rhs_true:
			//   br on_exit -> phi true
			// on_rhs_false:
			//   br on_exit -> phi false
			// on_exit:
			//   %2 = phi [ true %on_lhs_true ], [ true, %on_rhs_true ], [ false, %on_rhs_false ]
		
			auto this_bb = cg.llvm.GetInsertBlock(cg.builder);
			auto this_fn = cg.llvm.GetBasicBlockParent(this_bb);

			auto on_lhs_true  = cg.llvm.CreateBasicBlockInContext(cg.context, "on_lhs_true");
			auto on_lhs_false = cg.llvm.CreateBasicBlockInContext(cg.context, "on_lhs_false");
			auto on_rhs_true  = cg.llvm.CreateBasicBlockInContext(cg.context, "on_rhs_true");
			auto on_rhs_false = cg.llvm.CreateBasicBlockInContext(cg.context, "on_rhs_false");
			auto on_exit      = cg.llvm.CreateBasicBlockInContext(cg.context, "on_exit");
	
			auto lhs = m_lhs->gen_value(cg);
			if (!lhs || !lhs->type()->is_bool()) {
				return None{};
			}

			cg.llvm.BuildCondBr(cg.builder, lhs->ref(), on_lhs_true, on_lhs_false);

			// on_lhs_true
			cg.llvm.AppendExistingBasicBlock(this_fn, on_lhs_true);
			cg.llvm.PositionBuilderAtEnd(cg.builder, on_lhs_true);
			cg.llvm.BuildBr(cg.builder, on_exit);
		
			// on_lhs_false
			cg.llvm.AppendExistingBasicBlock(this_fn, on_lhs_false);
			cg.llvm.PositionBuilderAtEnd(cg.builder, on_lhs_false);
			auto rhs = m_rhs->gen_value(cg);
			if (!rhs || !rhs->type()->is_bool()) {
				return None{};
			}
			on_lhs_false = cg.llvm.GetInsertBlock(cg.builder);
			cg.llvm.BuildCondBr(cg.builder, rhs->ref(), on_rhs_true, on_rhs_false);

			// on_rhs_true
			cg.llvm.AppendExistingBasicBlock(this_fn, on_rhs_true);
			cg.llvm.PositionBuilderAtEnd(cg.builder, on_rhs_true);
			cg.llvm.BuildBr(cg.builder, on_exit);

			// on_rhs_false
			cg.llvm.AppendExistingBasicBlock(this_fn, on_rhs_false);
			cg.llvm.PositionBuilderAtEnd(cg.builder, on_rhs_false);
			cg.llvm.BuildBr(cg.builder, on_exit);

			// on_exit
			cg.llvm.AppendExistingBasicBlock(this_fn, on_exit);
			cg.llvm.PositionBuilderAtEnd(cg.builder, on_exit);
			
			LLVM::BasicBlockRef blocks[] = {
				on_lhs_true,
				on_rhs_true,
				on_rhs_false,
			};

			LLVM::ValueRef values[] = {
				cg.llvm.ConstInt(cg.types.b32()->ref(cg), 1, false),
				cg.llvm.ConstInt(cg.types.b32()->ref(cg), 1, false),
				cg.llvm.ConstInt(cg.types.b32()->ref(cg), 0, false),
			};
			
			auto phi = cg.llvm.BuildPhi(cg.builder, cg.types.b32()->ref(cg), "");
			cg.llvm.AddIncoming(phi, values, blocks, 3);

			return CgValue { cg.types.b32(), phi };
		}
		break;
	case Op::LAND:
		{
			// CBB
			//   %0 = <lhs>
			//   cond br %0, %on_lhs_true, %on_lhs_false
			// on_lhs_true:
			//   %1 = <rhs>
			//   cond br %1, %on_rhs_true, %on_rhs_false
			// on_lhs_false:
			//   br on_exit -> phi false
			// on_rhs_true:
			//   br on_exit -> phi true
			// on_rhs_false:
			//   br on_exit -> phi false
			// on_exit:
			//   %2 = phi [ false %on_lhs_false ], [ true, %on_rhs_true ], [ false, %on_rhs_false ]

			auto this_bb = cg.llvm.GetInsertBlock(cg.builder);
			auto this_fn = cg.llvm.GetBasicBlockParent(this_bb);

			auto on_lhs_true  = cg.llvm.CreateBasicBlockInContext(cg.context, "on_lhs_true");
			auto on_lhs_false = cg.llvm.CreateBasicBlockInContext(cg.context, "on_lhs_false");
			auto on_rhs_true  = cg.llvm.CreateBasicBlockInContext(cg.context, "on_rhs_true");
			auto on_rhs_false = cg.llvm.CreateBasicBlockInContext(cg.context, "on_rhs_false");
			auto on_exit      = cg.llvm.CreateBasicBlockInContext(cg.context, "on_exit");
	
			auto lhs = m_lhs->gen_value(cg);
			if (!lhs || !lhs->type()->is_bool()) {
				return None{};
			}

			cg.llvm.BuildCondBr(cg.builder, lhs->ref(), on_lhs_true, on_lhs_false);

			// on_lhs_true
			cg.llvm.AppendExistingBasicBlock(this_fn, on_lhs_true);
			cg.llvm.PositionBuilderAtEnd(cg.builder, on_lhs_true);
			auto rhs = m_rhs->gen_value(cg);
			if (!rhs || !rhs->type()->is_bool()) {
				return None{};
			}
			cg.llvm.BuildCondBr(cg.builder, rhs->ref(), on_rhs_true, on_rhs_false);

			// on_lhs_false
			cg.llvm.AppendExistingBasicBlock(this_fn, on_lhs_false);
			cg.llvm.PositionBuilderAtEnd(cg.builder, on_lhs_false);
			cg.llvm.BuildBr(cg.builder, on_exit);

			// on_rhs_true
			cg.llvm.AppendExistingBasicBlock(this_fn, on_rhs_true);
			cg.llvm.PositionBuilderAtEnd(cg.builder, on_rhs_true);
			cg.llvm.BuildBr(cg.builder, on_exit);

			// on_rhs_false
			cg.llvm.AppendExistingBasicBlock(this_fn, on_rhs_false);
			cg.llvm.PositionBuilderAtEnd(cg.builder, on_rhs_false);
			cg.llvm.BuildBr(cg.builder, on_exit);

			// on_exit
			cg.llvm.AppendExistingBasicBlock(this_fn, on_exit);
			cg.llvm.PositionBuilderAtEnd(cg.builder, on_exit);

			LLVM::BasicBlockRef blocks[] = {
				on_lhs_false,
				on_rhs_true,
				on_rhs_false,
			};

			LLVM::ValueRef values[] = {
				cg.llvm.ConstInt(cg.types.b32()->ref(cg), 0, false),
				cg.llvm.ConstInt(cg.types.b32()->ref(cg), 1, false),
				cg.llvm.ConstInt(cg.types.b32()->ref(cg), 0, false),
			};
			
			auto phi = cg.llvm.BuildPhi(cg.builder, cg.types.b32()->ref(cg), "");
			cg.llvm.AddIncoming(phi, values, blocks, 3);

			return CgValue { cg.types.b32(), phi };
		}
		break;
	case Op::BOR:
		return CgValue { lhs->type(), cg.llvm.BuildOr(cg.builder, lhs->ref(), rhs->ref(), "") };
	case Op::BAND:
		return CgValue { lhs->type(), cg.llvm.BuildAnd(cg.builder, lhs->ref(), rhs->ref(), "") };
	case Op::LSHIFT:
		return CgValue { lhs->type(), cg.llvm.BuildShl(cg.builder, lhs->ref(), rhs->ref(), "") };
	case Op::RSHIFT:
		if (lhs->type()->is_sint()) {
			return CgValue { lhs->type(), cg.llvm.BuildAShr(cg.builder, lhs->ref(), rhs->ref(), "") };
		} else if (lhs->type()->is_uint()) {
			return CgValue { lhs->type(), cg.llvm.BuildLShr(cg.builder, lhs->ref(), rhs->ref(), "") };
		}
		break;
	case Op::OF:
		// This is the complex beast.
		break;
	case Op::DOT:
		if (auto addr = gen_addr(cg)) {
			return addr->load(cg);
		}
		break;
	default:
		break;
	}
	return None{};
}

Maybe<CgAddr> AstUnaryExpr::gen_addr(Cg& cg) const noexcept {
	switch (m_op) {
	case Op::NEG:
		fprintf(stderr, "Cannot take the address of unary - expression");
		return None{};
	case Op::NOT:
		fprintf(stderr, "Cannot take the address of unary ! expression");
		return None{};
	case Op::DEREF:
		// When dereferencing on the LHS we will have a R-value of the address 
		// which we just turn back into an address.
		//
		// The LHS dereference does not actually load or dereference anything, it's
		// used in combination with an AssignStmt to allow storing results through
		// the pointer.
		if (auto operand = m_operand->gen_value(cg)) {
			return CgAddr { operand->type(), operand->ref() };
		}
		break;
	case Op::ADDROF:
		fprintf(stderr, "Cannot take the address of unary & expression");
		break;
	}
	return None{};
}

Maybe<CgValue> AstUnaryExpr::gen_value(Cg& cg) const noexcept {
	switch (m_op) {
	case Op::NEG:
		if (auto operand = m_operand->gen_value(cg)) {
			return CgValue { operand->type(), cg.llvm.BuildNeg(cg.builder, operand->ref(), "") };
		}
		break;
	case Op::NOT:
		if (auto operand = m_operand->gen_value(cg)) {
			return CgValue { operand->type(), cg.llvm.BuildNot(cg.builder, operand->ref(), "") };
		}
		break;
	case Op::DEREF:
		// When dereferencing on the RHS we just gen_addr followed by a load.
		if (auto addr = gen_addr(cg)) {
			return addr->load(cg);
		}
		break;
	case Op::ADDROF:
		// When taking the address we just gen_addr and turn it into a CgValue which
		// gives us an R-value of the address.
		if (auto operand = m_operand->gen_addr(cg)) {
			return CgValue { operand->type(), operand->ref() };
		}
		return None{};
	}

	return None{};
}

Maybe<CgAddr> AstIndexExpr::gen_addr(Cg& cg) const noexcept {
	auto operand = m_operand->gen_addr(cg);
	if (!operand) {
		return None{};
	}

	auto index = m_index->gen_value(cg);
	if (!index) {
		return None{};
	}

	if (auto addr = operand->at(cg, *index)) {
		return addr;
	}

	return None{};
}

Maybe<CgValue> AstIndexExpr::gen_value(Cg& cg) const noexcept {
	if (auto addr = gen_addr(cg)) {
		return addr->load(cg);
	}
	return None{};
}

} // namespace Biron