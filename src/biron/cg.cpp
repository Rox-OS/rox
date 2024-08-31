#include <stdio.h>

#include <biron/cg.h>
#include <biron/cg_value.h>

namespace Biron {

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

Maybe<Cg> Cg::make(Allocator& allocator, LLVM& llvm, StringView target_triple, Diagnostic& diagnostic) noexcept {
	// The target triple cannot be that long so use an inline allocator here for it.
	InlineAllocator<1024> temporary;
	auto triple = target_triple.terminated(temporary);
	if (!triple) {
		return None{};
	}

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
		machine,
		move(*types),
		diagnostic
	};
}

Bool Cg::optimize(Ulen level) noexcept {
	if (!verify()) {
		return false;
	}
	auto options = llvm.CreatePassBuilderOptions();
	LLVM::ErrorRef result = nullptr;
	switch (level) {
	case 0:
		result = llvm.RunPasses(module, "default<O0>", machine, options);
		break;
	case 1:
		result = llvm.RunPasses(module, "default<O1>", machine, options);
		break;
	case 2:
		result = llvm.RunPasses(module, "default<O2>", machine, options);
		break;
	case 3:
		result = llvm.RunPasses(module, "default<O3>", machine, options);
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
	                      LLVM::VerifierFailureAction::PrintMessage,
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

Bool Cg::emit(StringView name) noexcept {
	char* error = nullptr;
	if (!verify()) {
		return false;
	}
	InlineAllocator<FILENAME_MAX + 1> scratch;
	auto terminated = name.terminated(scratch);
	if (!terminated) {
		fprintf(stderr, "Out of memory\n");
		return false;
	}
	if (llvm.TargetMachineEmitToFile(machine,
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
	if (machine) {
		llvm.DisposeTargetMachine(machine);
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