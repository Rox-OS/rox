#ifndef BIRON_CG_TYPE_H
#define BIRON_CG_TYPE_H

#include <biron/util/pool.h>
#include <biron/util/string.inl>

#include <biron/llvm.h>

namespace Biron {

struct Cg;
struct CgTypeCache;

struct AstType;

struct CgType {
	enum class Kind {
		U8, U16, U32, U64, // Uint{8,16,32,64}
		S8, S16, S32, S64, // Sint{8,16,32,64}
		B8, B16, B32, B64, // Bool{8,16,32,64}
		F32, F64,          // Float{32, 64}
		STRING,            // String
		POINTER,           // *T
		SLICE,             // []T
		ARRAY,             // [N]T
		PADDING,           // [N]u8 // Special meta-type for tuple padding
		TUPLE,             // (T1, ..., Tn)
		UNION,             // T1 | ... | Tn
		FN,                // fn (T1, ..., Tn) -> (R1, ..., Rn)
		VA,                // ...
	};

	using Field = Maybe<StringView>;

	void dump(StringBuilder& builder) const noexcept;

	CgType* addrof(Cg& cg) noexcept;

	[[nodiscard]] CgType* deref() const noexcept { return at(0); }
	[[nodiscard]] CgType* at(Ulen i) const noexcept {
		return types()[i];
	}

	[[nodiscard]] constexpr const Array<CgType*>& types() const noexcept {
		BIRON_ASSERT(m_types && "No nested types");
		return (*m_types);
	}
	[[nodiscard]] constexpr const Array<Field>& fields() const noexcept {
		BIRON_ASSERT(m_fields && "No nested fields");
		return (*m_fields);
	}

	[[nodiscard]] constexpr Ulen size() const noexcept { return m_size; }
	[[nodiscard]] constexpr Ulen align() const noexcept { return m_align; }
	[[nodiscard]] constexpr Ulen length() const noexcept { return m_types ? m_types->length() : 0; };
	[[nodiscard]] constexpr Ulen extent() const noexcept { return m_extent; };

	[[nodiscard]] constexpr Kind kind() const noexcept { return m_kind; }

	[[nodiscard]] constexpr Bool is_bool() const noexcept { return m_kind >= Kind::B8 && m_kind <= Kind::B64; }
	[[nodiscard]] constexpr Bool is_sint() const noexcept { return m_kind >= Kind::S8 && m_kind <= Kind::S64; }
	[[nodiscard]] constexpr Bool is_uint() const noexcept { return m_kind >= Kind::U8 && m_kind <= Kind::U64; }
	[[nodiscard]] constexpr Bool is_real() const noexcept { return m_kind >= Kind::F32 && m_kind <= Kind::F64; }

	[[nodiscard]] constexpr Bool is_pointer() const noexcept { return m_kind == Kind::POINTER; }
	[[nodiscard]] constexpr Bool is_string() const noexcept { return m_kind == Kind::STRING; }
	[[nodiscard]] constexpr Bool is_slice() const noexcept { return m_kind == Kind::SLICE; }
	[[nodiscard]] constexpr Bool is_array() const noexcept { return m_kind == Kind::ARRAY; }
	[[nodiscard]] constexpr Bool is_padding() const noexcept { return m_kind == Kind::PADDING; }
	[[nodiscard]] constexpr Bool is_tuple() const noexcept { return m_kind == Kind::TUPLE; }
	[[nodiscard]] constexpr Bool is_fn() const noexcept { return m_kind == Kind::FN; }
	[[nodiscard]] constexpr Bool is_va() const noexcept { return m_kind == Kind::VA; }

	[[nodiscard]] constexpr LLVM::TypeRef ref() const noexcept { return m_ref; }

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

	struct FltInfo {
		Ulen size;
		Ulen align;
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

	struct StringInfo {};

	struct TupleInfo {
		Array<CgType*>                  types;
		Maybe<Array<Maybe<StringView>>> fields;
	};

