#include <biron/cg.h>
#include <biron/cg_value.h>

#include <biron/ast_expr.h>
#include <biron/ast_type.h>
#include <biron/ast_const.h>
#include <biron/ast_unit.h>

#include <biron/util/unreachable.inl>

namespace Biron {

Maybe<CgAddr> AstExpr::gen_addr(Cg& cg, CgType*) const noexcept {
	return cg.fatal(range(), "Unsupported gen_addr for %s", name());
}

Maybe<CgValue> AstExpr::gen_value(Cg& cg, CgType*) const noexcept {
	return cg.fatal(range(), "Unsupported gen_value for %s", name());
}

CgType* AstExpr::gen_type(Cg& cg, CgType*) const noexcept {
	return cg.fatal(range(), "Unsupported gen_type for %s", name());
}

Maybe<AstConst> AstExpr::eval_value(Cg&) const noexcept {
	return None{};
}

static AstExpr* detuple(AstExpr* expr) noexcept {
	if (auto tuple = expr->to_expr<AstTupleExpr>(); tuple && tuple->length() == 1) {
		return tuple->at(0);
	}
	return expr;
}

const char* AstExpr::name() const noexcept {
	switch (m_kind) {
	case Kind::TUPLE:     return "TUPLE";
	case Kind::CALL:      return "CALL";
	case Kind::TYPE:      return "TYPE";
	case Kind::VAR:       return "VAR";
	case Kind::INT:       return "INT";
	case Kind::FLT:       return "FLT";
	case Kind::BOOL:      return "BOOL";
	case Kind::STR:       return "STR";
	case Kind::AGG:       return "AGG";
	case Kind::BIN:       return "BIN";
	case Kind::LBIN:      return "LBIN";
	case Kind::UNARY:     return "UNARY";
	case Kind::INDEX:     return "INDEX";
	case Kind::EXPLODE:   return "EXPLODE";
	case Kind::EFF:       return "EFF";
	case Kind::SELECTOR:  return "SELECTOR";
	case Kind::INFERSIZE: return "INFERSIZE";
	case Kind::ACCESS:    return "ACCESS";
	case Kind::CAST:      return "CAST";
	case Kind::TEST:      return "TEST";
	case Kind::PROP:      return "PROP";
	}
	BIRON_UNREACHABLE();
}

Maybe<AstConst> AstTupleExpr::eval_value(Cg& cg) const noexcept {
	Array<AstConst> values{m_exprs.allocator()};
	Maybe<Range> range;
	for (const auto& expr : m_exprs) {
		if (range) {
			range = range->include(expr->range());
		} else {
			range.emplace(expr->range());
		}
		auto value = expr->eval_value(cg);
		if (!value) {
			return None{};
		}
		if (!values.push_back(move(*value))) {
			return cg.oom();
		}
	}

	// Should we infer the type for ConstTuple here? A compile-time constant tuple
	// cannot have fields and cannot be indexed any other way than with integers,
	// for which a type is not needed. However, it might be useful to have a type
	// anyways. Revisit this later. As of now we just pass nullptr.
	return AstConst { *range, AstConst::ConstTuple { nullptr, move(values), None{} } };
}

Maybe<CgAddr> AstTupleExpr::gen_addr(Cg& cg, CgType* want) const noexcept {
	auto type = gen_type(cg, want ? want->deref() : nullptr);
	if (!type) {
		return None{};
	}

	// Otherwise we actually construct a tuple and emit stores for all the values.
	// We do some additional work for padding fields to ensure they're always
	// zeroed as well.
	Array<CgValue> values{*cg.scratch};
	for (Ulen l = length(), i = 0; i < l; i++) {
		auto infer = type->at(i);
		auto value = at(i)->gen_value(cg, infer);
		if (!value) {
			return None{};
		}
		if (!values.push_back(move(*value))) {
			return cg.oom();
		}
	}

	auto addr = cg.emit_alloca(type); // *(...)

	Array<CgAddr> dsts{*cg.scratch};
	if (!dsts.reserve(type->length())) {
		return cg.oom();
	}
	for (Ulen l = type->length(), i = 0; i < l; i++) {
		(void)dsts.push_back(addr.at(cg, i));
	}

	Ulen j = 0;
	for (auto& dst : dsts) {
		if (dst.type()->deref()->is_padding()) {
			// Emit zeroinitializer for padding.
			if (!dst.zero(cg)) {
				return cg.oom();
			}
		} else {
			// We use j++ to index as values contains non-padding fields.
			if (!dst.store(cg, values[j++])) {
				return cg.oom();
			}
		}
	}
	return addr;
}

Maybe<CgValue> AstTupleExpr::gen_value(Cg& cg, CgType* want) const noexcept {
	auto type = gen_type(cg, want);
	if (!type) {
		return None{};
	}
	if (auto addr = gen_addr(cg, type->addrof(cg))) {
		return addr->load(cg);
	}
	return cg.fatal(range(), "Could not generate address");
}

CgType* AstTupleExpr::gen_type(Cg& cg, CgType* want) const noexcept {
	switch (length()) {
	case 0:
		return cg.types.unit();
	}
	// [1, n)
	Array<CgType*> types{*cg.scratch};
	if (!types.reserve(length())) {
		return cg.oom();
	}
	// When infering the elements of our tuple we expect 'want' to be of a tuple
	// type or pointer-to-tuple type.
	//
	// This allows us to infer stuff like:
	//	let x = &(a, b, c);
	CgType* expect = nullptr;
	if (want) {
		if (want->is_pointer()) {
			want = want->deref();
		}
		if (want->is_tuple()) {
			expect = want;
		}
	}
	for (Ulen l = length(), i = 0; i < l; i++) {
		auto infer = expect ? expect->at(i) : nullptr;
		auto elem = at(i);
		auto type = elem->gen_type(cg, infer);
		if (!type) {
			return nullptr;
		}
		if (!types.push_back(type)) {
			return cg.oom();
		}
	}
	return cg.types.make(CgType::TupleInfo { move(types), None{}, None{} });
}

CgType* AstCallExpr::gen_type(Cg& cg, CgType*) const noexcept {
	auto fn = m_callee->gen_type(cg, nullptr);
	if (!fn) {
		return nullptr;
	}
	
	// All function types are implicit pointer types so we need a dereference.
	auto type = fn->deref();
	if (type->is_tuple()) {
		// This is a method call like:
		//	(a, b).c
		//
		// Which the compiler turns into:
		//	(&c, &(a, b))
		//
		// This is just to make it easier to handle later. We should ideally make
		// a special intermediate representation but that will require our own IR.
		//
		// Anyways, this is why type->at(0) is used here.
		type = type->at(0)->deref();
	} else {
		// This is a function call
		if (type->is_pointer()) {
			// This is an indirect function call like:
			//	a.b()
			//
			// Where b is a field of a
			type = type->deref();
		} else {
			// This is a direct function call like
			//	m.a() or a()
			//
			// Where a is a function and p is an optional m
		}
	}

	if (type->is_fn()) {
		return type->at(3);
	}
	
	return cg.error(m_callee->range(), "Expected function type for callee. Got '%S' instead", fn->to_string(*cg.scratch));
}

Maybe<CgAddr> AstCallExpr::gen_addr(Cg&, CgType*) const noexcept {
	// Unsupported
	return None{};
}

Maybe<CgValue> AstCallExpr::gen_value(Cg& cg, CgType*) const noexcept {
	if (!gen_type(cg, nullptr)) {
		return None{};
	}

	auto callee = m_callee->gen_addr(cg, nullptr);
	if (!callee) {
		return None{};
	}

	auto call = callee;
	auto type = callee->type()->deref();
	if (type->is_tuple()) {
		call = callee->at(cg, 0).load(cg).to_addr();
		type = type->at(0)->deref();
	} else {
		// This is a function call
		if (type->is_pointer()) {
			// This is an indirect function call
			call = callee->load(cg).to_addr();
			type = type->deref();
		} else {
			// This is a direct function call
			call = callee;
		}
	}

	// 0 = objs
	// 1 = args
	// 2 = effects
	// 3 = ret

	auto objs = type->at(0);
	auto expected = type->at(1);
	auto effects = type->at(2);
	auto ret = type->at(3);

	Array<LLVM::ValueRef> values{*cg.scratch};
	auto reserve = objs->length() + expected->length() + effects->length();
	if (!values.reserve(reserve)) {
		return cg.oom();
	}

	// Populate the optional effects.
	if (effects != cg.types.unit()) {
		Array<CgVar> usings{*cg.scratch};
		const auto& fields = effects->fields();
		for (const auto& field : fields) {
			if (!field.name) {
				continue;
			}
			auto lookup = cg.lookup_using(*field.name);
			if (!lookup) {
				return cg.error(m_callee->range(), "This function requires the '%S' effect", *field.name);
			}
			if (!usings.push_back(*lookup)) {
				return cg.oom();
			}
		}
		auto dst = cg.emit_alloca(effects);

		// Populate the effects tuple with all our effects.
		Array<CgAddr> dsts{*cg.scratch};
		if (!dsts.reserve(usings.length())) {
			return cg.oom();
		}
		for (Ulen l = usings.length(), i = 0; i < l; i++) {
			// Use virtual indices since usings array is in terms of virtual indices.
			(void)dsts.emplace_back(dst.at_virt(cg, i));
		}
		for (Ulen l = usings.length(), i = 0; i < l; i++) {
			dsts[i].store(cg, usings[i].addr().load(cg));
		}
		// Then pass that tuple by address as the first argument to the function.
		if (!values.push_back(dst.ref())) {
			return cg.oom();
		}
	}

	// Populate the objects now
	if (objs != cg.types.unit()) {
		auto packet = callee->at(cg, 1).load(cg).to_addr();

		// The packet will have type *(...)
		// detupling semantics.
		auto type = packet.type()->deref();
		// When we get here we will have *(T) or *NamedT
		if (type->is_tuple() && !type->name()) {
			for (Ulen l = type->length(), i = 0; i < l; i++) {
				auto value = packet.at(cg, i).load(cg);
				if (!values.push_back(value.ref())) {
					return cg.oom();
				}
			}
		} else {
			if (!values.push_back(packet.ref())) {
				return cg.oom();
			}
		}
	}

	Ulen k = 0;
	for (Ulen l = m_args->length(), i = 0; i < l; i++) {
		auto arg = m_args->at(i);
		if (auto expr = arg->to_expr<const AstExplodeExpr>()) {
			auto args = expr->gen_value(cg, expected);
			if (!args) {
				return None{};
			}
			for (Ulen l = args->type()->length(), i = 0; i < l; i++) {
				auto value = args->at(cg, i);
				auto have_type = args->type()->at(i);
				auto want_type = expected->at(k);
				if (*have_type != *want_type) {
					auto have_type_string = have_type->to_string(*cg.scratch);
					auto want_type_string = want_type->to_string(*cg.scratch);
					return cg.error(m_args->range(),
					                "Expected expression of type '%S' in expansion of tuple for argument. Got '%S' instead",
					                want_type_string,
					                have_type_string);
				}
				k++;
				if (!value) {
					return None{};
				}
				if (!values.push_back(value->ref())) {
					return cg.oom();
				}
			}
			continue;
		} 
		CgType* want_type = nullptr;
		if (k < expected->length()) {
			want_type = expected->at(k);
		} else {
			want_type = expected->at(expected->length() - 1);
		}
		auto value = arg->gen_value(cg, want_type);
		if (!value) {
			return None{};
		}
		// When we're calling a C abi function we destructure the String type and
		// pick out the pointer to the string to pass along instead.
		if (m_c && value->type()->is_string()) {
			// We want to extract the 0th element of the String CgValue which contains
			// the raw string pointer. Since it's a pointer we do not need to create
			// an alloca to extract it.
			value = value->at(cg, 0);
			if (!value) {
				return None{};
			}
		}
		auto have_type = value->type();
		if (!want_type->is_va() && *value->type() != *want_type) {
			auto have_type_string = have_type->to_string(*cg.scratch);
			auto want_type_string = want_type->to_string(*cg.scratch);
			return cg.error(arg->range(),
			                "Expected expression of type '%S' for argument. Got '%S' instead",
			                want_type_string,
			                have_type_string);
		}
		k++;
		if (!values.push_back(value->ref())) {
			return cg.oom();
		}
	}

	auto value = cg.llvm.BuildCall2(cg.builder,
	                                type->ref(),
	                                call->ref(),
	                                values.data(),
	                                values.length(),
	                                "");

	// When the function is marked as returning a single-element tuple
	// we need to retuple the result because we detuple the generation of the
	// function but the caller still expects to have a single-element tuple
	// as a result.
	if (ret->is_tuple() && ret->length() == 1) {
		// TODO(dweiler): We need to do this in a much better way.
		auto dst = cg.emit_alloca(ret);
		auto at = dst.at(cg, 0);
		at.store(cg, CgValue { at.type()->deref(), value });
		return dst.load(cg);
	}

	return CgValue { ret, value };
}

Maybe<AstConst> AstVarExpr::eval_value(Cg& cg) const noexcept {
	for (const auto& global : cg.globals) {
		if (global.var().name() == m_name) {
			return global.value().copy();
		}
	}
	// Not a valid compile-time expression
	return None{};
}

Maybe<CgAddr> AstVarExpr::gen_addr(Cg& cg, CgType*) const noexcept {
	auto lookup = cg.lookup_let(m_name);
	if (lookup) {
		return lookup->addr();
	}

	// Search module for functions.
	for (const auto& fn : cg.fns) {
		if (fn.name() == m_name) {
			return fn.addr();
		}
	}

	// Search module for globals.
	for (const auto& global : cg.globals) {
		if (global.var().name() == m_name) {
			return global.var().addr();
		}
	}

	return cg.error(range(), "Could not find symbol '%S'", m_name);
}

Maybe<CgValue> AstVarExpr::gen_value(Cg& cg, CgType* want) const noexcept {
	auto type = gen_type(cg, want);
	if (!type) {
		return None{};
	}
	if (auto addr = gen_addr(cg, type->addrof(cg))) {
		return addr->load(cg);
	}
	return cg.fatal(range(), "Could not generate value (AstVarExpr)");
}

CgType* AstVarExpr::gen_type(Cg& cg, CgType* want) const noexcept {
	auto addr = gen_addr(cg, want ? want->addrof(cg) : nullptr);
	if (!addr) {
		return cg.fatal(range(), "Could not generate type (AstVarExpr)");
	}
	auto type = addr->type();
	auto deref = type->deref();
	return deref->is_fn() ? type : deref;
}

CgType* AstSelectorExpr::gen_type(Cg& cg, CgType* want) const noexcept {
	if (want && want->is_union()) {
		// Search the union for an enum which contains the selector.
		CgType* found = nullptr;
		for (const auto& type : want->types()) {
			if (!type->is_enum()) {
				// Skip non-enum types in the union.
				continue;
			}
			for (const auto& field : type->fields()) {
				if (field.name != m_name) {
					// Skip fields of the enum which do not match our selector.
					continue;
				}
				if (found) {
					// We already found one matching our name so this would be ambigious.
					return cg.error(range(), "Selector '.%S' is ambigious in this context", m_name);
				}
				// We found a matching enum type for our selector.
				found = type;
			}
		}
		if (found) {
			return found;
		}
	} else if (want && want->is_enum()) {
		return want;
	}
	return cg.error(range(), "Cannot infer type from implicit selector expression");
}

Maybe<CgValue> AstSelectorExpr::gen_value(Cg& cg, CgType* want) const noexcept {
	auto type = gen_type(cg, want);
	if (!type) {
		return None{};
	}

	const auto& fields = type->fields();
	for (const auto& field : fields) {
		if (*field.name != m_name) {
			continue;
		}
		auto value = field.init->codegen(cg, type);
		if (!value) {
			return None{};
		}
		// TODO(dweiler): Check this cast is safe ... Safeish cast
		return CgValue { type, value->ref() };
	}

	return cg.error(range(), "Could not find enumerator");
}

Maybe<CgAddr> AstSelectorExpr::gen_addr(Cg& cg, CgType* want) const noexcept {
	auto value = gen_value(cg, want ? want->deref() : nullptr);
	if (!value) {
		return None{};
	}
	auto dst = cg.emit_alloca(value->type());
	if (!dst.store(cg, *value)) {
		return None{};
	}
	return dst;
}

Maybe<AstConst> AstIntExpr::eval_value(Cg&) const noexcept {
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

Maybe<CgValue> AstIntExpr::gen_value(Cg& cg, CgType* want) const noexcept {
	if (want && want->is_atomic()) {
		want = want->types()[0];
	}

	auto type = gen_type(cg, want);
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
		v = cg.llvm.ConstInt(type->ref(), m_as_uint, type->is_sint());
		break;
	}
	if (v) {
		return CgValue { type, v };
	}
	return cg.fatal(range(), "Could not generate value (AstIntExpr)");
}

CgType* AstIntExpr::gen_type(Cg& cg, CgType* want) const noexcept {
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
		if (want) {
			if (want->is_integer()) {
				return want;
			} else if (want->is_real()) {
				return cg.error(range(), "Expected integer literal. Got '%S' instead", want->to_string(*cg.scratch));
			}
		}
		break;
	}
	return nullptr;
}

