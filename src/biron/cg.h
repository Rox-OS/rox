#ifndef BIRON_CG_H
#define BIRON_CG_H
#include <biron/cg_type.h>
#include <biron/cg_value.h>

#include <biron/diagnostic.h>

#include <biron/util/string.h>
#include <biron/util/error.inl>

namespace Biron {

struct Allocator;
struct Terminal;

struct Diagnostic;

struct Ast;
struct AstStmt;
struct AstFn;

// We keep track of the loop post and exit BBs for "continue" and "break"
struct Loop {
	LLVM::BasicBlockRef post;
	LLVM::BasicBlockRef exit;
};

struct CgScope {
	constexpr CgScope(Allocator& allocator) noexcept
		: vars{allocator}, tests{allocator}, defers{allocator}, usings{allocator}
	{
	}

	Bool emit_defers(Cg& cg) const noexcept;

	Maybe<CgVar> lookup_let(StringView name) const noexcept {
		// Search the flow-sensitive type aliases list first.
		for (Ulen l = tests.length(), i = l - 1; i < l; i--) {
			const auto& test = tests[i];
			if (test.name() == name) {
				return test;
			}
		}
		for (Ulen l = vars.length(), i = l - 1; i < l; i--) {
			const auto& var = vars[i];
			if (var.name() == name) {
				return var;
			}
		}
		return None{};
	}

	Maybe<CgVar> lookup_using(StringView name) const noexcept {
		for (Ulen l = usings.length(), i = l - 1; i < l; i--) {
			const auto& u = usings[i];
			if (u.name() == name) {
				return u;
			}
		}
		return None{};
	}

	Array<CgVar>    vars;
	Array<CgVar>    tests;
	Array<AstStmt*> defers;
	Array<CgVar>    usings;
	Maybe<Loop>     loop;
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

	static Maybe<CgMachine> make(Terminal& terminal,
	                             LLVM& llvm,
	                             StringView triple) noexcept;

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

	static Maybe<Cg> make(Terminal& terminal,
	                      Allocator& allocator,
	                      LLVM& llvm,
	                      Diagnostic& diagnostic) noexcept;

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
	Error error(Range range, StringView fmt, Ts&&... args) const noexcept {
		m_diagnostic.error(range, fmt, forward<Ts>(args)...);
		return {};
	}

	template<typename... Ts>
	Error fatal(Range range, StringView fmt, Ts&&... args) const noexcept {
		m_diagnostic.fatal(range, fmt, forward<Ts>(args)...);
		return {};
	}

	Error oom() const noexcept {
		return fatal(Range{0, 0}, "Out of memory while generating code");
	}

	Maybe<CgAddr> intrinsic(StringView name) const noexcept;

	CgAddr emit_alloca(CgType* type) noexcept;
	Maybe<CgValue> emit_lt(const CgValue& lhs, const CgValue& rhs, Range range) noexcept;
	Maybe<CgValue> emit_le(const CgValue& lhs, const CgValue& rhs, Range range) noexcept;
	Maybe<CgValue> emit_gt(const CgValue& lhs, const CgValue& rhs, Range range) noexcept;
	Maybe<CgValue> emit_ge(const CgValue& lhs, const CgValue& rhs, Range range) noexcept;
	Maybe<CgValue> emit_add(const CgValue& lhs, const CgValue& rhs, Range range) noexcept;
	Maybe<CgValue> emit_sub(const CgValue& lhs, const CgValue& rhs, Range range) noexcept;
	Maybe<CgValue> emit_mul(const CgValue& lhs, const CgValue& rhs, Range range) noexcept;
	Maybe<CgValue> emit_div(const CgValue& lhs, const CgValue& rhs, Range range) noexcept;
	Maybe<CgValue> emit_min(const CgValue& lhs, const CgValue& rhs, Range range) noexcept;
	Maybe<CgValue> emit_max(const CgValue& lhs, const CgValue& rhs, Range range) noexcept;
	Maybe<CgValue> emit_for_array(const CgValue& lhs,
	                              const CgValue& rhs,
	                              Range range,
	                              Maybe<CgValue> (Cg::*emit)(const CgValue&,
	                                                         const CgValue&,
	                                                         Range));

	const char* nameof(StringView name) const noexcept;

	Maybe<CgVar> lookup_let(StringView name) const noexcept;
	Maybe<CgVar> lookup_using(StringView name) const noexcept;
	Maybe<CgVar> lookup_fn(StringView name) const noexcept;

	Allocator&          allocator;
	LLVM&               llvm;
	ScratchAllocator*   scratch;
	ContextRef          context;
	BuilderRef          builder;
	ModuleRef           module;
	CgTypeCache         types;
	Array<CgVar>        fns;
	Array<CgGlobal>     globals;
	Array<CgScope>      scopes;
	Array<CgTypeDef>    typedefs;
	Array<CgTypeDef>    effects;
	Array<CgVar>        intrinsics;
	const Ast*          ast; // Current unit
	const AstFn*        fn;  // Current function
	LLVM::BasicBlockRef entry;
	StringView          prefix;

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
		, effects{move(other.effects)}
		, intrinsics{move(other.intrinsics)}
		, ast{exchange(other.ast, nullptr)}
		, fn{exchange(other.fn, nullptr)}
		, entry{exchange(other.entry, nullptr)}
		, prefix{move(other.prefix)}
		, m_terminal{other.m_terminal}
		, m_diagnostic{other.m_diagnostic}
	{
	}

	~Cg() noexcept;

private:
	constexpr Cg(Terminal&         terminal,
	             Allocator&        allocator,
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
		, effects{allocator}
		, intrinsics{allocator}
		, ast{nullptr}
		, fn{nullptr}
		, entry{nullptr}
		, prefix{}
		, m_terminal{terminal}
		, m_diagnostic{diagnostic}
	{
	}

	Terminal&   m_terminal;
	Diagnostic& m_diagnostic;
};

} // namespace Biron

#endif // BIRON_CG_H