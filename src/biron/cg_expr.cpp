#include <biron/cg.h>
#include <biron/cg_value.h>

#include <biron/ast_expr.h>
#include <biron/ast_type.h>
#include <biron/ast_const.h>
#include <biron/ast_unit.h>

#include <biron/util/unreachable.inl>

namespace Biron {

Maybe<CgAddr> AstExpr::gen_addr(Cg& cg) const noexcept {
	cg.fatal(range(), "Unsupported gen_addr for %s", name());
	return None{};
}

Maybe<CgValue> AstExpr::gen_value(Cg& cg) const noexcept {
	cg.fatal(range(), "Unsupported gen_value for %s", name());
	return None{};
}

CgType* AstExpr::gen_type(Cg& cg) const noexcept {
	cg.fatal(range(), "Unsupported gen_type for %s", name());
	return nullptr;
}

Maybe<AstConst> AstExpr::eval_value() const noexcept {
	return None{};
}

const char* AstExpr::name() const noexcept {
	switch (m_kind) {
	case Kind::TUPLE:   return "TUPLE";
	case Kind::CALL:    return "CALL";
	case Kind::TYPE:    return "TYPE";
	case Kind::VAR:     return "VAR";
	case Kind::INT:     return "INT";
	case Kind::FLT:     return "FLT";
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

Maybe<AstConst> AstTupleExpr::eval_value() const noexcept {
	// When a tuple contains only a single element we detuple it and emit the
	// inner expression directly.
	if (length() == 1) {
		auto value = m_exprs[0]->eval_value();
		if (!value) {
			return None{};
		}
		return value;
	}

	Array<AstConst> values{m_exprs.allocator()};
	Range range{0, 0};
	for (const auto& expr : m_exprs) {
		range.include(expr->range());
		auto value = expr->eval_value();
		if (!value || !values.push_back(move(*value))) {
			return None{};
		}
	}
	// Should we infer the type for ConstTuple here? A compile-time constant tuple
	// cannot have fields and cannot be indexed any other way than with integers,
	// for which a type is not needed. However, it might be useful to have a type
	// anyways. Revisit this later. As of now we just pass nullptr.
	return AstConst { range, AstConst::ConstTuple { nullptr, move(values), None{} } };
}

Maybe<CgAddr> AstTupleExpr::gen_addr(Cg& cg) const noexcept {
	auto type = gen_type(cg);
	if (!type) {
		return None{};
	}

	// When a tuple contains only a single element we detuple it and emit the
	// inner expression directly.
	if (length() == 1) {
		auto value = at(0)->gen_value(cg);
		if (!value) {
			return None{};
		}
		auto dst = cg.emit_alloca(type);
		if (!dst || !dst->store(cg, *value)) {
			return cg.oom();
		}
		return dst;
	}

	// Otherwise we actually construct a tuple and emit stores for all the values.
	// We do some additional work for padding fields to ensure they're always
	// zeroed as well.
	Array<CgValue> values{*cg.scratch};
	for (Ulen l = length(), i = 0; i < l; i++) {
		auto value = at(i)->gen_value(cg);
		if (!value) {
			return None{};
		}
		if (!values.push_back(move(*value))) {
			return cg.oom();
		}
	}

	auto addr = cg.emit_alloca(type); // *(...)
	if (!addr) {
		return cg.oom();
	}

	Ulen j = 0;
	for (Ulen l = type->length(), i = 0; i < l; i++) {
		auto dst_type = type->at(i);
		if (dst_type->is_padding()) {
			// Emit zeroinitializer for padding.
			auto padding_addr = addr->at(cg, i);
			auto padding_zero = CgValue::zero(dst_type, cg);
			if (!padding_addr || !padding_zero) {
				return cg.oom();
			}
			if (!padding_addr->store(cg, *padding_zero)) {
				return cg.oom();
			}
		} else {
			// We use j++ to index as values contains non-padding fields.
			auto dst = addr->at(cg, i);
			if (!dst || !dst->store(cg, values[j++])) {
				return cg.oom();
			}
		}
	}
	return addr;
}

Maybe<CgValue> AstTupleExpr::gen_value(Cg& cg) const noexcept {
	if (auto addr = gen_addr(cg)) {
		return addr->load(cg);
	}
	cg.fatal(range(), "Could not generate address");
	return None{};
}

CgType* AstTupleExpr::gen_type(Cg& cg) const noexcept {
	// When a tuple contains only a single element we detuple it so we only want
	// the inner type here.
	if (length() == 1) {
		return at(0)->gen_type(cg);
	}

	Array<CgType*> types{*cg.scratch};
	if (!types.reserve(length())) {
		cg.oom();
		return nullptr;
	}
	for (Ulen l = length(), i = 0; i < l; i++) {
		auto type = at(i)->gen_type(cg);
		if (!type) {
			return nullptr;
		}
		if (!types.push_back(type)) {
			cg.oom();
			return nullptr;
		}
	}

	return cg.types.make(CgType::TupleInfo { move(types), None{}, None{} });
}

Bool AstTupleExpr::prepend(Array<AstExpr*>&& exprs) noexcept {
	for (auto expr : m_exprs) {
		if (!exprs.push_back(expr)) {
			return false;
		}
	}
	m_exprs = move(exprs);
	return true;
}

CgType* AstCallExpr::gen_type(Cg& cg) const noexcept {
	auto fn = m_callee->gen_type(cg);
	if (!fn || !fn->is_fn()) {
		return nullptr;
	}
	auto rets = fn->at(2);
	return rets->length() == 1 ? rets->at(0) : rets;
}

Maybe<CgValue> AstCallExpr::gen_value(Cg& cg) const noexcept {
	return gen_value(None{}, cg);
}

Maybe<CgValue> AstCallExpr::gen_value(const Maybe<Array<CgValue>>& prepend, Cg& cg) const noexcept {
	auto callee = m_callee->gen_addr(cg);
	if (!callee) {
		return None{};
	}

	if (!callee->type()->deref()->is_fn()) {
		cg.error(m_callee->range(), "Callee is not a function");
		return None{};
	}

	// 0 = objs
	// 1 = args
	// 2 = rets

	auto expected = callee->type()->deref()->at(1);

	Array<LLVM::ValueRef> values{*cg.scratch};
	Ulen reserve = 0;
	if (prepend) {
		reserve += prepend->length();
	}
	reserve += m_args->length();
	if (!values.reserve(reserve)) {
		return cg.oom();
	}
	if (prepend) {
		for (auto value : *prepend) {
			if (!values.push_back(value.ref())) {
				return cg.oom();
			}
		}
	}
	Ulen k = 0;
	for (Ulen l = m_args->length(), i = 0; i < l; i++) {
		auto arg = m_args->at(i);
		if (arg->is_expr<AstExplodeExpr>()) {
			auto expr = static_cast<const AstExplodeExpr*>(arg);
			auto args = expr->gen_value(cg);
			if (!args) {
				return None{};
			}
			for (Ulen l = args->type()->length(), i = 0; i < l; i++) {
				auto value = cg.llvm.BuildExtractValue(cg.builder, args->ref(), i, "");
				auto have_type = args->type()->at(i);
				auto want_type = expected->at(k);
				if (*have_type != *want_type) {
					auto have_type_string = have_type->to_string(*cg.scratch);
					auto want_type_string = want_type->to_string(*cg.scratch);
					cg.error(m_args->range(),
					         "Expected expression of type '%S' in expansion of tuple for argument '%zu'. Got '%S' instead",
					         want_type_string,
					         k + 1,
					         have_type_string);
					return None{};
				}
				k++;
				if (!value || !values.push_back(value)) {
					return None{};
				}
			}
			continue;
		} 
		auto value = arg->gen_value(cg);
		if (!value) {
			return None{};
		}
		// When we're calling a C abi function we destructure the String type and
		// pick out the pointer to the string to pass along instead.
		auto have_type = value->type();
		LLVM::ValueRef ref = nullptr;
		if (m_c && value->type()->is_string()) {
			// We want to extract the 0th element of the String CgValue which contains
			// the raw string pointer. Since it's a pointer we do not need to create
			// an alloca to extract it.
			ref = cg.llvm.BuildExtractValue(cg.builder, value->ref(), 0, "");
			have_type = cg.types.ptr();
		} else {
			ref = value->ref();
		}
		auto want_type = expected->at(min(k, expected->length() - 1));
		if (!want_type->is_va() && *have_type != *want_type) {
			auto have_type_string = have_type->to_string(*cg.scratch);
			auto want_type_string = want_type->to_string(*cg.scratch);
			cg.error(arg->range(),
			         "Expected expression of type '%S' for argument '%zu'. Got '%S' instead",
			         want_type_string,
			         k + 1,
			         have_type_string);
			return None{};
		}
		k++;
		if (!values.push_back(ref)) {
			return cg.oom();
		}
	}

	auto type = callee->type()->deref();
	auto value = cg.llvm.BuildCall2(cg.builder,
	                                type->ref(),
	                                callee->ref(),
	                                values.data(),
	                                values.length(),
	                                "");

	// When the rets tuple only contains a single element we detuple it. This
	// changes the return type of the function from (T) to T.
	auto rets = type->at(2);
	if (rets->length() == 1) {
		return CgValue { rets->at(0), value };
	}
	
	return CgValue { rets, value };
}

Maybe<CgAddr> AstVarExpr::gen_addr(Cg& cg) const noexcept {
	// Search function locals in reverse scope order
	for (Ulen l = cg.scopes.length(), i = l - 1; i < l; i--) {
		const auto& scope = cg.scopes[i];
		for (const auto& var : scope.vars) {
			if (var.name() == m_name) {
				return var.addr();
			}
		}
	}
	// Search module for functions.
	for (const auto& fn : cg.fns) {
		if (fn.name() == m_name) {
			return fn.addr();
		}
	}
	// Search module for globals.
	for (const auto& global : cg.globals) {
		if (global.name() == m_name) {
			return global.addr();
		}
	}

	cg.error(range(), "Could not find symbol '%S'", m_name);
	return None{};
}

Maybe<CgValue> AstVarExpr::gen_value(Cg& cg) const noexcept {
	if (auto addr = gen_addr(cg)) {
		return addr->load(cg);
	}
	cg.fatal(range(), "Could not generate value");
	return None{};
}

CgType* AstVarExpr::gen_type(Cg& cg) const noexcept {
	auto addr = gen_addr(cg);
	if (!addr) {
		cg.fatal(range(), "Could not generate type");
		return nullptr;
	}
	return addr->type()->deref();
}

Maybe<AstConst> AstIntExpr::eval_value() const noexcept {
	switch (m_kind) {
	case Kind::U8:  return AstConst { range(), Uint8(m_as_uint) };
	case Kind::U16: return AstConst { range(), Uint16(m_as_uint) };
	case Kind::U32: return AstConst { range(), Uint32(m_as_uint) };
	case Kind::U64: return AstConst { range(), Uint64(m_as_uint) };
	case Kind::S8:  return AstConst { range(), Sint8(m_as_sint) };
	case Kind::S16: return AstConst { range(), Sint16(m_as_sint) };
	case Kind::S32: return AstConst { range(), Sint32(m_as_sint) };
	case Kind::S64: return AstConst { range(), Sint64(m_as_sint) };
	case Kind::UNTYPED:
		return AstConst { range(), AstConst::UntypedInt { m_as_uint } };
	}
	return None{};
}

Maybe<CgValue> AstIntExpr::gen_value(Cg& cg) const noexcept {
	auto type = gen_type(cg);
	if (!type) {
		return None{};
	}
	LLVM::ValueRef v = nullptr;
	switch (m_kind) {
	/****/ case Kind::U8:  v = cg.llvm.ConstInt(type->ref(), m_as_uint, false);
	break; case Kind::U16: v = cg.llvm.ConstInt(type->ref(), m_as_uint, false);
	break; case Kind::U32: v = cg.llvm.ConstInt(type->ref(), m_as_uint, false);
	break; case Kind::U64: v = cg.llvm.ConstInt(type->ref(), m_as_uint, false);
	break; case Kind::S8:  v = cg.llvm.ConstInt(type->ref(), m_as_sint, true);
	break; case Kind::S16: v = cg.llvm.ConstInt(type->ref(), m_as_sint, true);
	break; case Kind::S32: v = cg.llvm.ConstInt(type->ref(), m_as_sint, true);
	break; case Kind::S64: v = cg.llvm.ConstInt(type->ref(), m_as_sint, true);
	break; case Kind::UNTYPED:
		cg.fatal(range(), "Untyped integer value must be typed");
		break;
	}
	if (v) {
		return CgValue { type, v };
	}
	cg.fatal(range(), "Could not generate value");
	return None{};
}

CgType* AstIntExpr::gen_type(Cg& cg) const noexcept {
	switch (m_kind) {
	case Kind::U8:  return cg.types.u8();
	case Kind::U16: return cg.types.u16();
	case Kind::U32: return cg.types.u32();
	case Kind::U64: return cg.types.u64();
	case Kind::S8:  return cg.types.s8();
	case Kind::S16: return cg.types.s16();
	case Kind::S32: return cg.types.s32();
	case Kind::S64: return cg.types.s64();
	case Kind::UNTYPED:
		cg.fatal(range(), "Untyped integer value must be typed");
		break;
	}
	return nullptr;
}

Maybe<AstConst> AstFltExpr::eval_value() const noexcept {
	switch (m_kind) {
	case Kind::F32:
		return AstConst { range(), m_as_f32 };
	case Kind::F64:
		return AstConst { range(), m_as_f64 };
	case Kind::UNTYPED:
		return AstConst { range(), AstConst::UntypedReal{m_as_f64} };
	}
	BIRON_UNREACHABLE();
}

Maybe<CgValue> AstFltExpr::gen_value(Cg& cg) const noexcept {
	auto type = gen_type(cg);
	if (!type) {
		return None{};
	}
	LLVM::ValueRef v = nullptr;
	switch (m_kind) {
	case Kind::F32:
		v = cg.llvm.ConstReal(type->ref(), m_as_f32);
		break;
	case Kind::F64:
		v = cg.llvm.ConstReal(type->ref(), m_as_f64);
		break;
	case Kind::UNTYPED:
		cg.fatal(range(), "Untyped floating-point value must be typed");
		break;
	}
	if (v) {
		return CgValue { type, v };
	}
	cg.fatal(range(), "Could not generate value");
	return None{};
}

CgType* AstFltExpr::gen_type(Cg& cg) const noexcept {
	switch (m_kind) {
	case Kind::F32:
		return cg.types.f32();
	case Kind::F64:
		return cg.types.f64();
	case Kind::UNTYPED:
		cg.error(range(), "Untyped floating-point value must be typed");
		return nullptr;
	}
	BIRON_UNREACHABLE();
}

Maybe<AstConst> AstStrExpr::eval_value() const noexcept {
	return AstConst { range(), m_literal };
}

Maybe<CgValue> AstStrExpr::gen_value(Cg& cg) const noexcept {
	// When building a string we need to escape it and add a NUL terminator. Biron
	// does not have NUL terminated strings but it always adds a NUL for literals
	// so they can be safely passed to C functions expecting NUL termination.
	StringBuilder builder{*cg.scratch};
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
		return cg.oom();
	}
	auto ptr = cg.llvm.BuildGlobalString(cg.builder, builder.data(), "");
	auto len = cg.llvm.ConstInt(cg.types.u64()->ref(), m_literal.length(), false);
	LLVM::ValueRef values[2] = { ptr, len };
	auto t = gen_type(cg);
	auto v = cg.llvm.ConstNamedStruct(t->ref(), values, countof(values));
	return CgValue { t, v };
}

CgType* AstStrExpr::gen_type(Cg& cg) const noexcept {
	return cg.types.str();
}

Maybe<AstConst> AstBoolExpr::eval_value() const noexcept {
	return AstConst { range(), Bool32 { m_value } };
}

Maybe<CgValue> AstBoolExpr::gen_value(Cg& cg) const noexcept {
	auto t = gen_type(cg);
	auto v = cg.llvm.ConstInt(t->ref(), m_value ? 1 : 0, false);
	return CgValue { t, v };
}

CgType* AstBoolExpr::gen_type(Cg& cg) const noexcept {
	// LLVM has an Int1 type which can store either a 0 or 1 value which is what
	// the IR uses for "boolean" like things. We map all our Bool* types to Int1
	// except we over-align them based on the size we expect. So we just use the
	// Bool8 type for any "literal" expression.
	return cg.types.b8();
}

Maybe<AstConst> AstAggExpr::eval_value() const noexcept {
	ScratchAllocator scratch{m_exprs.allocator()};
	Array<AstConst> values{m_exprs.allocator()};
	if (!values.reserve(m_exprs.length())) {
		return None{};
	}
	auto range = m_type->range();
	for (auto expr : m_exprs) {
		auto value = expr->eval_value();
		if (!value) {
			return None{};
		}
		range = range.include(expr->range());
		if (!values.push_back(move(*value))) {
			return None{};
		}
	}
	if (m_type->is_type<AstArrayType>()) {
		return AstConst { range, AstConst::ConstArray { m_type, move(values) } };
	} else {
		return AstConst { range, AstConst::ConstTuple { m_type, move(values), None{} } };
	}
	BIRON_UNREACHABLE();
}

Maybe<CgAddr> AstAggExpr::gen_addr(Cg& cg) const noexcept {
	auto type = gen_type(cg);
	if (!type) {
		return None{};
	}

	// When emitting an aggregate we need to stack allocate because the aggregate
	// likely cannot fit into a regsiter. This means an aggregate has an address.
	auto addr = cg.emit_alloca(type);
	if (!addr) {
		return cg.oom();
	}

	Ulen count = 1;
	Bool scalar = false;
	if (type->is_array()) {
		count = type->extent();
	} else if (type->is_tuple()) {
		count = type->length();
	} else {
		scalar = true;
	}

	if (m_exprs.length() > count) {
		cg.error(range(), "Too many expressions in aggregate initializer");
		return None{};
	}

	if (m_exprs.length() == 0) {
		// No expression so zero initialize it.
		auto zero = CgValue::zero(addr->type()->deref(), cg);
		if (!zero || !addr->store(cg, *zero)) {
			return cg.oom();
		}
		return addr;
	}

	// The scalar case we just read from [0] and write to addr.
	if (scalar) {
		auto value = m_exprs[0]->gen_value(cg);
		if (!value || !addr->store(cg, *value)) {
			return None{};
		}
		return addr;
	}

	// Sequence types (tuples, arrays, etc) we have to step over every index.
	for (Ulen l = count, i = 0, j = 0; i < l; i++) {
		auto dst = addr->at(cg, i);
		if (!dst) {
			return cg.oom();
		}
		// The 'dst' type will always be a pointer so dereference.
		auto type = dst->type()->deref();
		if (type->is_padding()) {
			// Write a zeroinitializer into padding at i'th.
			auto zero = CgValue::zero(type, cg);
			if (!zero || !dst->store(cg, *zero)) {
				return cg.oom();
			}
		} else if (auto expr = m_exprs.at(j++)) {
			// Otherwise take the next expression and store it at i'th.
			auto value = (*expr)->gen_value(cg);
			if (!value) {
				return None{};
			}
			if (*value->type() != *type) {
				return None{};
			}
			if (!dst->store(cg, *value)) {
				return cg.oom();
			}
		} else {
			// No expression for that initializer so generate a zeroinitializer
			auto zero = CgValue::zero(type, cg);
			if (!zero || !dst->store(cg, *zero)) {
				return cg.oom();
			}
		}
	}
	return addr;
}

Maybe<CgValue> AstAggExpr::gen_value(Cg& cg) const noexcept {
	if (auto addr = gen_addr(cg)) {
		return addr->load(cg);
	}
	cg.fatal(range(), "Could not generate value");
	return None{};
}

CgType* AstAggExpr::gen_type(Cg& cg) const noexcept {
	return m_type->codegen(cg);
}

Maybe<AstConst> AstBinExpr::eval_value() const noexcept {
	if (m_op == Op::DOT) {
		// TODO(dweiler): See if we can work out constant tuple indexing
		return None{};
	}

	auto lhs = m_lhs->eval_value();
	if (!lhs) {
		// Not a valid compile time constant expression
		return None{};
	}

	auto rhs = m_rhs->eval_value();
	if (!rhs) {
		return None{};
	}

	// Operands to binary operator must be the same type
	if (lhs->kind() != rhs->kind()) {
		return None{};
	}

	// Generates 26 functions
	auto numeric = [&](auto f) noexcept -> Maybe<AstConst> {
		if (lhs->is_uint()) {
			return AstConst { range(), lhs->kind(), f(lhs->as_uint(), rhs->as_uint()) };
		} else if (lhs->is_sint()) {
			return AstConst { range(), lhs->kind(), f(lhs->as_sint(), rhs->as_sint()) };
		}
		return None{};
	};

	// Generates 8 functions
	auto boolean = [&](auto f) noexcept -> Maybe<AstConst> {
		if (lhs->is_bool()) {
			return AstConst { range(), lhs->kind(), f(lhs->as_bool(), rhs->as_bool()) };
		}
		return None{};
	};

	// Generates 4 functions
	auto either = [&](auto f) noexcept -> Maybe<AstConst> {
		if (auto try_numeric = numeric(f)) {
			return try_numeric;
		}
		if (auto try_boolean = boolean(f)) {
			return try_boolean;
		}
		return None{};
	};

	switch (m_op) {
	case Op::ADD:    return numeric([]<typename T>(T lhs, T rhs) -> T { return lhs + rhs; });
	case Op::SUB:    return numeric([]<typename T>(T lhs, T rhs) -> T { return lhs - rhs; });
	case Op::MUL:    return numeric([]<typename T>(T lhs, T rhs) -> T { return lhs * rhs; });
	case Op::EQ:     return either([]<typename T>(T lhs, T rhs) -> T { return lhs == rhs; });
	case Op::NE:     return either([]<typename T>(T lhs, T rhs) -> T { return lhs != rhs; });
	case Op::GT:     return numeric([]<typename T>(T lhs, T rhs) -> T { return lhs > rhs; });
	case Op::GE:     return numeric([]<typename T>(T lhs, T rhs) -> T { return lhs >= rhs; });
	case Op::LT:     return numeric([]<typename T>(T lhs, T rhs) -> T { return lhs < rhs; });
	case Op::LE:     return numeric([]<typename T>(T lhs, T rhs) -> T { return lhs <= rhs; });
	case Op::LOR:    return boolean([]<typename T>(T lhs, T rhs) -> T { return lhs || rhs; });
	case Op::LAND:   return boolean([]<typename T>(T lhs, T rhs) -> T { return lhs && rhs; });
	case Op::BOR:    return numeric([]<typename T>(T lhs, T rhs) -> T { return lhs | rhs; });
	case Op::BAND:   return numeric([]<typename T>(T lhs, T rhs) -> T { return lhs & rhs; });
	case Op::LSHIFT: return numeric([]<typename T>(T lhs, T rhs) -> T { return lhs << rhs; });
	case Op::RSHIFT: return numeric([]<typename T>(T lhs, T rhs) -> T { return lhs >> rhs; });
	default:
		return None{};
	}
	BIRON_UNREACHABLE();
}

Maybe<CgAddr> AstBinExpr::gen_addr(Cg& cg) const noexcept {
	if (m_op != Op::DOT) {
		return None{};
	}
	auto lhs_type = m_lhs->gen_type(cg);
	if (!lhs_type) {
		return None{};
	}
	auto is_tuple = lhs_type->is_tuple()
		|| (lhs_type->is_pointer() && lhs_type->deref()->is_tuple());
	if (is_tuple) {
		if (m_rhs->is_expr<AstVarExpr>()) {
			auto lhs_addr = m_lhs->gen_addr(cg);
			if (!lhs_addr) {
				return None{};
			}
			if (lhs_type->is_pointer()) {
				// Handle implicit dereference, that is:
				// 	ptr.field is sugar for (*ptr).field when ptr is a ptr
				lhs_type = lhs_type->deref();
				lhs_addr = lhs_addr->load(cg)->to_addr();
			}
			auto expr = static_cast<const AstVarExpr *>(m_rhs);
			const auto& name = expr->name();
			const auto& fields = lhs_type->fields();
			for (Ulen l = fields.length(), i = 0; i < l; i++) {
				const auto& field = fields[i];
				if (field && *field == name) {
					return lhs_addr->at(cg, i);
				}
			}
			cg.error(m_rhs->range(), "Undeclared field '%S'", name);
			return None{};
		} else if (m_rhs->is_expr<AstIntExpr>()) {
			auto rhs = m_rhs->eval_value();
			if (!rhs || !rhs->is_integral()) {
				cg.error(m_rhs->range(), "Expected integer constant expression");
				return None{};
			}
			auto index = rhs->to<Uint64>();
			if (!index) {
				cg.error(m_rhs->range(), "Expected integer constant expression");
				return None{};
			}
			auto addr = m_lhs->gen_addr(cg);
			if (!addr) {
				return None{};
			}
			return addr->at(cg, *index);
		} else {
			cg.error(m_rhs->range(), "Unknown expression");
			return None{};
		}
	}
	return None{};
}

Maybe<CgValue> AstBinExpr::gen_value(Cg& cg) const noexcept {
	using IntPredicate = LLVM::IntPredicate;
	using RealPredicate = LLVM::RealPredicate;

	CgType* lhs_type = nullptr;
	CgType* rhs_type = nullptr;

	if (m_op != Op::DOT && m_op != Op::LOR && m_op != Op::LAND && m_op != Op::AS) {
		// Operands to binary operator must be the same type
		lhs_type = m_lhs->gen_type(cg);
		rhs_type = m_rhs->gen_type(cg);
		if (!lhs_type || !rhs_type) {
			return None{};
		}
		if (*lhs_type != *rhs_type) {
			auto lhs_to_string = lhs_type->to_string(*cg.scratch);
			auto rhs_to_string = rhs_type->to_string(*cg.scratch);
			cg.error(range(),
			         "Operands to binary operator must be the same type: Got '%S' and '%S'",
			         lhs_to_string,
			         rhs_to_string);
			return None{};
		}
	}

	// Special behavior needed for 'as'
	if (m_op == Op::AS) {
		auto lhs = m_lhs->gen_value(cg);
		if (!lhs) {
			return None{};
		}

		auto rhs = gen_type(cg);
		if (!rhs) {
			return None{};
		}

		auto lhs_is_signed = lhs->type()->is_sint();
		auto rhs_is_signed = rhs->is_sint();

		auto cast_op =
			cg.llvm.GetCastOpcode(lhs->ref(),
			                      lhs_is_signed,
			                      rhs->ref(),
			                      rhs_is_signed);

		auto value = cg.llvm.BuildCast(cg.builder, cast_op, lhs->ref(), rhs->ref(), "");
		return CgValue { rhs, value };
	}

	Maybe<CgValue> lhs;
	Maybe<CgValue> rhs;
	auto gen_values = [&]() -> Bool {
		lhs = m_lhs->gen_value(cg);
		rhs = m_rhs->gen_value(cg);
		return lhs && rhs;
	};

	switch (m_op) {
	case Op::ADD:
		if (gen_values()) {
			return cg.emit_add(*lhs, *rhs, range());
		}
		break;
	case Op::SUB:
		if (gen_values()) {
			return cg.emit_sub(*lhs, *rhs, range());
		}
		break;
	case Op::MUL:
		if (gen_values()) {
			return cg.emit_mul(*lhs, *rhs, range());
		}
		break;
	case Op::DIV:
		if (gen_values()) {
			return cg.emit_div(*lhs, *rhs, range());
		}
		break;
	case Op::MIN:
		if (gen_values()) {
			return cg.emit_min(*lhs, *rhs, range());
		}
		break;
	case Op::MAX:
		if (gen_values()) {
			return cg.emit_max(*lhs, *rhs, range());
		}
		break;
	case Op::EQ:
		if (lhs_type->is_sint() || lhs_type->is_uint() || lhs_type->is_pointer()) {
			if (gen_values()) {
				auto value = cg.llvm.BuildICmp(cg.builder, IntPredicate::EQ, lhs->ref(), rhs->ref(), "");
				return CgValue { cg.types.b32(), value };
			}
		} else if (lhs_type->is_real()) {
			if (gen_values()) {
				auto value = cg.llvm.BuildFCmp(cg.builder, RealPredicate::OEQ, lhs->ref(), rhs->ref(), "");
				return CgValue { cg.types.b32(), value };
			}
		} else {
			auto intrinsic = cg.intrinsic("memory_eq");
			if (!intrinsic) {
				cg.fatal(range(), "Could not find 'memory_eq' intrinsic");
				return None{};
			}
			auto lhs_dst = m_lhs->gen_addr(cg);
			auto rhs_dst = m_rhs->gen_addr(cg);
			LLVM::ValueRef args[] = {
				lhs_dst->ref(),
				rhs_dst->ref(),
				cg.llvm.ConstInt(cg.types.u64()->ref(), lhs_type->size(), false),
			};
			auto call = cg.llvm.BuildCall2(cg.builder,
			                               intrinsic->type()->deref()->ref(),
			                               intrinsic->ref(),
			                               args,
			                               countof(args),
			                               "");
			return CgValue { cg.types.b32(), call };
		}
		break;
	case Op::NE:
		if (lhs_type->is_sint() || lhs_type->is_uint() || lhs_type->is_pointer()) {
			if (gen_values()) {
				auto value = cg.llvm.BuildICmp(cg.builder, IntPredicate::NE, lhs->ref(), rhs->ref(), "");
				return CgValue { cg.types.b32(), value };
			}
		} else if (lhs_type->is_real()) {
			if (gen_values()) {
				auto value = cg.llvm.BuildFCmp(cg.builder, RealPredicate::ONE, lhs->ref(), rhs->ref(), "");
				return CgValue { cg.types.b32(), value };
			}
		} else {
			auto intrinsic = cg.intrinsic("memory_ne");
			if (!intrinsic) {
				cg.fatal(range(), "Could not find 'memory_ne' intrinsic");
				return None{};
			}
			auto lhs_dst = m_lhs->gen_addr(cg);
			auto rhs_dst = m_rhs->gen_addr(cg);
			LLVM::ValueRef args[] = {
				lhs_dst->ref(),
				rhs_dst->ref(),
				cg.llvm.ConstInt(cg.types.u64()->ref(), lhs_type->size(), false),
			};
			auto call = cg.llvm.BuildCall2(cg.builder,
			                               intrinsic->type()->deref()->ref(),
			                               intrinsic->ref(),
			                               args,
			                               countof(args), "");

			return CgValue { cg.types.b32(), call };
		}
		break;
	case Op::GT:
		if (gen_values()){
			return cg.emit_gt(*lhs, *rhs, range());
		}
		break;
	case Op::GE:
		if (gen_values()) {
			return cg.emit_ge(*lhs, *rhs, range());
		}
		break;
	case Op::LT:
		if (gen_values()) {
			return cg.emit_lt(*lhs, *rhs, range());
		}
		break;
	case Op::LE:
		if (gen_values()) {
			return cg.emit_le(*lhs, *rhs, range());
		}
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
				auto lhs_type_string = lhs->type()->to_string(*cg.scratch);
				cg.error(m_lhs->range(),
				         "Operands to '||' operator must have boolean type. Got '%S' instead",
				         lhs_type_string);
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
				cg.llvm.ConstInt(cg.types.b32()->ref(), 1, false),
				cg.llvm.ConstInt(cg.types.b32()->ref(), 1, false),
				cg.llvm.ConstInt(cg.types.b32()->ref(), 0, false),
			};
			
			auto phi = cg.llvm.BuildPhi(cg.builder, cg.types.b32()->ref(), "");

			cg.llvm.AddIncoming(phi, values, blocks, countof(blocks));

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
				auto lhs_type_string = lhs->type()->to_string(*cg.scratch);
				cg.error(m_lhs->range(),
				         "Operands to '&&' operator must have boolean type. Got '%S' instead",
				         lhs_type_string);
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
				cg.llvm.ConstInt(cg.types.b32()->ref(), 0, false),
				cg.llvm.ConstInt(cg.types.b32()->ref(), 1, false),
				cg.llvm.ConstInt(cg.types.b32()->ref(), 0, false),
			};
			
			auto phi = cg.llvm.BuildPhi(cg.builder, cg.types.b32()->ref(), "");

			cg.llvm.AddIncoming(phi, values, blocks, countof(blocks));

			return CgValue { cg.types.b32(), phi };
		}
		break;
	case Op::BOR:
		if (lhs_type->is_sint() || lhs_type->is_uint() || lhs_type->is_bool()) {
			if (gen_values()) {
				auto value = cg.llvm.BuildOr(cg.builder, lhs->ref(), rhs->ref(), "");
				return CgValue { lhs_type, value };
			}
		} else {
			auto lhs_type_string = lhs_type->to_string(*cg.scratch);
			cg.error(range(),
			         "Operands to '|' operator must have integer or boolean type. Got '%S' instead",
			         lhs_type_string);
			return None{};
		}
		break;
	case Op::BAND:
		if (lhs_type->is_sint() || lhs_type->is_uint() || lhs_type->is_bool()) {
			if (gen_values()) {
				auto value = cg.llvm.BuildAnd(cg.builder, lhs->ref(), rhs->ref(), "");
				return CgValue { lhs_type, value };
			}
		} else {
			auto lhs_type_string = lhs_type->to_string(*cg.scratch);
			cg.error(range(),
			         "Operands to '&' operator must have integer or boolean type. Got '%S' instead",
			         lhs_type_string);
			return None{};
		}
		break;
	case Op::LSHIFT:
		if (lhs_type->is_sint() || lhs_type->is_uint()) {
			if (gen_values()) {
				auto value = cg.llvm.BuildShl(cg.builder, lhs->ref(), rhs->ref(), "");
				return CgValue { lhs_type, value };
			}
		} else {
			auto lhs_type_string = lhs_type->to_string(*cg.scratch);
			cg.error(range(),
			         "Operands to '<<' operator must have integer type. Got '%S' instead",
			         lhs_type_string);
			return None{};
		}
		break;
	case Op::RSHIFT:
		if (gen_values()) {
			if (lhs_type->is_sint()) {
				auto value = cg.llvm.BuildAShr(cg.builder, lhs->ref(), rhs->ref(), "");
				return CgValue { lhs_type, value };
			} else if (lhs_type->is_uint()) {
				auto value = cg.llvm.BuildLShr(cg.builder, lhs->ref(), rhs->ref(), "");
				return CgValue { lhs_type, value };
			} else {
				auto lhs_type_string = lhs_type->to_string(*cg.scratch);
				cg.error(range(),
				         "Operands to '>>' operator must have integer type. Got '%S' instead",
				         lhs_type_string);
				return None{};
			}
		}
		break;
	case Op::DOT:
		{
			auto lhs_type = m_lhs->gen_type(cg);
			if (!lhs_type) {
				return None{};
			}
			auto is_tuple =
				lhs_type->is_tuple() || (lhs_type->is_pointer() && lhs_type->deref()->is_tuple());
			if (!is_tuple) {
				cg.error(m_lhs->range(), "Expression is not a tuple");
				return None{};
			}
			if (m_rhs->is_expr<AstCallExpr>()) {
				auto rhs = static_cast<AstCallExpr *>(m_rhs);
				// Generate the left-hand side tuple containing all the values.
				auto objs = m_lhs->gen_addr(cg);
				if (!objs) {
					return None{};
				}
				if (objs->type()->deref()->is_pointer()) {
					// Allow (&obj).method() where method takes *T
					objs = objs->load(cg)->to_addr();
				}
				// Generate the prepended values for the call.
				Array<CgValue> values{*cg.scratch};
				auto rhs_type = rhs->callee()->gen_type(cg);
				if (!rhs_type) {
					return None{};
				}
				if (!rhs_type->is_fn()) {
					auto rhs_type_string = rhs_type->to_string(*cg.scratch);
					cg.error(rhs->range(), "Expected function type. Got '%S' instead", rhs_type_string);
					return None{};
				}
				// Fn: type 0 = objs
				//     type 1 = args
				//     type 2 = rets
				auto objs_type = rhs_type->at(0);
				// Specialization when only a single object as objs is the single
				// object (i.e untupled tupled).
				if (objs_type->length() == 1) {
					if (objs_type->at(0)->is_pointer()) {
						// Reciever wants it passed as pointer.
						if (!values.emplace_back(objs_type, objs->ref())) {
							return cg.oom();
						}
					} else {
						// Otherwise reciever wants it passed by copy so do a load.
						auto value = objs->load(cg);
						// Implicit dereference behavior for when a copy is wanted.
						if (value->type()->is_pointer()) {
							value = value->to_addr().load(cg);
						}
						if (!value) {
							return None{};
						}
						if (!values.push_back(*value)) {
							return cg.oom();
						}
					}
				} else {
					for (Ulen l = objs_type->length(), i = 0; i < l; i++) {
						auto type = objs_type->at(i);
						auto addr = objs->at(cg, i);
						if (type->is_pointer()) {
							// Reciever wants it passed as pointer.
							if (!values.emplace_back(type, addr->ref())) {
								return cg.oom();
							}
						} else if (!type->is_padding()) {
							// Otherwise reciever wants it passed by copy so do a load.
							auto value = addr->load(cg);
							if (value->type()->is_pointer()) {
								// Implicit dereference behavior for when a copy is wanted.
								value = value->to_addr().load(cg);
							}
							if (!value) {
								return None{};
							}
							if (!values.push_back(*value)) {
								return cg.oom();
							}
						}
					}
				}
				return rhs->gen_value(move(values), cg);
			} else if (m_rhs->is_expr<AstVarExpr>()) {
				auto addr = gen_addr(cg);
				if (!addr) {
					return None{};
				}
				return addr->load(cg);
			} else if (m_rhs->is_expr<AstIntExpr>()) {
				auto addr = gen_addr(cg);
				if (!addr) {
					return None{};
				}
				return addr->load(cg);
			}
		}
		break;
	default:
		break;
	}

	cg.fatal(range(), "Could not generate value");
	return None{};
}