Maybe<AstConst> AstFltExpr::eval_value(Cg&) const noexcept {
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

Maybe<CgValue> AstFltExpr::gen_value(Cg& cg, CgType* want) const noexcept {
	auto type = gen_type(cg, want);
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
		if (want->is_real()) {
			v = cg.llvm.ConstReal(want->ref(), m_as_f64);
		}
		break;
	}
	if (v) {
		return CgValue { type, v };
	}
	return cg.fatal(range(), "Could not generate value (AstFltExpr)");
}

CgType* AstFltExpr::gen_type(Cg& cg, CgType* want) const noexcept {
	switch (m_kind) {
	case Kind::F32:
		return cg.types.f32();
	case Kind::F64:
		return cg.types.f64();
	case Kind::UNTYPED:
		if (want) {
			if (want->is_real()) {
				return want;
			} else if (want->is_integer()) {
				return cg.error(range(), "Expected floating-point literal. Got '%S' instead", want->to_string(*cg.scratch));
			}
		}
		break;
	}
	return nullptr;
}

Maybe<AstConst> AstStrExpr::eval_value(Cg&) const noexcept {
	return AstConst { range(), m_literal };
}

Maybe<CgValue> AstStrExpr::gen_value(Cg& cg, CgType* want) const noexcept {
	auto type = gen_type(cg, want);
	if (!type) {
		return None{};
	}

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
	auto value = cg.llvm.ConstNamedStruct(type->ref(), values, countof(values));
	return CgValue { type, value };
}

