#ifndef BIRON_CG_H
#define BIRON_CG_H
#include <biron/cg_type.h>
#include <biron/cg_value.h>

#include <biron/util/string.inl>

namespace Biron {

struct Allocator;
struct AstTopFn;

struct CgScope {
	Array<CgVar> vars;
};

struct Cg {
	using ContextRef       = LLVM::ContextRef;
	using BuilderRef       = LLVM::BuilderRef;
	using ModuleRef        = LLVM::ModuleRef;
	using TargetMachineRef = LLVM::TargetMachineRef;

	static Maybe<Cg> make(Allocator& allocator, LLVM& llvm, StringView triple) noexcept;

	Bool optimize() noexcept;
	Bool verify() noexcept;
	void dump() noexcept;
	Bool emit(StringView name) noexcept;

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
		, loops{move(other.loops)}
	{
	}

	~Cg() noexcept;

private:
	friend struct AstForStmt;
	friend struct AstTopFn;
	friend struct AstVarExpr;
	friend struct AstBreakStmt;
	friend struct AstContinueStmt;

	constexpr Cg(Allocator&       allocator,
	             LLVM&            llvm,
	             ContextRef       context,
	             BuilderRef       builder,
	             ModuleRef        module,
	             TargetMachineRef machine,
	             CgTypeCache&&    types) noexcept
		: allocator{allocator}
		, llvm{llvm}
		, context{context}
		, builder{builder}
		, module{module}
		, machine{machine}
		, types{move(types)}
		, fns{allocator}
		, scopes{allocator}
		, loops{allocator}
	{
	}

	// We keep track of the loop post and exit BBs for "continue" and "break"
	struct Loop {
		LLVM::BasicBlockRef post;
		LLVM::BasicBlockRef exit;
	};

	Array<Loop> loops;
};

} // namespace Biron

#endif // BIRON_CG_H