#include <biron/ast_unit.h>
#include <biron/ast_type.h>
#include <biron/ast_stmt.h>
#include <biron/ast_attr.h>
#include <biron/ast.h>

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
	auto objs = m_objs->codegen(cg, None{});
	if (!objs) {
		return false;
	}

	auto args = m_args->codegen(cg, None{});
	if (!args) {
		return false;
	}

	// We need to generate a tuple for the effects of this function.
	CgType::TupleInfo info { *cg.scratch, { *cg.scratch }, None {} };
	for (auto effect : m_effects) {
		auto type = effect->codegen(cg, None{});
		if (!type) {
			return false;
		}
		if (!info.types.push_back(type)) {
			return false;
		}
		if (!info.fields->emplace_back(effect->name(), None{})) {
			return false;
		}
	}
	auto effects = info.types.empty()
		? cg.types.unit()
		: cg.types.make(move(info));
	if (!effects) {
		return false;
	}

	auto ret = m_ret->codegen(cg, None{});
	if (!ret) {
		return false;
	}

	auto fn_t = cg.types.make(CgType::FnInfo { objs, args, effects, ret });
	if (!fn_t) {
		return false;
	}

	// Check for the export attribute. When present and true we do not use nameof,
	// which will typically mangle the name to add the module name.
	const char* name = nullptr;
	Bool exported = false;
	for (auto attr : m_attrs) {
		if (attr->name() == "export") {
			auto eval = attr->eval(cg);
			if (!eval || !eval->is_bool()) {
				return cg.error(eval->range(), "Expected boolean constant expression for attribute");
			}
			auto value = eval->to<Bool>();
			if (value) {
				name = m_name.terminated(*cg.scratch);
				if (!name) {
					return cg.oom();
				}
				exported = true;
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

	if (exported) {
		cg.llvm.SetLinkage(fn_v, LLVM::Linkage::External);
	} else {
		cg.llvm.SetLinkage(fn_v, LLVM::Linkage::Private);
	}

	for (auto attr : m_attrs) {
		if (attr->name() == "redzone") {
			auto eval = attr->eval(cg);
			if (!eval || !eval->is_bool()) {
				return cg.error(eval->range(), "Expected boolean constant expression for attribute");
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
				return cg.error(eval->range(), "Expected integer constant expression for attribute");
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
	BIRON_ASSERT(cg.scopes.empty());

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

	if (!cg.scopes.emplace_back(cg.allocator)) {
		return false;
	}

	cg.fn = this;

	auto type = addr->type()->deref();
	auto effects = type->at(2);
	auto ret = type->at(3);

	// Construct the entry basic-block, append it to the function and position
	// the IR builder at the end of the basic-block.
	auto entry_bb = cg.llvm.CreateBasicBlockInContext(cg.context, "entry");
	cg.llvm.AppendExistingBasicBlock(addr->ref(), entry_bb);
	cg.llvm.PositionBuilderAtEnd(cg.builder, entry_bb);

	cg.entry = entry_bb;

	// On entry to the function we will allocate storage for all objs and args.
	struct Arg {
		CgAddr addr;
		Ulen   index;
	};

	Array<Arg> args{*cg.scratch};

	// When we have effects the first argument is the effect tuple
	Ulen i = 0;
	if (effects != cg.types.unit()) {
		i++;
	}
	for (const auto& elem : m_objs->elems()) {
		if (auto name = elem.name()) {
			auto type = elem.type()->codegen(cg, None{});
			if (!type) {
				return false;
			}
			auto dst = cg.emit_alloca(type);
			if (!args.emplace_back(dst, i)) {
				return false;
			}
			if (!cg.scopes.last().vars.emplace_back(this, *name, move(dst))) {
				return false;
			}
		}
		i++;
	}

	for (const auto& elem : m_args->elems()) {
		if (auto name = elem.name()) {
			auto type = elem.type()->codegen(cg, None{});
			if (!type) {
				return false;
			}
			auto dst = cg.emit_alloca(type);
			if (!args.emplace_back(dst, i)) {
				return false;
			}
			if (!cg.scopes.last().vars.emplace_back(this, *name, move(dst))) {
				return false;
			}
		}
		i++;
	}

	auto join_bb = cg.llvm.CreateBasicBlockInContext(cg.context, "join");
	cg.llvm.AppendExistingBasicBlock(addr->ref(), join_bb);
	cg.llvm.PositionBuilderAtEnd(cg.builder, join_bb);

	// Populate our effects which are passed by pointer so we can synthesize address
	// to them directly here.
	if (effects != cg.types.unit()) {
		// The type of src is effects->addrof(cg)
		auto src = CgAddr { effects->addrof(cg), cg.llvm.GetParam(addr->ref(), 0) };
		// Populate the using for this scope
		Ulen i = 0;
		for (const auto& field : effects->fields()) {
			auto field_addr = src.at(cg, i);
			if (field.name && !cg.scopes.last().usings.emplace_back(this, *field.name, move(field_addr))) {
				return false;
			}
			i++;
		}
	}

	// Once we have storage allocated for all objs and args we will make a copy of
	// the function parameters into them inside our join block.
	for (auto& arg : args) {
		auto src = cg.llvm.GetParam(addr->ref(), arg.index);
		arg.addr.store(cg, CgValue { arg.addr.type()->deref(), src });
	}

	// We can then emit the body.
	if (!m_body->codegen(cg)) {
		return false;
	}

	// Obtain the current block we're writing into since it might've moved as
	// a result of generating the block statement which represents the function
	// body.
	auto resume_bb = cg.llvm.GetInsertBlock(cg.builder);

	// The entry block up to this point has been used by emit_alloca to emit all
	// the allocas. We do it this way because LLVM says allocas should only be
	// emitted to the entry block for optimization purposes.
	//
	// The entry block now needs a terminator though. So emit a branch from the
	// end of it to the start of our join block which is the true entry block of
	// the function.
	//
	// This block mostly only contains some stores of function arguments to the
	// alloca copies since the body itself has an implicitly generated block
	// too.
	cg.llvm.PositionBuilderAtEnd(cg.builder, entry_bb);
	cg.llvm.BuildBr(cg.builder, join_bb);

	// We now position the builder back to resume_bb so we can emit an implicit
	// return statement if any are needed
	cg.llvm.PositionBuilderAtEnd(cg.builder, resume_bb);

	// When the block doesn't contain a terminator we will need to emit a return.
	if (!cg.llvm.GetBasicBlockTerminator(resume_bb)) {
		if (ret->is_tuple() && ret->length() == 0) {
			// The zero-element tuple is our void type
			cg.llvm.BuildRetVoid(cg.builder);
		} else if (ret->is_tuple() && ret->length() == 1) {
			// We detuple single element tuples
			if (auto value = CgValue::zero(ret->at(0), cg)) {
				cg.llvm.BuildRet(cg.builder, value->ref());
			} else {
				return false;
			}
		} else {
			if (auto value = CgValue::zero(ret, cg)) {
				cg.llvm.BuildRet(cg.builder, value->ref());
			} else {
				return false;
			}
		}
	}

	return cg.scopes.pop_back();
}

Bool AstTypedef::codegen(Cg& cg) const noexcept {
	if (m_generated) {
		return true;
	}
	auto type = m_type->codegen(cg, m_name);
	if (!type) {
		return false;
	}	
	if (!cg.typedefs.emplace_back(m_name, type)) {
		return false;
	}
	m_generated = true;
	return true;
}

Bool AstEffect::codegen(Cg& cg) const noexcept {
	if (m_generated) {
		return true;
	}
	auto type = m_type->codegen(cg, None{});
	if (!type) {
		return false;
	}
	if (!cg.effects.emplace_back(m_name, type)) {
		return false;
	}
	m_generated = true;
	return true;
}

Bool AstModule::codegen(Cg& cg) const noexcept {
	if (m_name == "intrinsics") {
		return cg.error(range(), "Module cannot be named 'intrinsics'");
	}
	cg.prefix = m_name;
	return true;
}

Bool Ast::codegen(Cg& cg) const noexcept {
	// We should have at least one top-level module.
	const auto modules = cache<AstModule>();
	if (!modules || modules->empty()) {
		return cg.error(Range{0, 0}, "Missing 'module'");
	}
	if (modules->length() > 1) {
		return cg.error(Range{0, 0}, "Multiple 'module' definitions in file");
	}
	auto module = static_cast<const AstModule*>((*modules)[0]);
	if (!module->codegen(cg)) {
		return false;
	}

	// Somewhat frustrating this is needed until we add a dependency sorter.
	cg.ast = this;
	
	// Register a "printf" function for debugging purposes
	{
		Array<CgType*> args{cg.allocator};
		if (!args.push_back(cg.types.u8()->addrof(cg)) || !args.push_back(cg.types.va())) {
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

		auto fn_t = cg.types.make(CgType::FnInfo { cg.types.unit(), args_t, cg.types.unit(), rets_t });
		if (!fn_t) {
			return false;
		}
		auto fn_v = cg.llvm.AddFunction(cg.module, "printf", fn_t->ref());
		if (!cg.fns.emplace_back(nullptr, "printf", CgAddr { fn_t->addrof(cg), fn_v })) {
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
		auto ret_t = cg.types.f32();
		if (!args_t || !ret_t) {
			return false;
		}
		auto fn_t = cg.types.make(CgType::FnInfo { cg.types.unit(), args_t, cg.types.unit(), ret_t });
		if (!fn_t) {
			return false;
		}
		auto fn_v = cg.llvm.AddFunction(cg.module, "llvm.sqrt.f32", fn_t->ref());
		if (!cg.fns.emplace_back(nullptr, "sqrt", CgAddr { fn_t->addrof(cg), fn_v })) {
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
		auto fn_t = cg.types.make(CgType::FnInfo { cg.types.unit(), args_t, cg.types.unit(), rets_t });
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

	// Emit all the global let statements first since types may depend on them for
	// e.g array extents and what not.
	if (const auto glets = cache<AstGLetStmt>()) {
		for (auto let : *glets) {
			cg.scratch->clear();
			if (!static_cast<const AstGLetStmt*>(let)->codegen(cg)) {
				return false;
			}
		}
	}

	// Emit all the types next. Each type will recurse and resolve their types
	// and mark the type as already being generated so that this main loop does
	// not generate the same type twice. This is how we do a topological sort.
	if (const auto typedefs = cache<AstTypedef>()) {
		for (auto type : *typedefs) {
			cg.scratch->clear();
			if (!static_cast<const AstTypedef*>(type)->codegen(cg)) {
				return false;
			}
		}
	}

	// Emit all the effects next.
	if (const auto effects = cache<AstEffect>()) {
		for (auto effect : *effects) {
			cg.scratch->clear();
			if (!static_cast<const AstEffect*>(effect)->codegen(cg)) {
				return false;
			}
		}
	}

	if (const auto fns = cache<AstFn>()) {
		// Before we codegen functions we do a preprocessing step to make sure all
		// functions have values generated for them so that we do not need function
		// prototypes in our language.
		for (auto fn : *fns) {
			cg.scratch->clear();
			if (!static_cast<const AstFn*>(fn)->prepass(cg)) {
				return false;
			}
		}
		// We can then codegen functions in any order we so desire.
		for (auto fn : *fns) {
			cg.scratch->clear();
			if (!static_cast<const AstFn*>(fn)->codegen(cg)) {
				return false;
			}
		}
	}

	return true;
}

} // namespace Biron