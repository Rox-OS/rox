#ifndef BIRON_CG_TYPE_H
#define BIRON_CG_TYPE_H
#include <stdio.h>

#include <biron/pool.h>
#include <biron/llvm.h>

namespace Biron {

struct Cg;
struct CgTypeCache;

struct CgType {
	enum class Kind {
		U8, U16, U32, U64, // Uint{8,16,32,64}
		S8, S16, S32, S64, // Sint{8,16,32,64}
		B8, B16, B32, B64, // Bool{8,16,32,64}
		STRING,            // String
		POINTER,           // *T
		SLICE,             // []T
		ARRAY,             // [N]T
		PADDING,           // [N]u8 // Special meta-type for tuple and struct padding
		TUPLE,             // (T1, ..., Tn)
		STRUCT,            // struct{_: T1; ...; _: Tn}
		UNION,             // union{_: T1; ...; _: Tn}
		FN,                // fn (T1, ..., Tn) -> (R1, ..., Rn)
		VA,                // ...
	};

	void dump(Bool nl = true) const noexcept {
		switch (m_kind) {
		/****/ case Kind::U8:      fprintf(stderr, "Uint8");
		break; case Kind::U16:     fprintf(stderr, "Uint16");
		break; case Kind::U32:     fprintf(stderr, "Uint32");
		break; case Kind::U64:     fprintf(stderr, "Uint64");
		break; case Kind::S8:      fprintf(stderr, "Sint8");
		break; case Kind::S16:     fprintf(stderr, "Sint16");
		break; case Kind::S32:     fprintf(stderr, "Sint32");
		break; case Kind::S64:     fprintf(stderr, "Sint64");
		break; case Kind::B8:      fprintf(stderr, "Bool8");
		break; case Kind::B16:     fprintf(stderr, "Bool16");
		break; case Kind::B32:     fprintf(stderr, "Bool32");
		break; case Kind::B64:     fprintf(stderr, "Bool64");
		break; case Kind::POINTER: fprintf(stderr, "*"); if (auto base = at(0)) base->dump(false);
		break; case Kind::STRING:  fprintf(stderr, "String");
		break; case Kind::SLICE:   fprintf(stderr, "[]"); at(0)->dump(false);
		break; case Kind::ARRAY:   fprintf(stderr, "[%zu]", m_extent); at(0)->dump(false);
		break; case Kind::PADDING: fprintf(stderr, ".Pad%zu", m_size);
		break; case Kind::STRUCT:  fprintf(stderr, "struct{...}");
		break; case Kind::UNION:   fprintf(stderr, "union{...}");
		break; case Kind::TUPLE:
			{
				fprintf(stderr, "(");
				for (Ulen l = length(), i = 0; i < l; i++) {
					at(i)->dump(false);
					if (i != l - 1) {
						fprintf(stderr, ", ");
					}
				}
				fprintf(stderr, ")");
			}
		break; case Kind::FN:
			{
				const auto& args = at(0)->types();
				const auto& rets = at(1)->types();
				fprintf(stderr, "fn(");
				Bool f = true;
				for (const auto& arg : args) {
					if (!f) fprintf(stderr, ", ");
					arg->dump(false);
					f = false;
				}
				fprintf(stderr, ") -> (");
				f = true;
				for (const auto& ret : rets) {
					if (!f) fprintf(stderr, ", ");
					ret->dump(false);
					f = false;
				}
				fprintf(stderr, ")");
			}
		break; case Kind::VA: fprintf(stderr, "...");
		break;
		}
		if (nl) fprintf(stderr, "\n");
	}

	CgType* addrof(Cg& cg) noexcept;

	[[nodiscard]] CgType* deref() const noexcept { return at(0); }
	[[nodiscard]] CgType* at(Ulen i) const noexcept { return types()[i]; }

	[[nodiscard]] constexpr const Array<CgType*>& types() const noexcept {
		BIRON_ASSERT(m_types && "No nested types");
		return (*m_types);
	}

