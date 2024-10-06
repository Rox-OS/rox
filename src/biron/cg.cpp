#include <biron/cg.h>
#include <biron/cg_value.h>

#include <biron/util/system.inl>
#include <biron/util/terminal.inl>

namespace Biron {

// [CgMachine]
static LLVM::TargetRef target_from_triple(Terminal& terminal,
                                          LLVM& llvm,
                                          const char* triple) noexcept
{
	LLVM::TargetRef target;
	char* error = nullptr;
	if (llvm.GetTargetFromTriple(triple, &target, &error) != 0) {
		terminal.err("Could not find target: %s\n", error);
		llvm.DisposeMessage(error);
		return nullptr;
	}
	return target;
}

Maybe<CgMachine> CgMachine::make(Terminal& terminal,
                                 LLVM& llvm,
                                 StringView target_triple) noexcept
{
	// Target triple cannot get too large so use an on-stack inline allocator.
	InlineAllocator<1024> scratch;
	auto triple = target_triple.terminated(scratch);
	if (!triple) {
		return None{};
	}

	auto target = target_from_triple(terminal, llvm, triple);
	if (!target) {
		return None{};
	}

	auto machine = llvm.CreateTargetMachine(target,
	                                        triple,
	                                        "generic",
	                                        "",
	                                        LLVM::CodeGenOptLevel::Aggressive,
	                                        LLVM::RelocMode::PIC,
	                                        LLVM::CodeModel::Kernel);

	if (!machine) {
		return None{};
	}

	return CgMachine { llvm, machine };
}

CgMachine::~CgMachine() noexcept {
	if (m_machine) {
		m_llvm.DisposeTargetMachine(m_machine);
	}
}

// [Cg]
Maybe<Cg> Cg::make(Terminal& terminal,
                   Allocator& allocator,
                   LLVM& llvm,
                   Diagnostic& diagnostic) noexcept
{
	auto context = llvm.ContextCreate();
	auto builder = llvm.CreateBuilderInContext(context);
	auto module  = llvm.ModuleCreateWithNameInContext("Biron", context);

	if (!context || !builder || !module) {
		if (module)  llvm.DisposeModule(module);
		if (builder) llvm.DisposeBuilder(builder);
		if (context) llvm.ContextDispose(context);
		return None{};
	}

	auto types = CgTypeCache::make(allocator, llvm, context, 1024);
	if (!types) {
		return None{};
	}

	auto scratch = allocator.make<ScratchAllocator>(allocator);
	if (!scratch) {
		return None{};
	}

	return Cg {
		terminal,
		allocator,
		llvm,
		scratch,
		context,
		builder,
		module,
		move(*types),
		diagnostic
	};
}

Bool Cg::optimize(CgMachine& machine, Ulen level) noexcept {
	if (!verify()) {
		return false;
	}
	auto options = llvm.CreatePassBuilderOptions();
	LLVM::ErrorRef result = nullptr;
	switch (level) {
	case 0:
		result = llvm.RunPasses(module, "default<O0>", machine.ref(), options);
		break;
	case 1:
		result = llvm.RunPasses(module, "default<O1>", machine.ref(), options);
		break;
	case 2:
		result = llvm.RunPasses(module, "default<O2>", machine.ref(), options);
		break;
	case 3:
		result = llvm.RunPasses(module, "default<O3>", machine.ref(), options);
		break;
	}
	llvm.DisposePassBuilderOptions(options);
	if (result) {
		llvm.ConsumeError(result);
		return false;
	}
	return verify();
}

Bool Cg::verify() noexcept {
	char* error = nullptr;
	if (llvm.VerifyModule(module,
	                      LLVM::VerifierFailureAction::ReturnStatus,
	                      &error) != 0)
	{
		m_terminal.err("Could not verify module: %s\n", error);
		llvm.DisposeMessage(error);
		return dump();
	}
	llvm.DisposeMessage(error);
	return true;
}

Bool Cg::dump() noexcept {
	llvm.DumpModule(module);
	return true;
}

Bool Cg::emit(CgMachine& machine, StringView name) noexcept {
	char* error = nullptr;
	if (!verify()) {
		return false;
	}
	auto terminated = name.terminated(*scratch);
	if (!terminated) {
		m_terminal.err("Out of memory\n");
		return false;
	}
	if (llvm.TargetMachineEmitToFile(machine.ref(),
	                                 module,
	                                 terminated,
	                                 LLVM::CodeGenFileType::Object,
	                                 &error) != 0)
	{
		m_terminal.err("Could not compile module '%S': %s\n", name, error);
		llvm.DisposeMessage(error);
		return false;
	}
	llvm.DisposeMessage(error);
	return true;
}

CgAddr Cg::emit_alloca(CgType* type) noexcept {
	// Emit the alloca at the end of the entry basic block.
	auto block = llvm.GetInsertBlock(builder);
	llvm.PositionBuilderAtEnd(builder, entry);
	auto value = llvm.BuildAlloca(builder, type->ref(), "");
	// Restore the builder to the current block.
	llvm.PositionBuilderAtEnd(builder, block);
	// We may have a higher alignment requirement than what Alloca will pick.
	llvm.SetAlignment(value, type->align());
	return CgAddr { type->addrof(*this), value };
}

Maybe<CgValue> Cg::emit_lt(const CgValue& lhs, const CgValue& rhs, Range range) noexcept {
	if (lhs.type()->is_sint()) {
		auto value = llvm.BuildICmp(builder, LLVM::IntPredicate::SLT, lhs.ref(), rhs.ref(), "");
		return CgValue { types.b32(), value };
	} else if (lhs.type()->is_uint()) {
		auto value = llvm.BuildICmp(builder, LLVM::IntPredicate::ULT, lhs.ref(), rhs.ref(), "");
		return CgValue { types.b32(), value };
	} else if (lhs.type()->is_real()) {
		auto value = llvm.BuildFCmp(builder, LLVM::RealPredicate::OLT, lhs.ref(), rhs.ref(), "");
		return CgValue { types.b32(), value };
	}
	auto lhs_type_string = lhs.type()->to_string(*scratch);
	return error(range,
	             "Operands to '<' operator must have numeric type. Got '%S' instead",
	             lhs_type_string);
}

Maybe<CgValue> Cg::emit_le(const CgValue& lhs, const CgValue& rhs, Range range) noexcept {
	if (lhs.type()->is_sint()) {
		auto value = llvm.BuildICmp(builder, LLVM::IntPredicate::SLE, lhs.ref(), rhs.ref(), "");
		return CgValue { types.b32(), value };
	} else if (lhs.type()->is_uint()) {
		auto value = llvm.BuildICmp(builder, LLVM::IntPredicate::ULE, lhs.ref(), rhs.ref(), "");
		return CgValue { types.b32(), value };
	} else if (lhs.type()->is_real()) {
		auto value = llvm.BuildFCmp(builder, LLVM::RealPredicate::OLE, lhs.ref(), rhs.ref(), "");
		return CgValue { types.b32(), value };
	}
	auto lhs_type_string = lhs.type()->to_string(*scratch);
	return error(range,
	             "Operands to '<=' operator must have numeric type. Got '%S' instead",
	             lhs_type_string);
}

Maybe<CgValue> Cg::emit_gt(const CgValue& lhs, const CgValue& rhs, Range range) noexcept {
	if (lhs.type()->is_sint()) {
		auto value = llvm.BuildICmp(builder, LLVM::IntPredicate::SGT, lhs.ref(), rhs.ref(), "");
		return CgValue { types.b32(), value };
	} else if (lhs.type()->is_uint()) {
		auto value = llvm.BuildICmp(builder, LLVM::IntPredicate::UGT, lhs.ref(), rhs.ref(), "");
		return CgValue { types.b32(), value };
	} else if (lhs.type()->is_real()) {
		auto value = llvm.BuildFCmp(builder, LLVM::RealPredicate::OGT, lhs.ref(), rhs.ref(), "");
		return CgValue { types.b32(), value };
	}
	auto lhs_type_string = lhs.type()->to_string(*scratch);
	return error(range,
	             "Operands to '>' operator must have numeric type. Got '%S' instead",
	             lhs_type_string);
}

Maybe<CgValue> Cg::emit_ge(const CgValue& lhs, const CgValue& rhs, Range range) noexcept {
	if (lhs.type()->is_sint()) {
		auto value = llvm.BuildICmp(builder, LLVM::IntPredicate::SGE, lhs.ref(), rhs.ref(), "");
		return CgValue { types.b32(), value };
	} else if (lhs.type()->is_uint()) {
		auto value = llvm.BuildICmp(builder, LLVM::IntPredicate::UGE, lhs.ref(), rhs.ref(), "");
		return CgValue { types.b32(), value };
	} else if (lhs.type()->is_real()) {
		auto value = llvm.BuildFCmp(builder, LLVM::RealPredicate::OGE, lhs.ref(), rhs.ref(), "");
		return CgValue { types.b32(), value };
	}
	auto lhs_type_string = lhs.type()->to_string(*scratch);
	return error(range,
	             "Operands to '>=' operator must have numeric type. Got '%S' instead",
	             lhs_type_string);
}

Maybe<CgValue> Cg::emit_add(const CgValue& lhs, const CgValue& rhs, Range range) noexcept {
	if (lhs.type()->is_sint() || lhs.type()->is_uint()) {
		return CgValue { lhs.type(), llvm.BuildAdd(builder, lhs.ref(), rhs.ref(), "") };
	} else if (lhs.type()->is_real()) {
		return CgValue { lhs.type(), llvm.BuildFAdd(builder, lhs.ref(), rhs.ref(), "") };
	} else if (lhs.type()->is_array()) {
		return emit_for_array(lhs, rhs, range, &Cg::emit_add);
	}
	auto lhs_type_string = lhs.type()->to_string(*scratch);
	return error(range,
	             "Operands to '+' operator must have numeric type. Got '%S' instead",
	             lhs_type_string);
}

Maybe<CgValue> Cg::emit_sub(const CgValue& lhs, const CgValue& rhs, Range range) noexcept {
	if (lhs.type()->is_sint() || lhs.type()->is_uint()) {
		return CgValue { lhs.type(), llvm.BuildSub(builder, lhs.ref(), rhs.ref(), "") };
	} else if (lhs.type()->is_real()) {
		return CgValue { lhs.type(), llvm.BuildFSub(builder, lhs.ref(), rhs.ref(), "") };
	} else if (lhs.type()->is_array()) {
		return emit_for_array(lhs, rhs, range, &Cg::emit_sub);
	}
	auto lhs_type_string = lhs.type()->to_string(*scratch);
	return error(range,
	             "Operands to '-' operator must have numeric type. Got '%S' instead",
	             lhs_type_string);
}

Maybe<CgValue> Cg::emit_mul(const CgValue& lhs, const CgValue& rhs, Range range) noexcept {
	if (lhs.type()->is_sint() || lhs.type()->is_uint()) {
		return CgValue { lhs.type(), llvm.BuildMul(builder, lhs.ref(), rhs.ref(), "") };
	} else if (lhs.type()->is_real()) {
		return CgValue { lhs.type(), llvm.BuildFMul(builder, lhs.ref(), rhs.ref(), "") };
	} else if (lhs.type()->is_array()) {
		return emit_for_array(lhs, rhs, range, &Cg::emit_mul);
	}
	auto lhs_type_string = lhs.type()->to_string(*scratch);
	return error(range,
	             "Operands to '*' operator must have numeric type. Got '%S' instead",
	             lhs_type_string);
}

Maybe<CgValue> Cg::emit_div(const CgValue& lhs, const CgValue& rhs, Range range) noexcept {
	if (lhs.type()->is_real()) {
		return CgValue { lhs.type(), llvm.BuildFDiv(builder, lhs.ref(), rhs.ref(), "") };
	} else if (lhs.type()->is_sint()) {
		return CgValue { lhs.type(), llvm.BuildSDiv(builder, lhs.ref(), rhs.ref(), "") };
	} else if (lhs.type()->is_uint()) {
		return CgValue { lhs.type(), llvm.BuildUDiv(builder, lhs.ref(), rhs.ref(), "") };
	} else if (lhs.type()->is_array()) {
		return emit_for_array(lhs, rhs, range, &Cg::emit_div);
	}
	auto lhs_type_string = lhs.type()->to_string(*scratch);
	return error(range,
	             "Operands to '/' operator must have numeric type. Got '%S' instead",
	             lhs_type_string);
}

Maybe<CgValue> Cg::emit_min(const CgValue& lhs, const CgValue& rhs, Range range) noexcept {
	if (auto cmp = emit_lt(lhs, rhs, range)) {
		// lhs < rhs ? lhs : rhs
		return CgValue { lhs.type(), llvm.BuildSelect(builder, cmp->ref(), lhs.ref(), rhs.ref(), "") };
	}
	return None{};
}

Maybe<CgValue> Cg::emit_max(const CgValue& lhs, const CgValue& rhs, Range range) noexcept {
	if (auto cmp = emit_gt(lhs, rhs, range)) {
		// lhs > rhs ? lhs : rhs
		return CgValue { lhs.type(), llvm.BuildSelect(builder, cmp->ref(), lhs.ref(), rhs.ref(), "") };
	}
	return None{};
}

Maybe<CgValue> Cg::emit_for_array(const CgValue& lhs,
                                  const CgValue& rhs,
                                  Range range,
                                  Maybe<CgValue> (Cg::*emit)(const CgValue&,
                                                             const CgValue&,
                                                             Range))
{
	Array<CgValue> values{*scratch};
	if (!values.reserve(lhs.type()->extent())) {
		return oom();
	}
	for (Ulen l = lhs.type()->extent(), i = 0; i < l; i++) {
		auto lhs_n = lhs.at(*this, i);
		auto rhs_n = rhs.at(*this, i);
		if (!lhs_n || !rhs_n) {
			return None{};
		}
		auto value = (this->*emit)(*lhs_n, *rhs_n, range);
		if (!value) {
			return None{};
		}
		(void)values.push_back(*value);
	}
	auto dst = emit_alloca(lhs.type());
	Ulen i = 0;
	for (auto value : values) {
		dst.at(*this, i++).store(*this, value);
	}
	return dst.load(*this);
}

const char* Cg::nameof(StringView name) const noexcept {
	auto dst = reinterpret_cast<char *>(scratch->allocate(prefix.length() + name.length() + 2));
	if (!dst) {
		return "OOM";
	}
	for (Ulen l = prefix.length(), i = 0; i < l; i++) {
		dst[i] = prefix[i];
	}
	dst[prefix.length()] = '.';
	for (Ulen l = name.length(), i = 0; i < l; i++) {
		dst[prefix.length() + 1 + i] = name[i];
	}
	dst[prefix.length() + 1 + name.length()] = '\0';
	return dst;
}

Maybe<CgVar> Cg::lookup_let(StringView name) const noexcept {
	for (Ulen l =	scopes.length(), i = l - 1; i < l; i--) {
		if (auto find = scopes[i].lookup_let(name)) {
			return find;
		}
	}
	return None{};
}

Maybe<CgVar> Cg::lookup_using(StringView name) const noexcept {
	for (Ulen l =	scopes.length(), i = l - 1; i < l; i--) {
		if (auto find = scopes[i].lookup_using(name)) {
			return find;
		}
	}
	return None{};
}

Maybe<CgVar> Cg::lookup_fn(StringView name) const noexcept {
	for (const auto& fn : fns) {
		if (fn.name() == name) {
			return fn;
		}
	}
	return None{};
}

Maybe<CgAddr> Cg::intrinsic(StringView name) const noexcept {
	for (const auto& intrinsic : intrinsics) {
		if (intrinsic.name() == name) {
			return intrinsic.addr();
		}
	}
	return None{};
}

Cg::~Cg() noexcept {
	if (scratch) {
		allocator.deallocate(scratch, sizeof *scratch);
	}
	if (module) {
		llvm.DisposeModule(module);
	}
	if (builder) {
		llvm.DisposeBuilder(builder);
	}
	if (context) {
		llvm.ContextDispose(context);
	}
}

} // namespace Biron