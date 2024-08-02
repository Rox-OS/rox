#ifndef BIRON_CODEGEN_H
#define BIRON_CODEGEN_H
#include <biron/llvm.h>

#include <biron/util/array.inl>
#include <biron/util/string.inl>

#include <biron/pool.h>

namespace Biron {

struct AstFn;
struct AstAsm;
struct AstLetStmt;

struct Type {
	enum Flag {
		UNSIGNED = 1 << 0,
		SIGNED   = 1 << 1,
		TUPLE    = 1 << 2,
	};
	constexpr Type(LLVM::TypeRef type, Uint32 flags, Type* base = nullptr) noexcept
		: type{type}
		, base{base}
		, flags{flags}
		, extent{0}
	{
	}
	Type unqual() const noexcept { return base ? *base : *this; }
	LLVM::TypeRef type;
	Type*         base;
	Uint32        flags;
	Uint64        extent;
};

struct Value {
	constexpr Value(Type type, LLVM::ValueRef value) noexcept
		: type{move(type)}
		, value{value}
	{
	}
	Type           type;  // The type of the value
	LLVM::ValueRef value;
};

struct Unit {
	constexpr Unit(Allocator& allocator) noexcept
		: fns{allocator}
		, asms{allocator}
		, lets{allocator}
		, caches{allocator}
	{
	}
	Array<AstFn*> fns;
	Array<AstAsm*> asms;
	Array<AstLetStmt*> lets;
	Array<Cache> caches;
	Bool dump(StringBuilder& builder) const noexcept;
};

struct Var {
	constexpr Var(StringView name, Type type, LLVM::ValueRef value)
		: name{name}
		, value{type, value}
	{
	}
	StringView name;
	Value      value;
};

struct Fn {
	constexpr Fn(StringView name, Type type, LLVM::ValueRef value)
		: name{name}
		, value{type, value}
	{
	}
	StringView name;
	Value      value;
};

struct Codegen {
	Codegen(LLVM& llvm, Allocator& allocator, const char* triple) noexcept;
	~Codegen();
	void dump() noexcept;
	Bool run(Unit& unit) noexcept;
	Bool optimize() noexcept;
	Bool emit(StringView name) noexcept;

	template<typename... Ts>
	[[nodiscard]] Type* new_type(Ts&&... args) noexcept {
		Ulen offset = types.length();
		if (!types.reserve(offset + 1)) {
			return nullptr;
		}
		if (auto addr = allocator.allocate(sizeof(Type))) {
			auto type = new(addr, Nat{}) Type{forward<Ts>(args)...};
			(void)types.push_back(type); // Cannot fail
			return type;
		}
		return nullptr;
	}

	LLVM&                  llvm;
	LLVM::ContextRef       context;
	LLVM::BuilderRef       builder;
	LLVM::ModuleRef        module;
	Type                   t_i1;
	Type                   t_s8, t_s16, t_s32, t_s64;
	Type                   t_u8, t_u16, t_u32, t_u64;
	Type                   t_ptr;
	Type                   t_unit;
	Type                   t_slice;
	LLVM::TargetMachineRef machine;

	// Unit                   *unit;
	Array<Var>             vars;
	Array<Fn>              fns;
	Array<Type*>           types;

	Allocator& allocator;
};

} // namespace Biron

#endif // BIRON_CODEGEN_H