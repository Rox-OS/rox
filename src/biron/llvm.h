#ifndef BIRON_LLVM_H
#define BIRON_LLVM_H
#include <biron/util/maybe.inl>
#include <biron/util/types.inl>

namespace Biron {

struct LLVM {
	LLVM() noexcept;
	LLVM(LLVM&&) noexcept = default;
	LLVM(const LLVM&) noexcept = delete;
	~LLVM() noexcept;

	struct OpaqueContext;
	struct OpaqueModule;
	struct OpaqueType;
	struct OpaqueValue;
	struct OpaqueBasicBlock;
	struct OpaqueBuilder;
	struct OpaqueTargetMachineOptions;
	struct OpaqueTargetMachine;
	struct OpaqueTarget;
	struct OpaquePassBuilderOptions;
	struct OpaqueError;

	using ContextRef              = OpaqueContext*;
	using ModuleRef               = OpaqueModule*;
	using TypeRef                 = OpaqueType*;
	using ValueRef                = OpaqueValue*;
	using BasicBlockRef           = OpaqueBasicBlock*;
	using BuilderRef              = OpaqueBuilder*;
	using TargetMachineOptionsRef = OpaqueTargetMachineOptions*;
	using TargetMachineRef        = OpaqueTargetMachine*;
	using TargetRef               = OpaqueTarget*;
	using PassBuilderOptionsRef   = OpaquePassBuilderOptions*;
	using ErrorRef                = OpaqueError*;
	using Bool                    = int;
	using Ulen                    = decltype(sizeof 0);
	using Opcode = int;

	enum class CodeGenOptLevel       : int { None, Less, Default, Aggressive };
	enum class RelocMode             : int { Default, Static, PIC, DynamicNoPic, ROPI, RWPI, ROPI_RWPI };
	enum class CodeModel             : int { Default, JITDefault, Tiny, Small, Kernel, Medium, Large };
	enum class CodeGenFileType       : int { Assembly, Object };
	enum class VerifierFailureAction : int { AbortProcess, PrintMessage, ReturnStatus };
	enum class IntPredicate          : int { EQ = 32, NE, UGT, UGE, ULT, ULE, SGT, SGE, SLT, SLE };

	#define FN(RETURN, NAME, ...) \
		RETURN (*NAME)(__VA_ARGS__);
	#include <biron/llvm.inl>
	#undef FN

	static Maybe<LLVM> load() noexcept;

private:
	void* m_lib;
};

} // namespace Biron

#endif // BIRON_LLVM_H