	[[nodiscard]] constexpr Ulen size() const noexcept { return m_size; }
	[[nodiscard]] constexpr Ulen align() const noexcept { return m_align; }
	[[nodiscard]] constexpr Ulen length() const noexcept { return m_types ? m_types->length() : 0; };
	[[nodiscard]] constexpr Ulen extent() const noexcept { return m_extent; };

	[[nodiscard]] constexpr Kind kind() const noexcept { return m_kind; }

	[[nodiscard]] constexpr Bool is_bool() const noexcept { return m_kind >= Kind::B8 && m_kind <= Kind::B64; }
	[[nodiscard]] constexpr Bool is_sint() const noexcept { return m_kind >= Kind::S8 && m_kind <= Kind::S64; }
	[[nodiscard]] constexpr Bool is_uint() const noexcept { return m_kind >= Kind::U8 && m_kind <= Kind::U64; }

	[[nodiscard]] constexpr Bool is_pointer() const noexcept { return m_kind == Kind::POINTER; }
	[[nodiscard]] constexpr Bool is_string() const noexcept { return m_kind == Kind::STRING; }
	[[nodiscard]] constexpr Bool is_slice() const noexcept { return m_kind == Kind::SLICE; }
	[[nodiscard]] constexpr Bool is_array() const noexcept { return m_kind == Kind::ARRAY; }
	[[nodiscard]] constexpr Bool is_padding() const noexcept { return m_kind == Kind::PADDING; }
	[[nodiscard]] constexpr Bool is_tuple() const noexcept { return m_kind == Kind::TUPLE; }
	[[nodiscard]] constexpr Bool is_struct() const noexcept { return m_kind == Kind::STRUCT; }
	[[nodiscard]] constexpr Bool is_union() const noexcept { return m_kind == Kind::UNION; }
	[[nodiscard]] constexpr Bool is_fn() const noexcept { return m_kind == Kind::FN; }
	[[nodiscard]] constexpr Bool is_va() const noexcept { return m_kind == Kind::VA; }

	LLVM::TypeRef ref(Cg& cg) noexcept;

	Bool operator==(const CgType& other) const noexcept {
		if (other.m_kind   != m_kind)   return false;
		if (other.m_size   != m_size)   return false;
		if (other.m_align  != m_align)  return false;
		if (other.m_extent != m_extent) return false;
		if (other.m_types  != m_types)  return false;
		// We do not compare m_ref
		return true;
	}

	// Custom make tags for the type cache
	struct IntInfo {
		Ulen size;
		Ulen align;
		Bool sign;
	};

	struct PtrInfo {
		CgType* base;
		Ulen    size;
		Ulen    align;
	};

	struct BoolInfo {
		Ulen size;
		Ulen align;
	};

	struct StringInfo {
		CgType* ptr;
		CgType* len;
	};

	struct UnionInfo {
		Array<CgType*> types;
	};

	struct RecordInfo {
		Bool tuple;
		Array<CgType*> types;
	};

	struct ArrayInfo {
		CgType* base;
		Uint64  extent;
	};

	struct SliceInfo {
		CgType* base;
	};

	struct PaddingInfo {
		Uint64 padding;
	};

	struct FnInfo {
		CgType* args;
		CgType* rets;
	};

	struct VaInfo { };

private:
	friend struct CgTypeCache;

	static Maybe<CgType> make(Cache& cache, IntInfo info) noexcept;
	static Maybe<CgType> make(Cache& cache, PtrInfo info) noexcept;
	static Maybe<CgType> make(Cache& cache, BoolInfo info) noexcept;
	static Maybe<CgType> make(Cache& cache, StringInfo info) noexcept;
	static Maybe<CgType> make(Cache& cache, UnionInfo info) noexcept;
	static Maybe<CgType> make(Cache& cache, RecordInfo info) noexcept;
	static Maybe<CgType> make(Cache& cache, ArrayInfo info) noexcept;
	static Maybe<CgType> make(Cache& cache, SliceInfo info) noexcept;
	static Maybe<CgType> make(Cache& cache, PaddingInfo info) noexcept;
	static Maybe<CgType> make(Cache& cache, FnInfo info) noexcept;
	static Maybe<CgType> make(Cache& cache, VaInfo info) noexcept;