	struct UnionInfo {
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

	CgType(Kind kind, Ulen size, Ulen align, Ulen extent, Maybe<Array<CgType*>>&& types, Maybe<Array<Field>>&& fields, LLVM::TypeRef ref) noexcept
		: m_kind{kind}
		, m_size{size}
		, m_align{align}
		, m_extent{extent}
		, m_types{move(types)}
		, m_fields{move(fields)}
		, m_ref{ref}
	{
	}

	Kind m_kind;
	Ulen m_size;
	Ulen m_align;
	Ulen m_extent;
	Maybe<Array<CgType*>> m_types;
	Maybe<Array<Field>> m_fields;
	LLVM::TypeRef m_ref;
};

struct CgTypeCache {
	static Maybe<CgTypeCache> make(Allocator& allocator, LLVM& llvm, LLVM::ContextRef context, Ulen capacity) noexcept;

	// Builtin types that are likely used everywhere.
	constexpr CgType* u8()   const noexcept { return m_builtin[0]; }
	constexpr CgType* u16()  const noexcept { return m_builtin[1]; }
	constexpr CgType* u32()  const noexcept { return m_builtin[2]; }
	constexpr CgType* u64()  const noexcept { return m_builtin[3]; }

	constexpr CgType* s8()   const noexcept { return m_builtin[4]; }
	constexpr CgType* s16()  const noexcept { return m_builtin[5]; }
	constexpr CgType* s32()  const noexcept { return m_builtin[6]; }
	constexpr CgType* s64()  const noexcept { return m_builtin[7]; }

	constexpr CgType* b8()   const noexcept { return m_builtin[8]; }
	constexpr CgType* b16()  const noexcept { return m_builtin[9]; }
	constexpr CgType* b32()  const noexcept { return m_builtin[10]; }
	constexpr CgType* b64()  const noexcept { return m_builtin[11]; }

	constexpr CgType* f32()  const noexcept { return m_builtin[12]; }
	constexpr CgType* f64()  const noexcept { return m_builtin[13]; }

	constexpr CgType* ptr()  const noexcept { return m_builtin[14]; }
	constexpr CgType* str()  const noexcept { return m_builtin[15]; }
	constexpr CgType* unit() const noexcept { return m_builtin[16]; }
	constexpr CgType* va()   const noexcept { return m_builtin[17]; }

	CgType* make(CgType::IntInfo info) noexcept;
	CgType* make(CgType::FltInfo info) noexcept;
	CgType* make(CgType::PtrInfo info) noexcept;
	CgType* make(CgType::BoolInfo info) noexcept;
	CgType* make(CgType::StringInfo info) noexcept;
	CgType* make(CgType::TupleInfo info) noexcept;
	CgType* make(CgType::UnionInfo info) noexcept;
	CgType* make(CgType::ArrayInfo info) noexcept;
	CgType* make(CgType::SliceInfo info) noexcept;
	CgType* make(CgType::PaddingInfo info) noexcept;
	CgType* make(CgType::FnInfo info) noexcept;
	CgType* make(CgType::VaInfo info) noexcept;

	~CgTypeCache() noexcept {
		for (const auto& type : m_cache) {
			static_cast<CgType*>(type)->~CgType();
		}
	}

	constexpr CgTypeCache(CgTypeCache&&) noexcept = default;

private:
	constexpr CgTypeCache(Cache&& cache, LLVM& llvm, LLVM::ContextRef context) noexcept
		: m_cache{move(cache)}
		, m_llvm{llvm}
		, m_context{context}
		, m_padding_cache{m_cache.allocator()}
	{
	}

	Cache m_cache;
	LLVM& m_llvm;
	LLVM::ContextRef m_context;
	CgType* m_builtin[18];
	Array<CgType*> m_padding_cache; // Indexed by padding size
};

} // namespace Biron

#endif