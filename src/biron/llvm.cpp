#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

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
		if (!link(llvm.lib, llvm.name, "LLVM" #name)) { \
			fprintf(stderr, "Could not load 'LLVM" #name "'\n"); \
			return None{}; \
		} \
	} while (0)


LLVM::LLVM() {
	// This structure only stores function pointers and a local library handle. We
	// start off with everything zeroed so any load errors along the way will at
	// least leave things null so that ~LLVM() can safely call Shutdown if opened
	// and dlclose if loaded.
	memset(this, 0, sizeof *this);
}

LLVM::~LLVM() {
	if (Shutdown) {
		Shutdown();
	}
	if (lib) {
		dlclose(lib);
	}
}

Maybe<LLVM> LLVM::load() noexcept {
	LLVM llvm;

	// We only support LLVM-18 and LLVM-17
	if (!(llvm.lib = dlopen("libLLVM-18.so", RTLD_NOW))) {
		// Try libLLVM-17.so as well
		if (!(llvm.lib = dlopen("libLLVM-17.so", RTLD_NOW))) {
			return None{};
		}
	}

	LINK(InitializeX86TargetInfo);
	LINK(InitializeX86Target);
	LINK(InitializeX86TargetMC);
	LINK(InitializeX86AsmPrinter);
	LINK(InitializeX86AsmParser);
	LINK(DisposeMessage);
	LINK(Shutdown);
	LINK(ContextCreate);
	LINK(ContextDispose);
	LINK(GetTypeByName2);
	LINK(Int1TypeInContext);
	LINK(Int8TypeInContext);
	LINK(Int16TypeInContext);
	LINK(Int32TypeInContext);
	LINK(Int64TypeInContext);
	LINK(VoidTypeInContext);
	LINK(FloatTypeInContext);
	LINK(DoubleTypeInContext);
	LINK(PointerTypeInContext);
	LINK(StructTypeInContext);
	LINK(StructCreateNamed);
	LINK(StructSetBody);
	LINK(ArrayType2);
	LINK(FunctionType);
	LINK(CreateBasicBlockInContext);
	LINK(GetBasicBlockParent);
	LINK(GetBasicBlockTerminator);
	LINK(ConstInt);
	LINK(ConstReal);
	LINK(ConstArray2);
	LINK(ConstPointerNull);
	LINK(ConstStructInContext);
	LINK(ConstNamedStruct);
	LINK(GetAggregateElement);
	LINK(StructGetTypeAtIndex);
	LINK(CountStructElementTypes);
	LINK(AppendExistingBasicBlock);
	LINK(AddGlobal);
	LINK(SetGlobalConstant);
	LINK(SetInitializer);
	LINK(SetSection);
	LINK(SetAlignment);
	LINK(SetValueName2);
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
	LINK(BuildFAdd);
	LINK(BuildSub);
	LINK(BuildFSub);
	LINK(BuildMul);
	LINK(BuildFMul);
	LINK(BuildFDiv);
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
	LINK(DumpType);
	LINK(BuildStore);
	LINK(BuildGEP2);
	LINK(BuildGlobalString);
	LINK(BuildICmp);
	LINK(BuildAlloca);
	LINK(BuildCast);
	LINK(BuildExtractValue);
	LINK(GetCastOpcode);
	LINK(BuildPhi);
	LINK(AddIncoming);
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