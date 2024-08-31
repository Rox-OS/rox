#ifndef BIRON_CG_H
#define BIRON_CG_H
#include <biron/cg_type.h>
#include <biron/cg_value.h>

#include <biron/diagnostic.h>

#include <biron/util/string.inl>

namespace Biron {

struct Allocator;
struct Diagnostic;

struct AstTopFn;
struct AstStmt;

// We keep track of the loop post and exit BBs for "continue" and "break"
struct Loop {
	LLVM::BasicBlockRef post;
	LLVM::BasicBlockRef exit;
};

struct CgScope {
	constexpr CgScope(Allocator& allocator) noexcept
		: vars{allocator}, defers{allocator}
	{
	}
	Bool emit_defers(Cg& cg) const noexcept;
	Array<CgVar> vars;
	Array<AstStmt*> defers;
	Maybe<Loop> loop;
};

struct CgMachine {
	constexpr CgMachine() noexcept = delete;
	~CgMachine() noexcept;

	CgMachine(CgMachine&& other) noexcept
		: m_llvm{other.m_llvm}
		, m_machine{exchange(other.m_machine, nullptr)}
	{
	}

	using TargetMachineRef = LLVM::TargetMachineRef;

	static Maybe<CgMachine> make(LLVM& llvm, StringView triple) noexcept;

	TargetMachineRef ref() const noexcept { return m_machine; }

private:
	constexpr CgMachine(LLVM& llvm, TargetMachineRef machine) noexcept
		: m_llvm{llvm}
		, m_machine{machine}
	{
	}
	LLVM&            m_llvm;
	TargetMachineRef m_machine;
};

struct Cg {
	using ContextRef = LLVM::ContextRef;
	using BuilderRef = LLVM::BuilderRef;
	using ModuleRef  = LLVM::ModuleRef;

	static Maybe<Cg> make(Allocator& allocator, LLVM& llvm, Diagnostic& diagnostic) noexcept;

	[[nodiscard]] Bool optimize(CgMachine& machine, Ulen level) noexcept;
	[[nodiscard]] Bool verify() noexcept;
	[[nodiscard]] Bool dump() noexcept;
	[[nodiscard]] Bool emit(CgMachine& machine, StringView name) noexcept;

	// Searches for the lexically closest loop
	const Loop* loop() const noexcept {
		for (Ulen l = scopes.length(), i = l - 1; i < l; i--) {
			if (const auto& scope = scopes[i]; scope.loop) {
				return &scope.loop.some();
			}
		}
		return nullptr;
	}

	template<typename... Ts>
	void error(Range range, const char* message, Ts&&... args) const noexcept {
		diagnostic.error(range, message, forward<Ts>(args)...);
	}

	template<typename... Ts>
	void fatal(Range range, const char* message, Ts&&... args) const noexcept {
		diagnostic.fatal(range, message, forward<Ts>(args)...);
	}

	None oom() const noexcept {
		fatal(Range{0, 0}, "Out of memory");
		return None{};
	}

	Maybe<CgAddr> emit_alloca(CgType* type) noexcept;

	Allocator&        allocator;
	LLVM&             llvm;
	ScratchAllocator* scratch;
	ContextRef        context;
	BuilderRef        builder;
	ModuleRef         module;
	CgTypeCache       types;
	Array<CgVar>      fns;
	Array<CgVar>      globals;
	Array<CgScope>    scopes;
	Array<CgTypeDef>  typedefs;
	Diagnostic&       diagnostic;

	constexpr Cg(Cg&& other) noexcept
		: allocator{other.allocator}
		, llvm{other.llvm}
		, scratch{exchange(other.scratch, nullptr)}
		, context{exchange(other.context, nullptr)}
		, builder{exchange(other.builder, nullptr)}
		, module{exchange(other.module, nullptr)}
		, types{move(other.types)}
		, fns{move(other.fns)}
		, globals{move(other.globals)}
		, scopes{move(other.scopes)}
		, typedefs{move(other.typedefs)}
		, diagnostic{other.diagnostic}
	{
	}

	~Cg() noexcept;

private:
	constexpr Cg(Allocator&        allocator,
	             LLVM&             llvm,
	             ScratchAllocator* scratch,
	             ContextRef        context,
	             BuilderRef        builder,
	             ModuleRef         module,
	             CgTypeCache&&     types,
	             Diagnostic&       diagnostic) noexcept
		: allocator{allocator}
		, llvm{llvm}
		, scratch{scratch}
		, context{context}
		, builder{builder}
		, module{module}
		, types{move(types)}
		, fns{allocator}
		, globals{allocator}
		, scopes{allocator}
		, typedefs{allocator}
		, diagnostic{diagnostic}
	{
	}
};

} // namespace Biron

#endif // BIRON_CG_H