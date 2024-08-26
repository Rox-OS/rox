#include <stdio.h>

#include <biron/ast_unit.h>
#include <biron/ast_type.h>
#include <biron/ast_stmt.h>

#include <biron/cg.h>
#include <biron/cg_value.h>

namespace Biron {

Bool AstTopFn::prepass(Cg& cg) const noexcept {
	auto rets_t = m_rets->codegen(cg);
	if (!rets_t) {
		return false;
	}

	auto args_t = m_args->codegen(cg);
	if (!args_t) {
		return false;
	}

	auto fn_t = cg.types.alloc(CgType::FnInfo { args_t, rets_t });
	if (!fn_t) {
		return false;
	}

	auto fn_v = cg.llvm.AddFunction(cg.module,
	                                m_name.terminated(cg.allocator),
	                                fn_t->ref(cg));

	if (!cg.fns.emplace_back(m_name, CgAddr{fn_t->addrof(cg), fn_v})) {
		return false;
	}

	return true;
}

Bool AstTopFn::codegen(Cg& cg) const noexcept {
	// When starting a new function we expect cg.scopes is empty
	BIRON_ASSERT(cg.scopes.empty());
	if (!cg.scopes.emplace_back(cg.allocator)) {
		return false;
	}

	// Search for the function by name
	Maybe<CgAddr> addr;
	for (const auto &var : cg.fns) {
		if (var.name() == m_name) {
			addr = var.addr();
			break;
		}
	}

	if (!addr) {
		return false;
	}

	auto type = addr->type()->deref();
	auto rets = type->at(1);

	// Construct the entry basic-block, append it to the function and position
	// the IR builder at the end of the basic-block.
	auto block = cg.llvm.CreateBasicBlockInContext(cg.context, "entry");
	if (!block) {
		return false;
	}
	cg.llvm.AppendExistingBasicBlock(addr->ref(), block);
	cg.llvm.PositionBuilderAtEnd(cg.builder, block);

	// On entry to the function we will make a copy of all named parameters and
	// populate cg.vars with the addresses of those copies.
	Ulen i = 0;
	for (const auto& elem : m_args->elems()) {
		if (auto name = elem.name()) {
			auto dst = cg.emit_alloca(elem.type()->codegen(cg));
			auto src = cg.llvm.GetParam(addr->ref(), i);
			dst->store(cg, CgValue { elem.type()->codegen(cg), src });
			if (!cg.scopes.last().vars.emplace_back(*name, move(*dst))) {
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

Bool AstUnit::codegen(Cg& cg) const noexcept {
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
		auto args_t = cg.types.alloc(CgType::TupleInfo { move(args) });
		auto rets_t = cg.types.alloc(CgType::TupleInfo { move(rets) });
		if (!args_t || !rets_t) {
			return false;
		}

		auto fn_t = cg.types.alloc(CgType::FnInfo { args_t, rets_t });
		if (!fn_t) {
			return false;
		}
		auto fn_v = cg.llvm.AddFunction(cg.module, "printf", fn_t->ref(cg));
		if (!cg.fns.emplace_back("printf", CgAddr{fn_t->addrof(cg), fn_v})) {
			return false;
		}
	}

	// Emit all the global constants
	for (auto let : m_lets) {
		if (!let->codegen_global(cg)) {
			return false;
		}
	}

	// Before we codegen we do a preprocessing step to make sure all functions
	// have values generated for them so that we do not need function prototypes
	// in our language.
	for (auto fn : m_fns) {
		if (!fn->prepass(cg)) {
			return false;
		}
	}

	// We can then codegen in any order we so desire.
	for (auto fn : m_fns) {
		if (!fn->codegen(cg)) {
			return false;
		}
	}

	return true;
}

} // namespace Biron