CgType* AstStrExpr::gen_type(Cg& cg, CgType*) const noexcept {
	return cg.types.str();
}

Maybe<AstConst> AstBoolExpr::eval_value(Cg&) const noexcept {
	return AstConst { range(), Bool32 { m_value } };
}

Maybe<CgValue> AstBoolExpr::gen_value(Cg& cg, CgType* want) const noexcept {
	auto type = gen_type(cg, want);
	auto value = cg.llvm.ConstInt(type->ref(), m_value ? 1 : 0, false);
	return CgValue { type, value };
}

CgType* AstBoolExpr::gen_type(Cg& cg, CgType*) const noexcept {
	// LLVM has an Int1 type which can store either a 0 or 1 value which is what
	// the IR uses for "boolean" like things. We map all our Bool* types to Int1
	// except we over-align them based on the size we expect. So we just use the
	// Bool8 type for any "literal" expression.
	return cg.types.b8();
}

Maybe<AstConst> AstAggExpr::eval_value(Cg& cg) const noexcept {
	ScratchAllocator scratch{m_exprs.allocator()};
	Array<AstConst> values{m_exprs.allocator()};
	if (!values.reserve(m_exprs.length())) {
		return None{};
	}
	auto range = m_exprs[0]->range();
	for (auto expr : m_exprs) {
		auto value = expr->eval_value(cg);
		if (!value) {
			return None{};
		}
		range = range.include(expr->range());
		if (!values.push_back(move(*value))) {
			return cg.oom();
		}
	}
	// TODO(dweiler): We need to introduce typing to eval_value for this
	if (!m_type || m_type->is_type<AstArrayType>()) {
		return AstConst { range, AstConst::ConstArray { m_type, move(values) } };
	} else {
		return AstConst { range, AstConst::ConstTuple { m_type, move(values), None{} } };
	}
	BIRON_UNREACHABLE();
}