CgType* AstBinExpr::gen_type(Cg& cg) const noexcept {
	switch (m_op) {
	case Op::ADD: case Op::SUB: case Op::MUL: case Op::DIV:
		return m_lhs->gen_type(cg);
	case Op::EQ: case Op::NE: case Op::GT: case Op::GE: case Op::LT: case Op::LE:
		return cg.types.b8();
	case Op::MIN: case Op::MAX:
		return m_lhs->gen_type(cg);
	case Op::LOR: case Op::LAND:
		return cg.types.b8();
	case Op::BOR: case Op::BAND:
		return m_lhs->gen_type(cg);
	case Op::LSHIFT: case Op::RSHIFT:
		return m_lhs->gen_type(cg);
	case Op::AS:
		if (m_rhs->is_expr<AstTypeExpr>()) {
			return static_cast<AstTypeExpr*>(m_rhs)->type()->codegen(cg);
		} else {
			cg.error(m_rhs->range(), "Expected type expression on right-hand side of 'as' operator");
			return nullptr;
		}
		break;
	case Op::DOT:
		{
			auto lhs_type = m_lhs->gen_type(cg);
			if (!lhs_type) {
				return nullptr;
			}
			if (lhs_type->is_pointer()) {
				lhs_type = lhs_type->deref();
			}
			if (m_rhs->is_expr<AstCallExpr>()) {
				return m_rhs->gen_type(cg);
			} else if (m_rhs->is_expr<AstVarExpr>()) {
				auto rhs = static_cast<const AstVarExpr *>(m_rhs);
				Ulen i = 0;
				for (const auto& field : lhs_type->fields()) {
					if (field && *field == rhs->name()) {
						return lhs_type->at(i);
					}
					i++;
				}
				cg.error(m_rhs->range(), "Undeclared field '%S'", rhs->name());
				return nullptr;
			} else if (m_rhs->is_expr<AstIntExpr>()) {
				auto i = m_rhs->eval_value();
				if (!i || !i->is_integral()) {
					cg.error(m_rhs->range(), "Not a valid integer constant expression");
					return nullptr;
				}
				return lhs_type->at(*i->to<Uint64>());
			}
		}
		break;
	case Op::OF:
		// The OF operator always returns some integer constant expression except
		// for "type of"
		//	size of expr
		//	align of expr
		//	count of expr
		//	offset of expr
		return cg.types.u64();
	}
	cg.fatal(range(), "Could not generate type");
	return nullptr;
}

