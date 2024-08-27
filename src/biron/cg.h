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

struct Cg {
	using ContextRef       = LLVM::ContextRef;
	using BuilderRef       = LLVM::BuilderRef;
	using ModuleRef        = LLVM::ModuleRef;
	using TargetMachineRef = LLVM::TargetMachineRef;

	static Maybe<Cg> make(Allocator& allocator, LLVM& llvm, StringView triple, Diagnostic& diagnostic) noexcept;

	Bool optimize() noexcept;
	Bool verify() noexcept;
	void dump() noexcept;
	Bool emit(StringView name) noexcept;

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
	void error(Range range, const char* message, Ts&&... args) {
		diagnostic.error(range, message, forward<Ts>(args)...);
	}

	Maybe<CgAddr> emit_alloca(CgType* type) noexcept;

	Allocator&             allocator;
	LLVM&                  llvm;
	ContextRef             context;
	BuilderRef             builder;
	ModuleRef              module;
	TargetMachineRef       machine;
	CgTypeCache            types;
	Array<CgVar>           fns;
	Array<CgScope>         scopes;
	Array<CgTypeDef>       typedefs;
	Diagnostic&            diagnostic;

	constexpr Cg(Cg&& other) noexcept
		: allocator{other.allocator}
		, llvm{other.llvm}
		, context{exchange(other.context, nullptr)}
		, builder{exchange(other.builder, nullptr)}
		, module{exchange(other.module, nullptr)}
		, machine{exchange(other.machine, nullptr)}
		, types{move(other.types)}
		, fns{move(other.fns)}
		, scopes{move(other.scopes)}
		, typedefs{move(other.typedefs)}
		, diagnostic{other.diagnostic}
	{
	}

	~Cg() noexcept;

private:
	friend struct AstForStmt;
	friend struct AstTopFn;
	friend struct AstVarExpr;
	friend struct AstBreakStmt;
	friend struct AstContinueStmt;
	friend struct AstDeferStmt;
	friend struct AstReturnStmt;

	constexpr Cg(Allocator&       allocator,
	             LLVM&            llvm,
	             ContextRef       context,
	             BuilderRef       builder,
	             ModuleRef        module,
	             TargetMachineRef machine,
	             CgTypeCache&&    types,
	             Diagnostic&      diagnostic) noexcept
		: allocator{allocator}
		, llvm{llvm}
		, context{context}
		, builder{builder}
		, module{module}
		, machine{machine}
		, types{move(types)}
		, fns{allocator}
		, scopes{allocator}
		, typedefs{allocator}
		, diagnostic{diagnostic}
	{
	}
};

} // namespace Biron

#endif // BIRON_CG_H