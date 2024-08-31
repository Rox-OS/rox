#include <dlfcn.h> // dlopen, dlclose, dlsym
#include <stdio.h> // fprintf, stderr
#include <string.h> // memset

#include <biron/llvm.h>

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

LLVM::LLVM() noexcept {
	// This structure only stores function pointers and a local library handle. We
	// start off with everything zeroed so any load errors along the way will at
	// least leave things null so that ~LLVM() can safely call Shutdown if opened
	// and dlclose if loaded.
	memset(this, 0, sizeof *this);
}

LLVM::~LLVM() noexcept {
	if (Shutdown) {
		Shutdown();
	}
	if (m_lib) {
		dlclose(m_lib);
	}
}

Maybe<LLVM> LLVM::load() noexcept {
	LLVM llvm;

	// We only support LLVM-18 and LLVM-17
	if (!(llvm.m_lib = dlopen("libLLVM-18.so", RTLD_NOW))) {
		// Try libLLVM-17.so as well
		if (!(llvm.m_lib = dlopen("libLLVM-17.so", RTLD_NOW))) {
			return None{};
		}
	}

	#define FN(RETURN, NAME, ...) \
		if (!link(llvm.m_lib, llvm.NAME, "LLVM" #NAME)) { \
			return None{}; \
		}
	#include "llvm.inl"
	#undef FN

	llvm.InitializeX86TargetInfo();
	llvm.InitializeX86Target();
	llvm.InitializeX86TargetMC();
	llvm.InitializeX86AsmPrinter();
	llvm.InitializeX86AsmParser();

	return llvm;
}

} // namespace Biron