Maybe<CgAddr> AstUnaryExpr::gen_addr(Cg& cg) const noexcept {
	switch (m_op) {
	case Op::NEG:
		cg.error(range(), "Cannot take the address of an rvalue");
		return None{};
	case Op::NOT:
		cg.error(range(), "Cannot take the address of an rvalue");
		return None{};
	case Op::DEREF:
		// When dereferencing on the LHS we will have a R-value of the address 
		// which we just turn back into an address.
		//
		// The LHS dereference does not actually load or dereference anything, it's
		// used in combination with an AssignStmt to allow storing results through
		// the pointer.
		if (auto operand = m_operand->gen_value(cg)) {
			if (!operand->type()->is_pointer()) {
				auto operand_type_string = operand->type()->to_string(*cg.scratch);
				cg.error(m_operand->range(),
				         "Operand to '*' must have pointer type. Got '%S' instead",
				         operand_type_string);
				return None{};
			}
			return operand->to_addr();
		}
		break;
	case Op::ADDROF:
		cg.error(range(), "Cannot take the address of an rvalue");
		break;
	}
	BIRON_UNREACHABLE();
}

Maybe<CgValue> AstUnaryExpr::gen_value(Cg& cg) const noexcept {
	switch (m_op) {
	case Op::NEG:
		if (auto operand = m_operand->gen_value(cg)) {
			if (operand->type()->is_real()) {
				return CgValue { operand->type(), cg.llvm.BuildFNeg(cg.builder, operand->ref(), "") };
			} else {
				return CgValue { operand->type(), cg.llvm.BuildNeg(cg.builder, operand->ref(), "") };
			}
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
			return operand->to_value();
		}
		break;
	}
	cg.fatal(m_operand->range(), "Could not generate value");
	return None{};
}