Maybe<CgAddr> AstAggExpr::gen_addr(Cg& cg, CgType* want) const noexcept {
	auto type = gen_type(cg, want ? want->deref() : nullptr);
	if (!type) {
		return cg.fatal(range(), "Could not generate type (AstAggExpr)");
	}

	// When emitting an aggregate we need to stack allocate because the aggregate
	// likely cannot fit into a regsiter. This means an aggregate has an address.
	auto addr = cg.emit_alloca(type);

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
		return cg.error(range(), "Too many expressions in aggregate initializer");
	}

	if (m_exprs.length() == 0) {
		// No expression so zero initialize it.
		if (!addr.zero(cg)) {
			return cg.oom();
		}
		return addr;
	}

	// The scalar case we just read from [0] and write to addr.
	if (scalar) {
		auto value = m_exprs[0]->gen_value(cg, type);
		if (!value) {
			return None{};
		}

		auto src_type = value->type();
		auto dst_type = addr.type()->deref();

		// When the destination is a union type look for a compatible inner type.
		if (dst_type->is_union()) {
			if (auto find = dst_type->contains(src_type)) {
				dst_type = find;
			}
		}

		if (*src_type != *dst_type) {
			return cg.error(m_exprs[0]->range(), "Expression with incompatible type in aggregate");
		}

		if (!addr.store(cg, *value)) {
			return cg.oom();
		}

		return addr;
	}

	// Sequence types (tuples, arrays, etc) we have to step over every index.
	Array<CgAddr> addrs{*cg.scratch};
	for (Ulen l = count, i = 0; i < l; i++) {
		auto dst = addr.at(cg, i);
		if (!addrs.push_back(dst)) {
			return cg.oom();
		}
	}

	for (Ulen l = count, i = 0, j = 0; i < l; i++) {
		auto& dst = addrs[i];
		// The 'dst' type will always be a pointer so dereference.
		auto dst_type = dst.type()->deref();
		if (dst_type->is_padding()) {
			// Write a zeroinitializer into padding at i'th.
			if (!dst.zero(cg)) {
				return cg.oom();
			}
		} else if (auto expr = m_exprs.at(j++)) {
			// Otherwise take the next expression and store it at i'th.
			auto infer = type->is_array() ? type->at(0) : type->at(i);
			auto value = (*expr)->gen_value(cg, infer);
			if (!value) {
				return None{};
			}

			// When the destination is a union type look for a compatible inner type.
			if (dst_type->is_union()) {
				auto src_type = value->type();
				if (auto find = dst_type->contains(src_type)) {
					dst_type = find;
				}
			}

			if (*value->type() != *dst_type) {
				return cg.error(m_exprs[0]->range(), "Expression with incompatible type in aggregate");
			}

			if (!dst.store(cg, *value)) {
				return cg.oom();
			}
		} else {
			// No expression for that initializer so generate a zeroinitializer
			if (!dst.zero(cg)) {
				return cg.oom();
			}
		}
	}
	return addr;
}

Maybe<CgValue> AstAggExpr::gen_value(Cg& cg, CgType* want) const noexcept {
	auto type = gen_type(cg, want);
	if (!type) {
		return None{};
	}
	if (auto addr = gen_addr(cg, type->addrof(cg))) {
		return addr->load(cg);
	}
	return cg.fatal(range(), "Could not generate value (AstAggExpr)");
}

CgType* AstAggExpr::gen_type(Cg& cg, CgType* want) const noexcept {
	if (!m_type) {
		return want;
	}
	// Special behavior needed when infering the length of an array.
	if (auto type = m_type->to_type<AstArrayType>()) {
		if (type->extent()->is_expr<AstInferSizeExpr>()) {
			auto base = type->base()->codegen(cg, None{});
			if (!base) {
				return nullptr;
			}
			auto type = cg.types.make(CgType::ArrayInfo {
				base,
				m_exprs.length(),
				None{}
			});
			if (!type) {
				return cg.oom();
			}
			return type;
		}
	}
	return m_type->codegen(cg, None{});
}

CgType* AstAccessExpr::gen_type(Cg& cg, CgType* want) const noexcept {
	auto lhs_type = m_lhs->gen_type(cg, nullptr);
	if (!lhs_type) {
		return nullptr;
	}
	if (lhs_type->is_pointer()) {
		// Implicit dereference behavior:
		//	a.b -> (*a).b
		lhs_type = lhs_type->deref();
	}
	if (!lhs_type->is_tuple()) {
		auto to_string = lhs_type->to_string(*cg.scratch);
		return cg.error(m_lhs->range(), "Expected tuple type. Got '%S' instead", to_string);
	}
	if (m_rhs->is_expr<AstCallExpr>()) {
		return m_rhs->gen_type(cg, want);
	} else if (auto rhs = m_rhs->to_expr<const AstVarExpr>()) {
		// TODO(dweiler): Name mangling.
		if (auto fn = cg.lookup_fn(rhs->name())) {
			return fn->addr().type();
		}
		// TODO(dweiler): support .* lookup
		// if (auto let = cg.lookup_let(rhs->name())) {
		// 	return let->addr().type();
		// }
		if (lhs_type->is_tuple() || lhs_type->is_enum()) {
			Ulen i = 0;
			for (const auto& field : lhs_type->fields()) {
				if (field.name && *field.name == rhs->name()) {
					return lhs_type->at(i);
				}
				i++;
			}
		}
		return cg.error(m_rhs->range(), "Undeclared field '%S'", rhs->name());
	} else if (m_rhs->is_expr<AstIntExpr>()) {
		const auto value = m_rhs->eval_value(cg);
		if (!value || !value->is_integral()) {
			return cg.error(m_rhs->range(), "Not a valid integer constant expression");
		}
		const auto u64 = value->to<Uint64>();
		if (!u64) {
			return nullptr;
		}
		return lhs_type->at_virt(*u64);
	}
	return nullptr;
}

