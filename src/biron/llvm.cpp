#include <biron/llvm.h>
#include <biron/util/system.inl>
#include <biron/util/terminal.inl>

namespace Biron {

template<typename T>
static Bool link(const System& system, Terminal& terminal, void *lib, T*& p, StringView name) noexcept {
	auto sym = system.lib_symbol(system, lib, name);
	if (!sym) {
		terminal.err("Could not find symbol: %S\n", name);
		return false;
	}
	*reinterpret_cast<void **>(&p) = sym;
	return true;
}

LLVM::LLVM(LLVM&& llvm) noexcept
	: m_system{llvm.m_system}
	, m_lib{exchange(llvm.m_lib, nullptr)}
{
	#define FN(_, NAME, ...) \
		NAME = exchange(llvm.NAME, nullptr);
	#include "llvm.inl"
	#undef FN
}

LLVM::~LLVM() noexcept {
	if (Shutdown) {
		Shutdown();
	}
	if (m_lib) {
		m_system.lib_close(m_system, m_lib);
	}
}

Maybe<LLVM> LLVM::load(const System& system) noexcept {
	LLVM llvm{system};

	// We only support LLVM-19, LLVM-18, and LLVM-17
	if (!(llvm.m_lib = system.lib_open(system, "libLLVM-19"))) {
		if (!(llvm.m_lib = system.lib_open(system, "libLLVM-18"))) {
			if (!(llvm.m_lib = system.lib_open(system, "libLLVM-17"))) {
				return None{};
			}
		}
	}

	Terminal terminal{system};
	#define FN(RETURN, NAME, ...) \
		if (!link(system, terminal, llvm.m_lib, llvm.NAME, "LLVM" #NAME)) { \
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
