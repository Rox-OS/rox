#include <dlfcn.h>
#include <stdio.h>
#include "llvm.h"

namespace Biron {

template<typename T>
static bool link(void *lib, T*& p, const char *name) {
	void *sym = dlsym(lib, name);
	if (!sym) {
		fprintf(stderr, "Could not find symbol: %s\n", name);
		return false;
	}
	*reinterpret_cast<void **>(&p) = sym;
	return true;
}

#define LINK(name) \
	do { \
		if (!link(lib, llvm.name, "LLVM" #name)) { \
			return None{}; \
		} \
	} while (0)

Maybe<LLVM> LLVM::load() noexcept {
	// We only support LLVM-18 and LLVM-17
	void *lib = dlopen("libLLVM-18.so", RTLD_NOW);
	if (!lib) {
		// Try libLLVM-17.so as well
		lib = dlopen("libLLVM-17.so", RTLD_NOW);
		if (!lib) {
			return None{};
		}
	}

	LLVM llvm;
	LINK(InitializeX86TargetInfo);
	LINK(InitializeX86Target);
	LINK(InitializeX86TargetMC);
	LINK(InitializeX86AsmPrinter);
	LINK(InitializeX86AsmParser);
	LINK(FunctionType);
	LINK(DisposeMessage);
	LINK(ContextCreate);
	LINK(ContextDispose);
	LINK(Int1TypeInContext);
	LINK(Int8TypeInContext);
	LINK(Int16TypeInContext);
	LINK(Int32TypeInContext);
	LINK(Int64TypeInContext);
	LINK(VoidTypeInContext);
	LINK(PointerTypeInContext);
	LINK(StructTypeInContext);
	LINK(ArrayType2);
	LINK(CreateBasicBlockInContext);
	LINK(ConstStructInContext);
	LINK(GetBasicBlockParent);
	LINK(GetBasicBlockTerminator);
	LINK(ConstInt);
	LINK(AppendExistingBasicBlock);
	LINK(GetInlineAsm);
	LINK(ModuleCreateWithNameInContext);
	LINK(AddFunction);
	LINK(GetNamedFunction);
	LINK(DisposeModule);
	LINK(DumpModule);
	LINK(VerifyModule);
	LINK(CreateBuilderInContext);
	LINK(DisposeBuilder);
	LINK(PositionBuilderAtEnd);
	LINK(GetInsertBlock);
	LINK(BuildRet);
	LINK(BuildRetVoid);
	LINK(BuildBr);
	LINK(BuildAdd);
	LINK(BuildSub);
	LINK(BuildMul);
	LINK(BuildAnd);
	LINK(BuildShl);
	LINK(BuildLShr);
	LINK(BuildAShr);
	LINK(BuildOr);
	LINK(BuildXor);
	LINK(BuildNeg);
	LINK(BuildNot);
	LINK(BuildCall2);
	LINK(BuildSelect);
	LINK(BuildCondBr);
	LINK(BuildLoad2);
	LINK(BuildStore);
	LINK(BuildGEP2);
	LINK(BuildGlobalString);
	LINK(BuildICmp);
	LINK(BuildAlloca);
	LINK(BuildCast);
	LINK(GetCastOpcode);
	LINK(GetParam);
	LINK(StructGetTypeAtIndex);
	LINK(GetTargetFromTriple);
	LINK(CreateTargetMachine);
	LINK(DisposeTargetMachine);
	LINK(TargetMachineEmitToFile);
	LINK(CreatePassBuilderOptions);
	LINK(DisposePassBuilderOptions);
	LINK(RunPasses);

	llvm.InitializeX86TargetInfo();
	llvm.InitializeX86Target();
	llvm.InitializeX86TargetMC();
	llvm.InitializeX86AsmPrinter();
	llvm.InitializeX86AsmParser();

	return llvm;
}

} // namespace Biron