CgType* AstUnaryExpr::gen_type(Cg& cg) const noexcept {
	auto type = m_operand->gen_type(cg);
	if (!type) {
		return nullptr;
	}
	switch (m_op) {
	case Op::NEG: case Op::NOT:
		return type;
	case Op::DEREF:
		return type->deref();
	case Op::ADDROF:
		return type->addrof(cg);
	}
	BIRON_UNREACHABLE();
}

Maybe<CgAddr> AstIndexExpr::gen_addr(Cg& cg) const noexcept {
	auto operand = m_operand->gen_addr(cg);
	if (!operand) {
		cg.fatal(m_operand->range(), "Could not generate operand");
		return None{};
	}

	// Optimization for constant integer expression indexing.
	if (auto eval = m_index->eval_value()) {
		if (!eval->is_integral()) {
			cg.error(eval->range(), "Cannot index with a constant expression of non-integer type");
			return None{};
		}
		auto index = eval->to<Uint64>();
		if (auto addr = operand->at(cg, *index)) {
			return addr;
		}
	} else {
		// Otherwise runtime indexing
		auto index = m_index->gen_value(cg);
		if (!index) {
			cg.fatal(m_index->range(), "Could not generate index");
			return None{};
		}
		if (!index->type()->is_uint() && !index->type()->is_sint()) {
			auto index_type_string = index->type()->to_string(*cg.scratch);
			cg.error(m_index->range(),
			         "Expected expression of integer type for index. Got '%S' instead",
			         index_type_string);
			return None{};
		}
		if (auto addr = operand->at(cg, *index)) {
			return addr;
		}
	}
	cg.fatal(range(), "Could not generate value");
	return None{};
}