Maybe<CgAddr> AstAccessExpr::gen_addr(Cg& cg, CgType* want) const noexcept {
	if (!gen_type(cg, want ? want->deref() : nullptr)) {
		return None{};
	}

	if (const auto expr = m_rhs->to_expr<const AstVarExpr>()) {
		auto lhs_addr = m_lhs->gen_addr(cg, want);
		if (!lhs_addr) {
			return None{};
		}

		auto type = lhs_addr->type()->deref();
		if (type->is_pointer()) {
			// Handle implicit dereference:
			//	a.b -> (*a).b
			lhs_addr = lhs_addr->load(cg).to_addr();
		}

		// When calling a method we generate a tuple with the first value the
		// address of the method and the second value the objects to pass to the
		// method. This is later exploded into the method call.
		Maybe<CgAddr> fun_addr;
		// TODO(dweiler): Name mangling.
		if (auto fn = cg.lookup_fn(expr->name())) {
			fun_addr = fn->addr();
		}
		// TODO(dweiler): Support .* lookup
		// if (auto let = cg.lookup_let(expr->name())) {
		// 	fun_addr = let->addr().load(cg).to_addr();
		// }

		if (fun_addr) {
			InlineAllocator<128> stack;
			Array<CgType*> types{stack};
			if (!types.resize(2)) {
				return cg.oom();
			}
			types[0] = fun_addr->type();
			types[1] = lhs_addr->type();
			auto tuple = cg.types.make(CgType::TupleInfo { move(types), None{}, None{} });
			if (!tuple) {
				return cg.oom();
			}
			// Populate the tuple with our two addresses.
			auto dst = cg.emit_alloca(tuple);
			CgAddr addrs[] = {
				dst.at(cg, 0),
				dst.at(cg, 1)
			};
			addrs[0].store(cg, fun_addr->to_value());
			addrs[1].store(cg, lhs_addr->to_value());
			return dst;
		}
		if (type->is_pointer()) {
			// Handle implicit dereference:
			//	a.b -> (*a).b
			type = type->deref();
		}
		if (type->is_tuple()) {
			const auto& name = expr->name();
			const auto& fields = type->fields();
			for (Ulen l = fields.length(), i = 0; i < l; i++) {
				const auto& field = fields[i];
				if (field.name && *field.name == name) {
					return lhs_addr->at(cg, i);
				}
			}
			return cg.error(m_rhs->range(), "Undeclared field '%S'", name);
		}
		return None{};
	} else if (m_rhs->is_expr<AstIntExpr>()) {
		auto rhs = m_rhs->eval_value(cg);
		if (!rhs || !rhs->is_integral()) {
			return cg.error(m_rhs->range(), "Expected integer constant expression");
		}
		auto index = rhs->to<Uint64>();
		if (!index) {
			return cg.error(m_rhs->range(), "Expected integer constant expression");
		}
		auto addr = m_lhs->gen_addr(cg, want);
		if (!addr) {
			return None{};
		}
		auto type = addr->type()->deref();
		if (type->is_pointer()) {
			// Handle implicit dereference:
			//	a.b -> (*a).b
			addr = addr->load(cg).to_addr();
		}
		return addr->at_virt(cg, *index);
	}
	return cg.error(m_rhs->range(), "Unknown expression for access");
}

Maybe<CgValue> AstAccessExpr::gen_value(Cg& cg, CgType* want) const noexcept {
	if (m_rhs->is_expr<AstVarExpr>() || m_rhs->is_expr<AstIntExpr>()) {
		auto addr = gen_addr(cg, want ? want->addrof(cg) : nullptr);
		if (!addr) {
			return None{};
		}
		return addr->load(cg);
	}
	return None{};
}

Maybe<AstConst> AstBinExpr::eval_value(Cg& cg) const noexcept {
	auto lhs = m_lhs->eval_value(cg);
	if (!lhs) {
		// Not a valid compile time constant expression
		return None{};
	}

	auto rhs = m_rhs->eval_value(cg);
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

	// Generates 2 functions
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
	case Op::BOR:    return numeric([]<typename T>(T lhs, T rhs) -> T { return lhs | rhs; });
	case Op::BAND:   return numeric([]<typename T>(T lhs, T rhs) -> T { return lhs & rhs; });
	case Op::LSHIFT: return numeric([]<typename T>(T lhs, T rhs) -> T { return lhs << rhs; });
	case Op::RSHIFT: return numeric([]<typename T>(T lhs, T rhs) -> T { return lhs >> rhs; });
	default:
		return None{};
	}
	BIRON_UNREACHABLE();
}

