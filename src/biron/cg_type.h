#ifndef BIRON_CG_TYPE_H
#define BIRON_CG_TYPE_H
#include <biron/llvm.h>

#include <biron/util/pool.h>
#include <biron/util/string.h>

#include <biron/ast_const.h>

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
		ATOMIC,            // @T
		SLICE,             // []T
		ARRAY,             // [N]T
		PADDING,           // [N]u8 // Special meta-type for tuple padding
		TUPLE,             // (T1, ..., Tn)
		UNION,             // T1 | ... | Tn
		ENUM,              // {E1, ... En}
		FN,                // fn (T1, ..., Tn) -> (R1, ..., Rn)
		VA,                // ...
	};

	// using Field = Maybe<StringView>;

	void dump(StringBuilder& builder) const noexcept;
	StringView to_string(Allocator& allocator) const noexcept;

	CgType* addrof(Cg& cg) noexcept;

	[[nodiscard]] CgType* deref() const noexcept { return at(0); }
	[[nodiscard]] CgType* at(Ulen i) const noexcept { return types()[i]; }

	[[nodiscard]] constexpr const Array<CgType*>& types() const noexcept {
		BIRON_ASSERT(m_types && "No nested types");
		return (*m_types);
	}
	[[nodiscard]] constexpr const Array<ConstField>& fields() const noexcept {
		BIRON_ASSERT(m_fields && "No nested fields");
		return (*m_fields);
	}

	// Custom make tags for the type cache
	struct Layout {
		Ulen size;
		Ulen align;
		constexpr Bool operator==(const Layout& other) const noexcept {
			return size == other.size && align == other.align;
		}
		constexpr Bool operator!=(const Layout& other) const noexcept {
			return size != other.size || align != other.align;
		}
	};

	[[nodiscard]] constexpr const Layout& layout() const noexcept { return m_layout; }
	[[nodiscard]] constexpr Ulen size() const noexcept { return m_layout.size; }
	[[nodiscard]] constexpr Ulen align() const noexcept { return m_layout.align; }
	[[nodiscard]] constexpr Ulen length() const noexcept { return m_types ? m_types->length() : 0; };
	[[nodiscard]] constexpr Ulen extent() const noexcept { return m_extent; };

	[[nodiscard]] constexpr Kind kind() const noexcept { return m_kind; }

	[[nodiscard]] constexpr Bool is_bool() const noexcept {
		return m_kind >= Kind::B8 && m_kind <= Kind::B64;
	}
	[[nodiscard]] constexpr Bool is_sint() const noexcept {
		return m_kind >= Kind::S8 && m_kind <= Kind::S64;
	}
	[[nodiscard]] constexpr Bool is_uint() const noexcept {
		return m_kind >= Kind::U8 && m_kind <= Kind::U64;
	}
	[[nodiscard]] constexpr Bool is_real() const noexcept {
		return m_kind >= Kind::F32 && m_kind <= Kind::F64;
	}
	[[nodiscard]] constexpr Bool is_integer() const noexcept {
		return is_sint() || is_uint();
	}
	[[nodiscard]] constexpr Bool is_f32() const noexcept {
		return m_kind == Kind::F32;
	}
	[[nodiscard]] constexpr Bool is_f64() const noexcept {
		return m_kind == Kind::F64;
	}
	[[nodiscard]] constexpr Bool is_pointer() const noexcept { return m_kind == Kind::POINTER; }
	[[nodiscard]] constexpr Bool is_string() const noexcept { return m_kind == Kind::STRING; }
	[[nodiscard]] constexpr Bool is_slice() const noexcept { return m_kind == Kind::SLICE; }
	[[nodiscard]] constexpr Bool is_array() const noexcept { return m_kind == Kind::ARRAY; }
	[[nodiscard]] constexpr Bool is_padding() const noexcept { return m_kind == Kind::PADDING; }
	[[nodiscard]] constexpr Bool is_tuple() const noexcept { return m_kind == Kind::TUPLE; }
	[[nodiscard]] constexpr Bool is_fn() const noexcept { return m_kind == Kind::FN; }
	[[nodiscard]] constexpr Bool is_va() const noexcept { return m_kind == Kind::VA; }
	[[nodiscard]] constexpr Bool is_atomic() const noexcept { return m_kind == Kind::ATOMIC; }
	[[nodiscard]] constexpr Bool is_enum() const noexcept { return m_kind == Kind::ENUM; }

	[[nodiscard]] constexpr LLVM::TypeRef ref() const noexcept { return m_ref; }

	constexpr const Maybe<StringView>& name() const noexcept { return m_name; }

	[[nodiscard]] Bool operator!=(const CgType& other) const noexcept {
		if (other.m_kind != m_kind) {
			return true;
		}
		if (other.m_layout != m_layout) {
			return true;
		}
		if (other.m_extent != m_extent) {
			return true;
		}
		if (other.m_types) { 
			if (!m_types) {
				// Other has types but we do not.
				return true;
			}
			const auto& lhs = *other.m_types;
			const auto& rhs = *m_types;
			if (lhs.length() != rhs.length()) {
				// The type lists are not the same length.
				return true;
			}
			for (Ulen l = lhs.length(), i = 0; i < l; i++) {
				if (*lhs[i] != *rhs[i]) {
					// The types are not the same.
					return true;
				}
			}
		}
		// We do not compare m_ref
		return false;
	}

	[[nodiscard]] Bool operator==(const CgType& other) const noexcept {
		return !operator!=(other);
	}

	struct IntInfo : Layout {
		Bool              sign;
		Maybe<StringView> named;
	};

	struct RealInfo : Layout {
		Maybe<StringView> named;
	};

	struct PtrInfo : Layout {
		CgType*           base;
		Maybe<StringView> named;
	};

	struct BoolInfo : Layout {
		Maybe<StringView> named;
	};

	struct StringInfo : Layout {
		Maybe<StringView> named;
	};

	struct TupleInfo {
		Array<CgType*>           types;
		Maybe<Array<ConstField>> fields;
		Maybe<StringView>        named;
	};

	struct UnionInfo {
		Array<CgType*>    types;
		Maybe<StringView> named;
	};

	struct ArrayInfo {
		CgType*           base;
		Ulen              extent;
		Maybe<StringView> named;
	};

	struct SliceInfo {
		CgType* base;
	};

	struct PaddingInfo {
		Ulen    padding;
	};

	struct FnInfo {
		CgType* objs;
		CgType* args;
		CgType* effects;
		CgType* rets;
	};

	struct VaInfo { };

	struct AtomicInfo {
		CgType*           base;
		Maybe<StringView> named;
	};

	struct EnumInfo {
		CgType*           base;
		Array<ConstField> fields;
		Maybe<StringView> named;
	};