Maybe<CgValue> AstIndexExpr::gen_value(Cg& cg) const noexcept {
	if (auto addr = gen_addr(cg)) {
		return addr->load(cg);
	}
	cg.fatal(range(), "Could not generate value");
	return None{};
}

CgType* AstIndexExpr::gen_type(Cg& cg) const noexcept {
	auto type = m_operand->gen_type(cg);
	if (!type) {
		return nullptr;
	}
	if (!type->is_pointer() && !type->is_array() && !type->is_slice()) {
		auto type_string = type->to_string(*cg.scratch);
		cg.error(range(),
		         "Cannot index expression of type '%S'",
		         type_string);
		return nullptr;
	}
	return type->deref();
}

Maybe<AstConst> AstIndexExpr::eval_value() const noexcept {
	auto operand = m_operand->eval_value();
	if (!operand) {
		return None{};
	}
	auto index = m_index->eval_value();
	if (!index) {
		return None{};
	}
	auto i = index->to<Uint64>();
	if (!i) {
		return None{};
	}
	if (operand->is_tuple()) {
		if (auto value = operand->as_tuple().values.at(*i)) {
			return value->copy();
		}
	} else if (operand->is_array()) {
		if (auto value = index->as_array().elems.at(*i)) {
			return value->copy();
		}
	}
	return None{};
}

Maybe<CgValue> AstExplodeExpr::gen_value(Cg& cg) const noexcept {
	return m_operand->gen_value(cg);
}

} // namespace Biron