Maybe<CgValue> AstBinExpr::gen_value(Cg& cg, CgType* want) const noexcept {
	using IntPredicate = LLVM::IntPredicate;
	using RealPredicate = LLVM::RealPredicate;

	CgType* lhs_type = nullptr;
	CgType* rhs_type = nullptr;

	auto lhs_expr = detuple(m_lhs);
	auto rhs_expr = detuple(m_rhs);

	// Operands to binary operator must be the same type
	lhs_type = lhs_expr->gen_type(cg, want);
	if (lhs_type) {
		rhs_type = rhs_expr->gen_type(cg, lhs_type);
	} else {
		rhs_type = rhs_expr->gen_type(cg, want);
		lhs_type = lhs_expr->gen_type(cg, rhs_type);
	}
	if (!lhs_type || !rhs_type) {
		return cg.error(range(), "Could not infer types in binary expression");
	}

	if (*lhs_type != *rhs_type) {
		auto lhs_to_string = lhs_type->to_string(*cg.scratch);
		auto rhs_to_string = rhs_type->to_string(*cg.scratch);
		return cg.error(range(),
		                "Operands to binary operator must be the same type: Got '%S' and '%S'",
		                lhs_to_string,
		                rhs_to_string);
	}

	auto lhs = lhs_expr->gen_value(cg, lhs_type);
	auto rhs = rhs_expr->gen_value(cg, lhs_type);
	if (!lhs || !rhs) {
		return None{};
	}

	switch (m_op) {
	case Op::ADD:
		return cg.emit_add(*lhs, *rhs, range());
	case Op::SUB:
		return cg.emit_sub(*lhs, *rhs, range());
	case Op::MUL:
		return cg.emit_mul(*lhs, *rhs, range());
	case Op::DIV:
		return cg.emit_div(*lhs, *rhs, range());
	case Op::MIN:
		return cg.emit_min(*lhs, *rhs, range());
	case Op::MAX:
		return cg.emit_max(*lhs, *rhs, range());
	case Op::EQ:
		if (lhs_type->is_sint() || lhs_type->is_uint() || lhs_type->is_pointer()) {
			auto value = cg.llvm.BuildICmp(cg.builder, IntPredicate::EQ, lhs->ref(), rhs->ref(), "");
			return CgValue { cg.types.b32(), value };
		} else if (lhs_type->is_real()) {
			auto value = cg.llvm.BuildFCmp(cg.builder, RealPredicate::OEQ, lhs->ref(), rhs->ref(), "");
			return CgValue { cg.types.b32(), value };
		} else {
			auto intrinsic = cg.intrinsic("memory_eq");
			if (!intrinsic) {
				return cg.fatal(range(), "Could not find 'memory_eq' intrinsic");
			}
			auto lhs_dst = m_lhs->gen_addr(cg, lhs_type->addrof(cg));
			auto rhs_dst = m_rhs->gen_addr(cg, rhs_type->addrof(cg));
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
			auto value = cg.llvm.BuildICmp(cg.builder, IntPredicate::NE, lhs->ref(), rhs->ref(), "");
			return CgValue { cg.types.b32(), value };
		} else if (lhs_type->is_real()) {
			auto value = cg.llvm.BuildFCmp(cg.builder, RealPredicate::ONE, lhs->ref(), rhs->ref(), "");
			return CgValue { cg.types.b32(), value };
		} else {
			auto intrinsic = cg.intrinsic("memory_ne");
			if (!intrinsic) {
				return cg.fatal(range(), "Could not find 'memory_ne' intrinsic");
			}
			auto lhs_dst = m_lhs->gen_addr(cg, nullptr);
			auto rhs_dst = m_rhs->gen_addr(cg, nullptr);
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
		return cg.emit_gt(*lhs, *rhs, range());
	case Op::GE:
		return cg.emit_ge(*lhs, *rhs, range());
	case Op::LT:
		return cg.emit_lt(*lhs, *rhs, range());
	case Op::LE:
		return cg.emit_le(*lhs, *rhs, range());
	case Op::BOR:
		if (lhs_type->is_integer() || lhs_type->is_bool()) {
			auto value = cg.llvm.BuildOr(cg.builder, lhs->ref(), rhs->ref(), "");
			return CgValue { lhs_type, value };
		} else {
			auto lhs_type_string = lhs_type->to_string(*cg.scratch);
			return cg.error(range(),
			                "Operands to '|' operator must have integer or boolean type. Got '%S' instead",
			                lhs_type_string);
			return None{};
		}
		break;
	case Op::BAND:
		if (lhs_type->is_integer() || lhs_type->is_bool()) {
			auto value = cg.llvm.BuildAnd(cg.builder, lhs->ref(), rhs->ref(), "");
			return CgValue { lhs_type, value };
		} else {
			auto lhs_type_string = lhs_type->to_string(*cg.scratch);
			return cg.error(range(),
			                "Operands to '&' operator must have integer or boolean type. Got '%S' instead",
			                lhs_type_string);
		}
		break;
	case Op::LSHIFT:
		if (lhs_type->is_integer()) {
			auto value = cg.llvm.BuildShl(cg.builder, lhs->ref(), rhs->ref(), "");
			return CgValue { lhs_type, value };
		} else {
			auto lhs_type_string = lhs_type->to_string(*cg.scratch);
			return cg.error(range(),
			                "Operands to '<<' operator must have integer type. Got '%S' instead",
			                lhs_type_string);
		}
		break;
	case Op::RSHIFT:
		if (lhs_type->is_sint()) {
			auto value = cg.llvm.BuildAShr(cg.builder, lhs->ref(), rhs->ref(), "");
			return CgValue { lhs_type, value };
		} else if (lhs_type->is_uint()) {
			auto value = cg.llvm.BuildLShr(cg.builder, lhs->ref(), rhs->ref(), "");
			return CgValue { lhs_type, value };
		} else {
			auto lhs_type_string = lhs_type->to_string(*cg.scratch);
			return cg.error(range(),
			                "Operands to '>>' operator must have integer type. Got '%S' instead",
			                lhs_type_string);
		}
	default:
		break;
	}
	return cg.fatal(range(), "Could not generate value (AstBinExpr)");
}

Maybe<CgAddr> AstBinExpr::gen_addr(Cg& cg, CgType* want) const noexcept {
	auto value = gen_value(cg, want ? want->deref() : nullptr);
	if (!value) {
		return None{};
	}
	auto addr = cg.emit_alloca(value->type());
	if (!addr.store(cg, *value)) {
		return None{};
	}
	return addr;
}

CgType* AstBinExpr::gen_type(Cg& cg, CgType* want) const noexcept {
	auto lhs_expr = detuple(m_lhs);
	if (auto type = lhs_expr->gen_type(cg, want)) {
		return type;
	}
	auto rhs_expr = detuple(m_rhs);
	if (auto type = rhs_expr->gen_type(cg, want)) {
		return type;
	}
	return nullptr;
}

Maybe<CgValue> AstLBinExpr::gen_value(Cg& cg, CgType* want) const noexcept {
	auto lhs_expr = detuple(m_lhs);
	auto rhs_expr = detuple(m_rhs);

	switch (m_op) {
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
	
			auto lhs = lhs_expr->gen_value(cg, want ? want : cg.types.b32());
			if (!lhs || !lhs->type()->is_bool()) {
				auto lhs_type_string = lhs->type()->to_string(*cg.scratch);
				return cg.error(lhs_expr->range(),
				                "Operands to '||' operator must have boolean type. Got '%S' instead",
				                lhs_type_string);
			}

			cg.llvm.BuildCondBr(cg.builder, lhs->ref(), on_lhs_true, on_lhs_false);

			// on_lhs_true
			cg.llvm.AppendExistingBasicBlock(this_fn, on_lhs_true);
			cg.llvm.PositionBuilderAtEnd(cg.builder, on_lhs_true);
			cg.llvm.BuildBr(cg.builder, on_exit);
		
			// on_lhs_false
			cg.llvm.AppendExistingBasicBlock(this_fn, on_lhs_false);
			cg.llvm.PositionBuilderAtEnd(cg.builder, on_lhs_false);
			auto rhs = rhs_expr->gen_value(cg, cg.types.b32());
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
	
			auto lhs = lhs_expr->gen_value(cg, want ? want : cg.types.b32());
			if (!lhs || !lhs->type()->is_bool()) {
				auto lhs_type_string = lhs->type()->to_string(*cg.scratch);
				return cg.error(lhs_expr->range(),
				                "Operands to '&&' operator must have boolean type. Got '%S' instead",
				                lhs_type_string);
			}

			cg.llvm.BuildCondBr(cg.builder, lhs->ref(), on_lhs_true, on_lhs_false);

			// on_lhs_true
			cg.llvm.AppendExistingBasicBlock(this_fn, on_lhs_true);
			cg.llvm.PositionBuilderAtEnd(cg.builder, on_lhs_true);
			auto rhs = rhs_expr->gen_value(cg, cg.types.b32());
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
	}
	return cg.fatal(range(), "Could not generate value (AstLBinExpr)");
}

Maybe<AstConst> AstLBinExpr::eval_value(Cg& cg) const noexcept {
	auto lhs = m_lhs->eval_value(cg);
	if (!lhs) {
		// Not a valid compile-time constant expression
		return None{};
	}

	auto rhs = m_rhs->eval_value(cg);
	if (!rhs) {
		// Not a valid compile-time constant expression
		return None{};
	}

	// Operands to binary operator must be the same type
	if (lhs->kind() != rhs->kind()) {
		return None{};
	}

	// Generates 2 functions
	auto boolean = [&](auto f) noexcept -> Maybe<AstConst> {
		if (lhs->is_bool()) {
			return AstConst { range(), lhs->kind(), f(lhs->as_bool(), rhs->as_bool()) };
		}
		return None{};
	};

	switch (m_op) {
	case Op::LOR:
		return boolean([]<typename T>(T lhs, T rhs) -> T { return lhs || rhs; });
	case Op::LAND:
		return boolean([]<typename T>(T lhs, T rhs) -> T { return lhs && rhs; });
	default:
		return None{};
	}
	BIRON_UNREACHABLE();
}

Maybe<CgAddr> AstLBinExpr::gen_addr(Cg& cg, CgType* want) const noexcept {
	auto value = gen_value(cg, want ? want->deref() : nullptr);
	if (!value) {
		return None{};
	}
	auto addr = cg.emit_alloca(value->type());
	if (!addr.store(cg, *value)) {
		return None{};
	}
	return addr;
}

CgType* AstLBinExpr::gen_type(Cg& cg, CgType* want) const noexcept {
	return want ? want : cg.types.b32();
}

Maybe<CgAddr> AstUnaryExpr::gen_addr(Cg& cg, CgType* want) const noexcept {
	auto operand = detuple(m_operand);

	switch (m_op) {
	case Op::NEG:
		return cg.error(range(), "Cannot take the address of an rvalue");
	case Op::NOT:
		return cg.error(range(), "Cannot take the address of an rvalue");
	case Op::DEREF:
		// When dereferencing on the LHS we will have a R-value of the address 
		// which we just turn back into an address.
		//
		// The LHS dereference does not actually load or dereference anything, it's
		// used in combination with an AssignStmt to allow storing results through
		// the pointer.
		if (auto value = operand->gen_value(cg, want ? want->deref() : nullptr)) {
			if (!value->type()->is_pointer()) {
				auto operand_type_string = value->type()->to_string(*cg.scratch);
				return cg.error(m_operand->range(),
				                "Operand to '*' must have pointer type. Got '%S' instead",
				                operand_type_string);
			}
			return value->to_addr();
		}
		break;
	case Op::ADDROF:
		return cg.error(range(), "Cannot take the address of an rvalue");
	}
	BIRON_UNREACHABLE();
}

Maybe<CgValue> AstUnaryExpr::gen_value(Cg& cg, CgType* want) const noexcept {
	auto type = gen_type(cg, want);
	if (!type) {
		return None{};
	}

	auto operand = detuple(m_operand);
	switch (m_op) {
	case Op::NEG:
		if (auto value = operand->gen_value(cg, type)) {
			if (value->type()->is_real()) {
				return CgValue { value->type(), cg.llvm.BuildFNeg(cg.builder, value->ref(), "") };
			} else {
				return CgValue { value->type(), cg.llvm.BuildNeg(cg.builder, value->ref(), "") };
			}
		}
		break;
	case Op::NOT:
		if (auto value = operand->gen_value(cg, type)) {
			return CgValue { value->type(), cg.llvm.BuildNot(cg.builder, value->ref(), "") };
		}
		break;
	case Op::DEREF:
		// When dereferencing on the RHS we just gen_addr followed by a load.
		if (auto addr = gen_addr(cg, type->addrof(cg))) {
			return addr->load(cg);
		}
		break;
	case Op::ADDROF:
		// When taking the address we just gen_addr and turn it into a CgValue which
		// gives us an R-value of the address.
		if (auto addr = operand->gen_addr(cg, type->addrof(cg))) {
			return addr->to_value();
		}
		break;
	}
	return cg.fatal(m_operand->range(), "Could not generate value (AstUnaryExpr)");
}

CgType* AstUnaryExpr::gen_type(Cg& cg, CgType* want) const noexcept {
	auto operand = detuple(m_operand);
	if (m_op == Op::NEG || m_op == Op::NOT) {
		auto type = operand->gen_type(cg, want);
		if (!type) {
			return nullptr;
		}
		return type;
	} else if (m_op == Op::DEREF) {
		auto type = operand->gen_type(cg, nullptr);
		if (!type) {
			return nullptr;
		}
		if (type->is_pointer()) {
			return type->deref();
		} else {
			return cg.error(range(), "Cannot dereference expression of type '%S'", type->to_string(*cg.scratch));
		}
	} else if (m_op == Op::ADDROF) {
		auto type = operand->gen_type(cg, nullptr);
		if (!type) {
			return nullptr;
		}
		return type->addrof(cg);
	}
	BIRON_UNREACHABLE();
}

Maybe<CgAddr> AstIndexExpr::gen_addr(Cg& cg, CgType*) const noexcept {
	auto operand = detuple(m_operand)->gen_addr(cg, nullptr);
	if (!operand) {
		return None{};
	}

	// Peel the *Uint8 out of the string when indexing
	if (operand->type()->deref()->is_string()) {
		operand = operand->at(cg, 0);
	}

	// Optimization for constant integer expression indexing.
	if (auto eval = m_index->eval_value(cg)) {
		if (!eval->is_integral()) {
			return cg.error(eval->range(), "Cannot index with a constant expression of non-integer type");
		}
		auto index = eval->to<Uint64>();
		if (!index) {
			return None{};
		}
		return operand->at(cg, *index);
	} else {
		// Otherwise runtime indexing
		auto index = m_index->gen_value(cg, cg.types.u64());
		if (!index) {
			return None{};
		}
		if (!index->type()->is_integer()) {
			auto index_type_string = index->type()->to_string(*cg.scratch);
			return cg.error(m_index->range(),
			                "Expected expression of integer type for index. Got '%S' instead",
			                index_type_string);
		}
		return operand->at(cg, *index);
	}
	return None{};
}

Maybe<CgValue> AstIndexExpr::gen_value(Cg& cg, CgType* want) const noexcept {
	auto type = gen_type(cg, want);
	if (!type) {
		return cg.fatal(range(), "Could not generate type");
	}

	// Cannot use CgValue::at when working with something that requires a load.
	// TODO(dweiler): reenable when I work out chained pointer dereference typing
	if (false && !type->is_pointer() && !type->is_slice() && !type->is_string()) {
		// Optimization when the index is a constant expression.
		if (auto eval = m_index->eval_value(cg)) {
			if (!eval->is_integral()) {
				return cg.error(eval->range(), "Cannot index with a constant expression of non-integer type");
			}
			auto index = eval->to<Uint64>();
			auto operand = m_operand->gen_value(cg, type);
			if (!operand) {
				return None{};
			}
			auto result = operand->at(cg, *index);
			if (!result) {
				return cg.fatal(range(), "Could not generate result '%S'", operand->type()->to_string(*cg.scratch));
			}
			return result;
		}
	}

	auto addr = gen_addr(cg, type->addrof(cg));
	if (!addr) {
		return cg.fatal(range(), "Could not generate address");
	}
	return addr->load(cg);
}

CgType* AstIndexExpr::gen_type(Cg& cg, CgType* want) const noexcept {
	auto type = detuple(m_operand)->gen_type(cg, want);
	if (!type) {
		return nullptr;
	}

	if (!type->is_pointer() && !type->is_array() && !type->is_slice() && !type->is_string()) {
		auto type_string = type->to_string(*cg.scratch);
		return cg.error(range(), "Cannot index expression of type '%S'", type_string);
	}
	return type->deref();
}

Maybe<AstConst> AstIndexExpr::eval_value(Cg& cg) const noexcept {
	auto operand = detuple(m_operand)->eval_value(cg);
	if (!operand) {
		return None{};
	}
	auto index = m_index->eval_value(cg);
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
	} else if (operand->is_string()) {
		if (auto value = operand->as_string()[*i]) {
			return AstConst { range(), Uint8(value) };
		}
	}
	return None{};
}

