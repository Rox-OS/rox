#include <stdio.h> // fprintf, stderr

#include <biron/cg.h>
#include <biron/cg_value.h>

namespace Biron {

// [CgMachine]
static LLVM::TargetRef target_from_triple(LLVM& llvm, const char* triple) noexcept {
	LLVM::TargetRef target;
	char* error = nullptr;
	if (llvm.GetTargetFromTriple(triple, &target, &error) != 0) {
		fprintf(stderr, "Could not find target: %s\n", error);
		llvm.DisposeMessage(error);
		return nullptr;
	}
	return target;
}

Maybe<CgMachine> CgMachine::make(LLVM& llvm, StringView target_triple) noexcept {
	InlineAllocator<1024> scratch;
	auto triple = target_triple.terminated(scratch);
	auto target = target_from_triple(llvm, triple);
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
Maybe<Cg> Cg::make(Allocator& allocator, LLVM& llvm, Diagnostic& diagnostic) noexcept {
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
		fprintf(stderr, "Could not verify module: %s\n", error);
		llvm.DisposeMessage(error);
		return false;
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
		fprintf(stderr, "Out of memory\n");
		return false;
	}
	if (llvm.TargetMachineEmitToFile(machine.ref(),
	                                 module,
	                                 terminated,
	                                 LLVM::CodeGenFileType::Object,
	                                 &error) != 0)
	{
		fprintf(stderr, "Could not compile module %s: %s\n", terminated, error);
		llvm.DisposeMessage(error);
		return false;
	}
	llvm.DisposeMessage(error);
	return true;
}

Maybe<CgAddr> Cg::emit_alloca(CgType* type) noexcept {
	if (auto value = llvm.BuildAlloca(builder, type->ref(), "")) {
		// We may have a higher alignment requirement than what Alloca will pick.
		llvm.SetAlignment(value, type->align());
		return CgAddr { type->addrof(*this), value };
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