private:
	friend struct CgTypeCache;
	friend struct Cache;

	CgType(Kind kind,
	       Layout layout,
	       Ulen extent,
	       Maybe<Array<CgType*>>&& types,
	       Maybe<Array<ConstField>>&& fields,
	       Maybe<StringView> name,
	       LLVM::TypeRef ref) noexcept
		: m_kind{kind}
		, m_layout{layout}
		, m_extent{extent}
		, m_types{move(types)}
		, m_fields{move(fields)}
		, m_name{move(name)}
		, m_ref{ref}
	{
	}

	Kind m_kind;
	Layout m_layout;
	Ulen m_extent;
	Maybe<Array<CgType*>> m_types;
	Maybe<Array<ConstField>> m_fields;
	Maybe<StringView> m_name;
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
	CgType* make(CgType::RealInfo info) noexcept;
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
	CgType* make(CgType::AtomicInfo info) noexcept;
	CgType* make(CgType::EnumInfo info) noexcept;

	~CgTypeCache() noexcept {
		for (const auto& type : m_cache) {
			static_cast<CgType*>(type)->~CgType();
		}
	}

	constexpr CgTypeCache(CgTypeCache&&) noexcept = default;

private:
	CgType* ensure_padding(Ulen padding) noexcept {
		if (auto find = m_padding_cache.at(padding); find && *find) {
			return *find;
		}
		if (!m_padding_cache.resize(padding + 1)) {
			return nullptr;
		}
		auto pad = make(CgType::PaddingInfo { padding });
		if (!pad) {
			return nullptr;
		}
		m_padding_cache[padding] = pad;
		return pad;
	};

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