Maybe<CgValue> AstExplodeExpr::gen_value(Cg& cg, CgType* want) const noexcept {
	return m_operand->gen_value(cg, want);
}

Maybe<CgAddr> AstEffExpr::gen_addr(Cg& cg, CgType*) const noexcept {
	auto expr = expression();
	if (!expr) {
		return cg.error(m_operand->range(), "Expected expression for effect");
	}
	auto find = cg.lookup_using(expr->name());
	if (!find) {
		return cg.error(m_operand->range(), "Could not find effect '%S'", expr->name());
	}
	return find->addr();
}

Maybe<CgValue> AstEffExpr::gen_value(Cg& cg, CgType* want) const noexcept {
	auto addr = gen_addr(cg, want ? want->addrof(cg) : nullptr);
	if (!addr) {
		return None{};
	}
	return addr->load(cg);
}

CgType* AstEffExpr::gen_type(Cg& cg, CgType* want) const noexcept {
	auto addr = gen_addr(cg, want ? want->addrof(cg) : nullptr);
	if (!addr) {
		return nullptr;
	}
	return addr->type()->deref();
}

Maybe<AstConst> AstCastExpr::eval_value(Cg& cg) const noexcept {
	auto operand = detuple(m_operand);
	auto value = operand->eval_value(cg);
	if (!value) {
		return None{};
	}
	// TODO(dweiler): constant casting
	return AstConst { value->range(), *value->to<Uint64>() };
}

