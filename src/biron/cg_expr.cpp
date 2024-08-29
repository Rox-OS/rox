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

Maybe<AstConst> AstExpr::eval() const noexcept {
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

Maybe<AstConst> AstTupleExpr::eval() const noexcept {
	Array<AstConst> values{m_exprs.allocator()};
	Range range{0, 0};
	for (const auto& expr : m_exprs) {
		range.include(expr->range());
		auto value = expr->eval();
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
		cg.fatal(range(), "Could not generate type");
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
	Array<CgValue> values{cg.allocator};
	for (Ulen l = length(), i = 0; i < l; i++) {
		auto value = at(i)->gen_value(cg);
		if (!value || !values.push_back(move(*value))) {
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

	Array<CgType*> types{cg.allocator};
	if (!types.reserve(length())) {
		cg.oom();
		return nullptr;
	}
	for (Ulen l = length(), i = 0; i < l; i++) {
		auto type = at(i)->gen_type(cg);
		if (!type || !types.push_back(type)) {
			cg.oom();
			return nullptr;
		}
	}

	return cg.types.make(CgType::TupleInfo { move(types), None{} });
}

Maybe<CgValue> AstCallExpr::gen_value(Cg& cg) const noexcept {
	auto callee = m_callee->gen_addr(cg);
	if (!callee) {
		return None{};
	}

	ScratchAllocator scratch{cg.allocator};
	Array<LLVM::ValueRef> values{scratch};
	if (!values.reserve(m_args->length())) {
		return cg.oom();
	}
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

	if (!value) {
		return cg.oom();
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
	// Search module for globals
	for (const auto& global : cg.globals) {
		if (global.name() == m_name) {
			return global.addr();
		}
	}

	cg.error(range(), "Could not find symbol '%.*s'", Sint32(m_name.length()), m_name.data());

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
	auto type = gen_type(cg);
	if (!type) {
		return None{};
	}
	LLVM::ValueRef v = nullptr;
	switch (m_kind) {
	/****/ case Kind::U8:  v = cg.llvm.ConstInt(type->ref(), m_as_u8, false);
	break; case Kind::U16: v = cg.llvm.ConstInt(type->ref(), m_as_u16, false);
	break; case Kind::U32: v = cg.llvm.ConstInt(type->ref(), m_as_u32, false);
	break; case Kind::U64: v = cg.llvm.ConstInt(type->ref(), m_as_u64, false);
	break; case Kind::S8:  v = cg.llvm.ConstInt(type->ref(), m_as_s8, true);
	break; case Kind::S16: v = cg.llvm.ConstInt(type->ref(), m_as_s16, true);
	break; case Kind::S32: v = cg.llvm.ConstInt(type->ref(), m_as_s32, true);
	break; case Kind::S64: v = cg.llvm.ConstInt(type->ref(), m_as_s64, true);
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
	}
	return nullptr;
}

Maybe<AstConst> AstFltExpr::eval() const noexcept {
	switch (m_kind) {
	case Kind::F32: return AstConst { range(), m_as_f32 };
	case Kind::F64: return AstConst { range(), m_as_f64 };
	}
	return None{};
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
	}
	if (v) {
		return CgValue { type, v };
	}
	cg.fatal(range(), "Could not generate value");
	return None{};
}

CgType* AstFltExpr::gen_type(Cg& cg) const noexcept {
	switch (m_kind) {
	case Kind::F32: return cg.types.f32();
	case Kind::F64: return cg.types.f64();
	}
	return nullptr;
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
		return cg.oom();
	}
	auto ptr = cg.llvm.BuildGlobalString(cg.builder, builder.data(), "");
	auto len = cg.llvm.ConstInt(cg.types.u64()->ref(), m_literal.length(), false);
	if (!ptr || !len) {
		return cg.oom();
	}
	LLVM::ValueRef values[2] = { ptr, len };
	auto t = gen_type(cg);
	auto v = cg.llvm.ConstNamedStruct(t->ref(), values, countof(values));
	return CgValue { t, v };
}

CgType* AstStrExpr::gen_type(Cg& cg) const noexcept {
	return cg.types.str();
}

Maybe<AstConst> AstBoolExpr::eval() const noexcept {
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

Maybe<AstConst> AstAggExpr::eval() const noexcept {
	ScratchAllocator scratch{m_exprs.allocator()};
	Array<AstConst> values{m_exprs.allocator()};
	if (!values.reserve(m_exprs.length())) {
		return None{};
	}
	auto range = m_type->range();
	for (auto expr : m_exprs) {
		auto value = expr->eval();
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
	ScratchAllocator scratch{cg.allocator};

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

	auto count = type->is_array() ? type->extent() : type->length();

	if (count == 0) {
		// When the type is a scalar.
		if (auto length = m_exprs.length()) {
			if (length > 1) {
				cg.error(range(), "Too many expressions in aggregate initializer");
				return None{};
			}
			// We have an expression to initialize it with.
			auto value = m_exprs[0]->gen_value(cg);
			if (!value || !addr->store(cg, *value)) {
				return cg.oom();
			}
		} else {
			// No expression so zero initialize it.
			auto zero = CgValue::zero(addr->type()->deref(), cg);
			if (!zero || !addr->store(cg, *zero)) {
				return cg.oom();
			}
		}
		return addr;
	}

	if (m_exprs.length() > count) {
		cg.error(range(), "Too many expressions in aggregate initializer");
		return None{};
	}

	// We now actually go over every index in the type.
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
				StringBuilder b0{scratch};
				StringBuilder b1{scratch};
				type->dump(b0);
				value->type()->dump(b1);
				cg.error((*expr)->range(), "Expected expression of type '%.*s'. Got '%.*s' instead",
				         Sint32(b0.length()), b0.data(),
				         Sint32(b1.length()), b1.data());
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

Maybe<AstConst> AstBinExpr::eval() const noexcept {
	if (m_op == Op::DOT) {
		// TODO(dweiler): See if we can work out constant tuple indexing
		fprintf(stderr, "Cannot index tuple constantly!\n");
		return None{};
	}

	auto lhs = m_lhs->eval();
	if (!lhs) {
		// Not a valid compile time constant expression
		return None{};
	}

	auto rhs = m_rhs->eval();
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

	auto lhs = m_lhs->gen_addr(cg);
	if (!lhs) {
		cg.fatal(m_lhs->range(), "Could not generate address");
		return None{};
	}

	auto type = lhs->type()->deref();
	Bool ptr = false;
	if (type->is_pointer()) {
		lhs = lhs->load(cg)->to_addr();
		ptr = true;
		// We do the implicit dereference behavior so that we do not need a
		// '->' operator like C and C++ here.
		type = type->deref();
	}
	if (type->is_tuple()) {
		// When the right hand side is an AstCallExpr we're performing method
		// dispatch. We support multimethods / multiple dispatch with this method
		// too since it's just a tuple of multiple things.
		if (m_rhs->is_expr<AstCallExpr>()) {
			cg.error(range(), "Unimplemented method call");
		} else {
			// When the right hand side is an AstVarExpr it means we're indexing the
			// tuple by field name.
			if (m_rhs->is_expr<AstVarExpr>()) {
				const auto expr = static_cast<const AstVarExpr *>(m_rhs);
				const auto& fields = type->fields();
				for (Ulen l = fields.length(), i = 0; i < l; i++) {
					const auto& field = fields[i];
					if (field && *field == expr->name()) {
						return lhs->at(cg, i);
					}
				}
				cg.error(m_rhs->range(), "Undeclared field '%.*s'", Sint32(expr->name().length()), expr->name().data());
				return None{};
			} else if (m_rhs->is_expr<AstIntExpr>()) {
				auto rhs = m_rhs->eval();
				if (!rhs) {
					cg.error(m_rhs->range(), "Expected constant integer expression for indexing tuple");
					return None{};
				}
				// We support an integer constant expression inside a tuple
				Maybe<AstConst> index;
				if (rhs->is_tuple() && rhs->as_tuple().values.length() == 1) {
					index = rhs->as_tuple().values[0].copy();
				} else if (rhs->is_integral()) {
					index = rhs->copy();
				}
				if (!index || !index->is_integral()) {
					cg.error(rhs->range(), "Expected constant integer expression for indexing tuple");
					return None{};
				}
				// TODO(dweiler): We need to map logical index to physical tuple index...
				auto n = index->to<Uint64>();
				if (!n) {
					cg.error(rhs->range(), "Expected integer constant expression for indexing tuple");
					return None{};
				}
				if (type->at(*n)->is_padding()) {
					(*n)++;
				}
				return lhs->at(cg, *n);
			} else {
				cg.error(m_rhs->range(), "Cannot index tuple with array indexing");
				return None{};
			}
		}
	} else {
		StringBuilder s{cg.allocator};
		type->dump(s);
		cg.error(range(),
		         "Operand to '.' operator must be of tuple type. Got '%s' instead",
		         Sint32(s.length()),
		         s.data());
		return None{};
	}

	// TODO(dweiler): Work out the field index by the structure name

	if (ptr) {
		if (auto load = lhs->load(cg)) {
			return load->to_addr().at(cg, 0);
		}
	}

	return lhs->at(cg, 0);
}

Maybe<CgValue> AstBinExpr::gen_value(Cg& cg) const noexcept {
	using IntPredicate = LLVM::IntPredicate;

	Maybe<CgValue> lhs;
	Maybe<CgValue> rhs;
	if (m_op != Op::DOT && m_op != Op::LOR && m_op != Op::LAND && m_op != Op::AS) {
		lhs = m_lhs->gen_value(cg);
		rhs = m_rhs->gen_value(cg);
		if (!lhs) {
			cg.fatal(m_lhs->range(), "Could not generate LHS binary (%s) operand", m_lhs->name());
			return None{};
		}
		if (!rhs) {
			cg.fatal(m_rhs->range(), "Could not generate RHS binary (%s) operand", m_rhs->name());
			return None{};
		}
		// Operands to binary operator must be the same type
		if (*lhs->type() != *rhs->type()) {
			StringBuilder lhs_to_string{cg.allocator};
			lhs->type()->dump(lhs_to_string);
			StringBuilder rhs_to_string{cg.allocator};
			rhs->type()->dump(rhs_to_string);
			cg.error(range(),
			         "Operands to binary operator must be the same type: Got '%s' and '%s'",
			         Sint32(lhs_to_string.length()), lhs_to_string.data(),
			         Sint32(rhs_to_string.length()), rhs_to_string.data());
			return None{};
		}
	}

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

		auto v = cg.llvm.BuildCast(cg.builder, cast_op, lhs->ref(), rhs->ref(), "");
		if (!v) {
			return cg.oom();
		}

		return CgValue { rhs, v };
	}

	switch (m_op) {
	case Op::ADD:
		if (lhs->type()->is_real()) {
			return CgValue { lhs->type(), cg.llvm.BuildFAdd(cg.builder, lhs->ref(), rhs->ref(), "") };
		} else {
			return CgValue { lhs->type(), cg.llvm.BuildAdd(cg.builder, lhs->ref(), rhs->ref(), "") };
		}
	case Op::SUB:
		if (lhs->type()->is_real()) {
			return CgValue { lhs->type(), cg.llvm.BuildFSub(cg.builder, lhs->ref(), rhs->ref(), "") };
		} else {
			return CgValue { lhs->type(), cg.llvm.BuildSub(cg.builder, lhs->ref(), rhs->ref(), "") };
		}
	case Op::MUL:
		if (lhs->type()->is_real()) {
			return CgValue { lhs->type(), cg.llvm.BuildFMul(cg.builder, lhs->ref(), rhs->ref(), "") };
		} else {
			return CgValue { lhs->type(), cg.llvm.BuildMul(cg.builder, lhs->ref(), rhs->ref(), "") };
		}
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
			// on_lhs_false = cg.llvm.GetInsertBlock(cg.builder);
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
			if (!phi) {
				return cg.oom();
			}

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
			if (!phi) {
				return cg.oom();
			}

			cg.llvm.AddIncoming(phi, values, blocks, countof(blocks));

			return CgValue { cg.types.b32(), phi };
		}
		break;
	case Op::BOR:
		return CgValue { lhs->type(), cg.llvm.BuildOr(cg.builder, lhs->ref(), rhs->ref(), "") };
	case Op::BAND:
		return CgValue { lhs->type(), cg.llvm.BuildAnd(cg.builder, lhs->ref(), rhs->ref(), "") };
	case Op::LSHIFT:
		if (lhs->type()->is_sint() || lhs->type()->is_uint()) {
			return CgValue { lhs->type(), cg.llvm.BuildShl(cg.builder, lhs->ref(), rhs->ref(), "") };
		} else {
			StringBuilder s{cg.allocator};
			lhs->type()->dump(s);
			cg.error(range(),
			         "Operands to '<<' must have integer type. Got '%.*s' instead",
			          Sint32(s.length()),
			          s.data());
			return None{};
		}
	case Op::RSHIFT:
		if (lhs->type()->is_sint()) {
			return CgValue { lhs->type(), cg.llvm.BuildAShr(cg.builder, lhs->ref(), rhs->ref(), "") };
		} else if (lhs->type()->is_uint()) {
			return CgValue { lhs->type(), cg.llvm.BuildLShr(cg.builder, lhs->ref(), rhs->ref(), "") };
		} else {
			StringBuilder s{cg.allocator};
			lhs->type()->dump(s);
			cg.error(range(),
			         "Operands to '>>' must have integer type. Got '%.*s' instead",
			         Sint32(s.length()),
			         s.data());
			return None{};
		}
		break;
	case Op::DOT:
		if (auto addr = gen_addr(cg)) {
			return addr->load(cg);
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
	case Op::ADD: case Op::SUB: case Op::MUL:
		return m_lhs->gen_type(cg);
	case Op::EQ: case Op::NE:  case Op::GT: case Op::GE: case Op::LT: case Op::LE:
		return cg.types.b8();
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
			if (m_rhs->is_expr<AstVarExpr>()) {
				auto rhs = static_cast<const AstVarExpr *>(m_rhs);
				Ulen i = 0;
				for (const auto& field : lhs_type->fields()) {
					if (field && *field == rhs->name()) {
						return lhs_type->at(i);
					}
					i++;
				}
				cg.error(m_rhs->range(), "Undeclared field '%.*s'", Sint32(rhs->name().length()), rhs->name().data());
			} else if (m_rhs->is_expr<AstIntExpr>()) {
				auto i = m_rhs->eval();
				if (!i) {
					cg.error(m_rhs->range(), "Not a valid constant expression");
					return nullptr;
				}
				auto index = i->to<Uint64>();
				if (!index) {
					cg.error(i->range(), "Not a valid integer constant expression");
					return nullptr;
				}
				return lhs_type->at(*index);
			}
		}
		break;
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
				StringBuilder builder{cg.allocator};
				operand->type()->dump(builder);
				cg.error(m_operand->range(),
				         "Operand to '*' must have pointer type. Got '%.*s' instead",
				         Sint32(builder.length()),
				         builder.data());
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
		return None{};
	}

	auto type = operand->type()->deref();
	if (!type->is_pointer() && !type->is_array() && !type->is_slice()) {
		StringBuilder builder{cg.allocator};
		type->dump(builder);
		cg.error(range(),
		         "Cannot index expression of type '%.*s'",
		         Sint32(builder.length()),
		         builder.data());
		return None{};
	}

	auto index = m_index->gen_value(cg);
	if (!index) {
		return None{};
	}

	if (!index->type()->is_uint() && !index->type()->is_sint()) {
		StringBuilder b0{cg.allocator};
		StringBuilder b1{cg.allocator};
		index->type()->dump(b0);
		type->dump(b1);
		cg.error(m_index->range(),
		         "Value of type '%.*s' cannot be used to index '%.*s'",
		         Sint32(b0.length()), b0.data(),
		         Sint32(b1.length()), b1.data());
		return None{};
	}

	if (auto addr = operand->at(cg, *index)) {
		return addr;
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

Maybe<AstConst> AstIndexExpr::eval() const noexcept {
	auto operand = m_operand->eval();
	if (!operand) {
		return None{};
	}
	auto index = m_index->eval();
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

} // namespace Biron