	constexpr CgType(Kind kind, Ulen size, Ulen align, Ulen extent, Maybe<Array<CgType*>>&& types) noexcept
		: m_kind{kind}
		, m_size{size}
		, m_align{align}
		, m_extent{extent}
		, m_types{move(types)}
		, m_ref{nullptr}
	{
	}

	Kind m_kind;
	Ulen m_size;
	Ulen m_align;
	Ulen m_extent;
	Maybe<Array<CgType*>> m_types;
	LLVM::TypeRef m_ref;
};

struct CgTypeCache {
	template<typename... Ts>
	CgType* alloc(Ts&&... args) noexcept {
		if (auto type = alloc(m_cache, forward<Ts>(args)...)) {
			return type;
		}
		return nullptr;
	}

	static Maybe<CgTypeCache> make(Allocator& allocator, Ulen capacity);

	[[nodiscard]] constexpr Allocator& allocator() const noexcept {
		return m_cache.allocator();
	}

	// Builtin types that are likely used everywhere.
	constexpr CgType* u8()   const noexcept { return m_uints[0]; }
	constexpr CgType* u16()  const noexcept { return m_uints[1]; }
	constexpr CgType* u32()  const noexcept { return m_uints[2]; }
	constexpr CgType* u64()  const noexcept { return m_uints[3]; }
	constexpr CgType* s8()   const noexcept { return m_sints[0]; }
	constexpr CgType* s16()  const noexcept { return m_sints[1]; }
	constexpr CgType* s32()  const noexcept { return m_sints[2]; }
	constexpr CgType* s64()  const noexcept { return m_sints[3]; }
	constexpr CgType* b8()   const noexcept { return m_bools[0]; }
	constexpr CgType* b16()  const noexcept { return m_bools[1]; }
	constexpr CgType* b32()  const noexcept { return m_bools[2]; }
	constexpr CgType* b64()  const noexcept { return m_bools[3]; }
	constexpr CgType* ptr()  const noexcept { return m_ptr;      }
	constexpr CgType* str()  const noexcept { return m_str;      }
	constexpr CgType* unit() const noexcept { return m_unit;     }
	constexpr CgType* va()   const noexcept { return m_va;       }

	template<typename... Ts>
	static CgType* alloc(Cache& cache, Ts&&... args) noexcept {
		if (auto src = CgType::make(cache, forward<Ts>(args)...)) {
			// Search the cache for a representation of 'src' and try to reuse it
			for (const auto& elem : cache) {
				auto type = static_cast<CgType*>(elem);
				if (*type == *src) {
					return type;
				}
			}
			if (auto dst = cache.allocate()) {
				return new (dst, Nat{}) CgType{move(*src)};
			}
		}
		return nullptr;
	}

	~CgTypeCache() noexcept {
		for (const auto& type : m_cache) {
			static_cast<CgType*>(type)->~CgType();
		}
	}

	constexpr CgTypeCache(CgTypeCache&&) noexcept = default;

private:
	constexpr CgTypeCache(Cache&& cache,
	                      CgType *const (&uints)[4],
	                      CgType *const (&sints)[4],
	                      CgType *const (&bools)[4],
	                      CgType *const ptr,
	                      CgType *const str,
	                      CgType *const unit,
	                      CgType *const va) noexcept
		: m_cache{move(cache)}
		, m_uints{uints[0], uints[1], uints[2], uints[3]}
		, m_sints{sints[0], sints[1], sints[2], sints[3]}
		, m_bools{bools[0], bools[1], bools[2], bools[3]}
		, m_ptr{ptr}
		, m_str{str}
		, m_unit{unit}
		, m_va{va}
	{
	}

	Cache m_cache;
	CgType *const m_uints[4]; // Uint{8,16,32,64}
	CgType *const m_sints[4]; // Sint{8,16,32,64}
	CgType *const m_bools[4]; // Bool{8,16,32,64}
	CgType *const m_ptr;      // *void
	CgType *const m_str;      // String
	CgType *const m_unit;     // ()
	CgType *const m_va;       // ...
};

} // namespace Biron

#endif 