Maybe<CgAddr> AstCastExpr::gen_addr(Cg& cg, CgType* want) const noexcept {
	auto value = gen_value(cg, want ? want->deref() : nullptr);
	if (!value) {
		return None{};
	}
	if (value->type()->is_pointer()) {
		auto dst = cg.emit_alloca(value->type());
		if (!dst.store(cg, *value)) {
			return None{};
		}
		return dst;
	}
	auto type_string = value->type()->to_string(*cg.scratch);
	return cg.error(range(), "Cannot cast expression with type '%S' to pointer type", type_string);
}

Maybe<CgValue> AstCastExpr::gen_value(Cg& cg, CgType* want) const noexcept {
	auto operand = detuple(m_operand);
	auto src = operand->gen_value(cg, nullptr);
	if (!src) {
		return None{};
	}

	auto dst = gen_type(cg, want);
	if (!dst) {
		return None{};
	}

	auto src_is_signed = src->type()->is_sint();
	auto dst_is_signed = dst->is_sint();

	auto cast_op =
		cg.llvm.GetCastOpcode(src->ref(),
		                      src_is_signed,
		                      dst->ref(),
		                      dst_is_signed);

	auto value = cg.llvm.BuildCast(cg.builder, cast_op, src->ref(), dst->ref(), "");
	return CgValue { dst, value };
}

CgType* AstCastExpr::gen_type(Cg& cg, CgType*) const noexcept {
	if (auto expr = m_type->to_expr<AstTypeExpr>()) {
		return expr->type()->codegen(cg, None{});
	}
	return cg.error(m_type->range(), "Expected type on right-hand side of 'as' operator");
}

Maybe<CgValue> AstTestExpr::gen_value(Cg& cg, CgType*) const noexcept {
	CgType* type = nullptr;
	if (auto expr = m_type->to_expr<AstTypeExpr>()) {
		type = expr->type()->codegen(cg, None{});
		if (!type) {
			return None{};
		}
	} else {
		return cg.error(m_type->range(), "Expected type on left-hand side of 'is' operator");
	}
	auto operand = detuple(m_operand);
	auto expr = operand->gen_addr(cg, nullptr);
	if (!expr) {
		return None{};
	}
	auto expr_type = expr->type()->deref();
	if (expr_type->is_union()) {
		const auto& types = expr_type->types();
		for (Ulen l = types.length(), i = 0; i < l; i++) {
			if (*type != *types[i]) {
				continue;
			}
			auto want = cg.llvm.ConstInt(cg.types.u8()->ref(), i, false);
			auto have = expr->load(cg).at(cg, 1)->ref();
			auto test = cg.llvm.BuildICmp(cg.builder, LLVM::IntPredicate::EQ, have, want, "");

			// We also emit the flow-sensitive alias if the operand was an AstVarExpr
			if (auto var = operand->to_expr<AstVarExpr>()) {
				auto addr = CgAddr { type->addrof(cg), expr->at(cg, 0).ref() };
				if (!cg.scopes.last().tests.emplace_back(this, var->name(), move(addr))) {
					return None{};
				}
			}

			return CgValue { cg.types.b32(), test };
		}
		auto want_type = type->to_string(*cg.scratch);
		auto union_type = expr_type->to_string(*cg.scratch);
		return cg.error(m_type->range(), "The type '%S' is not a variant of '%S'", want_type, union_type);
	}
	auto type_string = expr_type->to_string(*cg.scratch);
	return cg.error(m_operand->range(), "Expected expression of union type on left-hand side of 'is' operator, got '%S' instead", type_string);
}

CgType* AstTestExpr::gen_type(Cg& cg, CgType*) const noexcept {
	if (m_type->is_expr<AstTypeExpr>()) {
		return cg.types.b32();
	}
	return nullptr;
}

Maybe<CgValue> AstPropExpr::gen_value(Cg& cg, CgType* want) const noexcept {
	auto value = eval_value(cg);
	if (!value) {
		return None{};
	}
	return value->codegen(cg, want ? want : cg.types.u64());
}

Maybe<AstConst> AstPropExpr::eval_value(Cg& cg) const noexcept {
	CgType* type = nullptr;
	// Convert the AstVarExpr to AstIdentType to allow `prop of T`
	if (auto expr = m_expr->to_expr<AstVarExpr>()) {
		type = AstIdentType{expr->name(), *cg.scratch, m_expr->range()}.codegen(cg, None{});
	} else {
		type = m_expr->gen_type(cg, nullptr);
	}
	if (!type) {
		return None{};
	}
	auto prop = m_prop->to_expr<AstVarExpr>();
	if (!prop) {
		return cg.error(m_prop->range(), "Expected property on left-hand side of 'of' operator");
	}
	auto name = prop->name();
	auto range = m_prop->range().include(m_expr->range());
	if (name == "size") {
		return AstConst { range, Uint64(type->size()) };
	} else if (name == "align") {
		return AstConst { range, Uint64(type->align()) };
	} else if (name == "count") {
		return AstConst { range, Uint64(type->extent()) };
	}
	return cg.error(m_prop->range(), "Unknown property '%S'", name);
}

CgType* AstPropExpr::gen_type(Cg& cg, CgType* want) const noexcept {
	// The OF operator always returns some integer constant expression except
	// for "type of"
	if (auto prop = m_prop->to_expr<const AstVarExpr>()) {
		if (prop->name() == "type") {
			return m_expr->gen_type(cg, want);
		} else {
			// size  of => u64
			// align of => u64
			// count of => u64
			// pad   of => u64
			return want ? want : cg.types.u64();
		}
	} else {
		return cg.error(m_prop->range(), "Expected property on left-hand side of 'of' operator");
	}
	return want ? want : cg.types.u64();
}

} // namespace Biron