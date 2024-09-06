#include <biron/ast_unit.h>
#include <biron/ast_type.h>
#include <biron/ast_stmt.h>
#include <biron/ast_attr.h>

#include <biron/cg.h>
#include <biron/cg_value.h>

namespace Biron {

Bool CgScope::emit_defers(Cg& cg) const noexcept {
	for (Ulen l = defers.length(), i = l - 1; i < l; i--) {
		if (!defers[i]->codegen(cg)) {
			return false;
		}
	}
	return true;
}

Bool AstFn::prepass(Cg& cg) const noexcept {
	auto rets_t = m_rets->codegen(cg);
	if (!rets_t) {
		return false;
	}

	auto args_t = m_args->codegen(cg);
	if (!args_t) {
		return false;
	}

	CgType* selfs_t = nullptr;
	if (m_selfs) {
		selfs_t = m_selfs->codegen(cg);
		if (!selfs_t) {
			return false;
		}
	}

	auto fn_t = cg.types.make(CgType::FnInfo { selfs_t, args_t, rets_t });
	if (!fn_t) {
		return false;
	}

	// Check for the export attribute. When present and true we do not use nameof,
	// which will typically mangle the name to add the module name.
	const char* name = nullptr;
	if (m_attrs) for (auto attr : *m_attrs) {
		if (attr->name() == "export") {
			auto eval = attr->eval(cg);
			if (!eval || !eval->is_bool()) {
				cg.error(eval->range(), "Expected boolean constant expression for attribute");
				return false;
			}
			auto value = eval->to<Bool>();
			if (value) {
				name = m_name.terminated(*cg.scratch);
				if (!name) {
					cg.oom();
					return false;
				}
			}
			break;
		}
	}
	if (!name) {
		// Use the mangled name
		name = cg.nameof(m_name);
	}

	LLVM::ValueRef fn_v = nullptr;
	StringView builtin = "__biron_runtime_";
	if (m_name.starts_with(builtin)) {
		fn_v = cg.intrinsic(m_name.slice(builtin.length()))->ref();
	} else {
		fn_v = cg.llvm.AddFunction(cg.module, name, fn_t->ref());
	}

	if (m_attrs) for (auto attr : *m_attrs) {
		if (attr->name() == "redzone") {
			auto eval = attr->eval(cg);
			if (!eval || !eval->is_bool()) {
				cg.error(eval->range(), "Expected boolean constant expression for attribute");
				return false;
			}
			auto value = eval->to<Bool>();
			if (value) continue;
			const StringView name = "noredzone";
			auto kind = cg.llvm.GetEnumAttributeKindForName(name.data(), name.length());
			auto data = cg.llvm.CreateEnumAttribute(cg.context, kind, 0);
			cg.llvm.AddAttributeAtIndex(fn_v, -1, data);
		} else if (attr->name() == "alignstack") {
			auto eval = attr->eval(cg);
			if (!eval || !eval->is_integral()) {
				cg.error(eval->range(), "Expected integer constant expression for attribute");
				return false;
			}
			const StringView name = "alignstack";
			auto kind = cg.llvm.GetEnumAttributeKindForName(name.data(), name.length());
			auto data = cg.llvm.CreateEnumAttribute(cg.context, kind, *eval->to<Uint64>());
			cg.llvm.AddAttributeAtIndex(fn_v, -1, data);
		}
	}

	if (!cg.fns.emplace_back(this, m_name, CgAddr { fn_t->addrof(cg), fn_v })) {
		return false;
	}

	return true;
}

Bool AstFn::codegen(Cg& cg) const noexcept {
	// When starting a new function we expect cg.scopes is empty
	cg.scopes.clear();
	if (!cg.scopes.emplace_back(cg.allocator)) {
		return false;
	}

	// Search for the function by node
	Maybe<CgAddr> addr;
	for (const auto &var : cg.fns) {
		if (var.node() == this) {
			addr = var.addr();
			break;
		}
	}

	if (!addr) {
		return false;
	}

	auto type = addr->type()->deref();
	auto rets = type->at(2);

	// Construct the entry basic-block, append it to the function and position
	// the IR builder at the end of the basic-block.
	auto block = cg.llvm.CreateBasicBlockInContext(cg.context, "entry");
	cg.llvm.AppendExistingBasicBlock(addr->ref(), block);
	cg.llvm.PositionBuilderAtEnd(cg.builder, block);

	// On entry to the function we will make a copy of all named parameters and
	// populate cg.vars with the addresses of those copies.
	Ulen i = 0;
	if (m_selfs) {
		for (const auto& elem : m_selfs->elems()) {
			if (auto name = elem.name()) {
				auto type = elem.type()->codegen(cg);
				if (!type) {
					return false;
				}
				auto dst = cg.emit_alloca(type);
				auto src = cg.llvm.GetParam(addr->ref(), i);
				dst->store(cg, CgValue { type, src });
				if (!cg.scopes.last().vars.emplace_back(this, *name, move(*dst))) {
					return false;
				}
			}
			i++;
		}
	}

	for (const auto& elem : m_args->elems()) {
		if (auto name = elem.name()) {
			auto type = elem.type()->codegen(cg);
			if (!type) {
				return false;
			}
			auto dst = cg.emit_alloca(type);
			auto src = cg.llvm.GetParam(addr->ref(), i);
			dst->store(cg, CgValue { type, src });
			if (!cg.scopes.last().vars.emplace_back(this, *name, move(*dst))) {
				return false;
			}
		}
		i++;
	}

	if (!m_body->codegen(cg)) {
		return false;
	}

	// Obtain the current block we're writing into since it might've moved as
	// a result of generating the block statement which represents the function
	// body.
	block = cg.llvm.GetInsertBlock(cg.builder);

	// When the block doesn't contain a terminator we will need to emit a return.
	if (!cg.llvm.GetBasicBlockTerminator(block)) {
		if (rets->length() == 0) {
			// The empty tuple () is our void type. We can emit RetVoid.
			cg.llvm.BuildRetVoid(cg.builder);
			return true;
		} else if (rets->length() == 1) {
			if (auto value = CgValue::zero(rets->at(0), cg)) {
				// When there is just a single element in our tuple we detuple it in
				// AstTupleExpr so we must also detuple it here in the return too.
				cg.llvm.BuildRet(cg.builder, value->ref());
				return true;
			}
		} else if (auto value = CgValue::zero(rets, cg)) {
			// Otherwise we return a zeroed tuple which matches the return type.
			cg.llvm.BuildRet(cg.builder, value->ref());
			return true;
		}
		return false;
	}

	return cg.scopes.pop_back();
}

Bool AstTypedef::codegen(Cg& cg) const noexcept {
	if (m_generated) {
		return true;
	}
	auto type = m_type->codegen_named(cg, m_name);
	if (!type) {
		return false;
	}
	if (!cg.typedefs.emplace_back(m_name, type)) {
		return false;
	}
	m_generated = true;
	return true;
}

Bool AstModule::codegen(Cg& cg) const noexcept {
	if (m_name == "intrinsics") {
		cg.error(range(), "Module cannot be named 'intrinsics'");
		return false;
	}
	cg.prefix = m_name;
	return true;
}


Bool AstUnit::codegen(Cg& cg) const noexcept {
	if (!m_module) {
		cg.error(Range{0, 0}, "Missing 'module'");
		return false;
	}

	if (!m_module->codegen(cg)) {
		return false;
	}

	cg.unit = this;
	
	// Register a "printf" function for debugging purposes
	{
		Array<CgType*> args{cg.allocator};
		if (!args.push_back(cg.types.ptr()) || !args.push_back(cg.types.va())) {
			return false;
		}
		Array<CgType*> rets{cg.allocator};
		if (!rets.push_back(cg.types.s32())) {
			return false;
		}
		auto args_t = cg.types.make(CgType::TupleInfo { move(args), None{}, None{} });
		auto rets_t = cg.types.make(CgType::TupleInfo { move(rets), None{}, None{} });
		if (!args_t || !rets_t) {
			return false;
		}

		auto fn_t = cg.types.make(CgType::FnInfo { nullptr, args_t, rets_t });
		if (!fn_t) {
			return false;
		}
		auto fn_v = cg.llvm.AddFunction(cg.module, "printf", fn_t->ref());
		if (!cg.fns.emplace_back(nullptr, "printf", CgAddr{fn_t->addrof(cg), fn_v})) {
			return false;
		}
	}

	// Register "enable_fpu" (hack)
	{
		Array<CgType*> args{cg.allocator};
		Array<CgType*> rets{cg.allocator};
		auto args_t = cg.types.make(CgType::TupleInfo { move(args), None{}, None{} });
		auto rets_t = cg.types.make(CgType::TupleInfo { move(rets), None{}, None{} });
		if (!args_t || !rets_t) {
			return false;
		}
		auto fn_t = cg.types.make(CgType::FnInfo { nullptr, args_t, rets_t });
		if (!fn_t) {
			return false;
		}
		auto fn_v = cg.llvm.AddFunction(cg.module, "enable_fpu", fn_t->ref());
		if (!cg.fns.emplace_back(nullptr, "enable_fpu", CgAddr{fn_t->addrof(cg), fn_v})) {
			return false;
		}
	}

	// sqrt
	{
		Array<CgType*> args{cg.allocator};
		Array<CgType*> rets{cg.allocator};
		if (!args.push_back(cg.types.f32())) {
			return false;
		}
		if (!rets.push_back(cg.types.f32())) {
			return false;
		}
		auto args_t = cg.types.make(CgType::TupleInfo { move(args), None{}, None{} });
		auto rets_t = cg.types.make(CgType::TupleInfo { move(rets), None{}, None{} });
		if (!args_t || !rets_t) {
			return false;
		}
		auto fn_t = cg.types.make(CgType::FnInfo { nullptr, args_t, rets_t });
		if (!fn_t) {
			return false;
		}
		auto fn_v = cg.llvm.AddFunction(cg.module, "llvm.sqrt.f32", fn_t->ref());
		if (!cg.fns.emplace_back(nullptr, "sqrt", CgAddr{fn_t->addrof(cg), fn_v})) {
			return false;
		}
	}

	// Register memory_ne and memory_eq intrinsic
	{
		Array<CgType*> args{cg.allocator};
		if (!args.resize(3)) {
			return false;
		}
		args[0] = cg.types.ptr();
		args[1] = cg.types.ptr();
		args[2] = cg.types.u64();
		Array<CgType*> rets{cg.allocator};
		if (!rets.push_back(cg.types.b32())) {
			return false;
		}
		auto args_t = cg.types.make(CgType::TupleInfo { move(args), None{}, None{} });
		auto rets_t = cg.types.make(CgType::TupleInfo { move(rets), None{}, None{} });
		if (!args_t || !rets_t) {
			return false;
		}
		auto fn_t = cg.types.make(CgType::FnInfo { nullptr, args_t, rets_t });
		if (!fn_t) {
			return false;
		}
		auto fn_v = cg.llvm.AddFunction(cg.module, "__biron_runtime_memory_ne", fn_t->ref());
		if (!cg.intrinsics.emplace_back(nullptr, "memory_ne", CgAddr { fn_t->addrof(cg), fn_v })) {
			return false;
		}
		// We can just reuse all of the same things
		fn_v = cg.llvm.AddFunction(cg.module, "__biron_runtime_memory_eq", fn_t->ref());
		if (!cg.intrinsics.emplace_back(nullptr, "memory_eq", CgAddr { fn_t->addrof(cg), fn_v })) {
			return false;
		}
	}

	// Emit all the global constants first since types may depend on them for
	// e.g array extents and what not.
	for (auto let : m_lets) {
		cg.scratch->clear();

		if (!let->codegen_global(cg)) {
			return false;
		}
	}

	// Emit all the types next. Each type will recurse and resolve their types
	// and mark the type as already being generated so that this main loop does
	// not generate the same type twice. This is how we do a topological sort.
	for (auto type : m_typedefs) {
		cg.scratch->clear();

		if (!type->codegen(cg)) {
			return false;
		}
	}

	// Before we codegen functions we do a preprocessing step to make sure all
	// functions have values generated for them so that we do not need function
	// prototypes in our language.
	for (auto fn : m_fns) {
		cg.scratch->clear();

		if (!fn->prepass(cg)) {
			return false;
		}
	}

	// We can then codegen functions in any order we so desire.
	for (auto fn : m_fns) {
		cg.scratch->clear();

		if (!fn->codegen(cg)) {
			return false;
		}
	}

	return true;
}

void AstUnit::dump(StringBuilder& builder) const noexcept {
	for (auto fn : m_fns) {
		fn->dump(builder, 0);
	}
}

} // namespace Biron