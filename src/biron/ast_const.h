#ifndef BIRON_AST_CONST_H
#define BIRON_AST_CONST_H

#include <stdio.h>

#include <biron/util/numeric.inl>
#include <biron/util/array.inl>
#include <biron/util/string.inl>
#include <biron/util/unreachable.inl>

namespace Biron {

struct Cg;
struct CgAddr;
struct CgValue;
struct CgType;

// Compile time typed constants
struct AstConst {
	enum class Kind {
		NONE,
		U8, U16, U32, U64, // digit+
		S8, S16, S32, S64, // digit+
		B8, B16, B32, B64,
		TUPLE,             // (...)
		STRING,            // ".*"
		ARRAY,             // {}
	};

	constexpr AstConst(Range range) noexcept
		: m_range{range}, m_kind{Kind::NONE}, m_as_nat{} {}
	constexpr AstConst(Range range, Uint8 value) noexcept
		: m_range{range}, m_kind{Kind::U8}, m_as_u8{value} {}
	constexpr AstConst(Range range, Uint16 value) noexcept
	  : m_range{range}, m_kind{Kind::U16}, m_as_u16{value} {}
	constexpr AstConst(Range range, Uint32 value) noexcept
		: m_range{range}, m_kind{Kind::U32}, m_as_u32{value} {}
	constexpr AstConst(Range range, Uint64 value) noexcept
		: m_range{range}, m_kind{Kind::U64}, m_as_u64{value} {}
	constexpr AstConst(Range range, Sint8 value) noexcept
		: m_range{range}, m_kind{Kind::S8}, m_as_s8{value} {}
	constexpr AstConst(Range range, Sint16 value) noexcept
	: m_range{range}, m_kind{Kind::S16}, m_as_s16{value} {}
	constexpr AstConst(Range range, Sint32 value) noexcept
		: m_range{range}, m_kind{Kind::S32}, m_as_s32{value} {}
	constexpr AstConst(Range range, Sint64 value) noexcept
		: m_range{range}, m_kind{Kind::S64}, m_as_s64{value} {}
	constexpr AstConst(Range range, Bool8 value) noexcept
		: m_range{range}, m_kind{Kind::B8}, m_as_b8{value} {}
	constexpr AstConst(Range range, Bool16 value) noexcept
		: m_range{range}, m_kind{Kind::B16}, m_as_b16{value} {}
	constexpr AstConst(Range range, Bool32 value) noexcept
		: m_range{range}, m_kind{Kind::B32}, m_as_b32{value} {}
	constexpr AstConst(Range range, Bool64 value) noexcept
		: m_range{range}, m_kind{Kind::B8}, m_as_b64{value} {}
	constexpr AstConst(Range range, Array<AstConst>&& tuple) noexcept
		: m_range{range}, m_kind{Kind::TUPLE}, m_as_tuple{move(tuple)} {}
	constexpr AstConst(Range range, StringView string)
		: m_range{range}, m_kind{Kind::STRING}, m_as_string{string} {}
	constexpr AstConst(Range range, CgType* base, Array<AstConst>&& array)
		: m_range{range}, m_kind{Kind::ARRAY}, m_as_array{base, move(array)} {}

	AstConst(AstConst&& other) noexcept;
	~AstConst() noexcept { drop(); }

	[[nodiscard]] constexpr Kind kind() const noexcept { return m_kind; }
	[[nodiscard]] constexpr Range range() const noexcept { return m_range; }

	[[nodiscard]] constexpr Bool is_integral() const noexcept {
		return m_kind >= Kind::U8 && m_kind <= Kind::S64;
	}
	[[nodiscard]] constexpr Bool is_boolean() const noexcept {
		return m_kind >= Kind::B8 && m_kind <= Kind::B64;
	}
	[[nodiscard]] constexpr Bool is_tuple() const noexcept { return m_kind == Kind::TUPLE; }
	[[nodiscard]] constexpr Bool is_array() const noexcept { return m_kind == Kind::ARRAY; }

	[[nodiscard]] constexpr Uint8 as_u8() const noexcept { return m_as_u8; }
	[[nodiscard]] constexpr Uint16 as_u16() const noexcept { return m_as_u16; }
	[[nodiscard]] constexpr Uint32 as_u32() const noexcept { return m_as_u32; }
	[[nodiscard]] constexpr Uint64 as_u64() const noexcept { return m_as_u64; }
	[[nodiscard]] constexpr Sint8 as_s8() const noexcept { return m_as_s8; }
	[[nodiscard]] constexpr Sint16 as_s16() const noexcept { return m_as_s16; }
	[[nodiscard]] constexpr Sint32 as_s32() const noexcept { return m_as_s32; }
	[[nodiscard]] constexpr Sint64 as_s64() const noexcept { return m_as_s64; }
	[[nodiscard]] constexpr Bool8 as_b8() const noexcept { return m_as_b8; }
	[[nodiscard]] constexpr Bool16 as_b16() const noexcept { return m_as_b16; }
	[[nodiscard]] constexpr Bool32 as_b32() const noexcept { return m_as_b32; }
	[[nodiscard]] constexpr Bool64 as_b64() const noexcept { return m_as_b64; }

	[[nodiscard]] constexpr const Array<AstConst>& as_tuple() const noexcept { return m_as_tuple; }
	[[nodiscard]] constexpr StringView as_string() const noexcept { return m_as_string; }

	Maybe<AstConst> copy() const noexcept;

	template<typename T>
	Maybe<AstConst> to() const noexcept;

	Maybe<CgValue> codegen(Cg& cg) const noexcept;

private:
	struct ConstArray {
		constexpr ConstArray(CgType* type, Array<AstConst>&& elems) noexcept
			: type{type}, elems{move(elems)}
		{
		}
		constexpr ConstArray(ConstArray&&) noexcept = default;
		CgType*         type;
		Array<AstConst> elems;
	};

	Range m_range;
	Kind m_kind;

	union {
		Nat             m_as_nat;
		Uint8           m_as_u8;
		Uint16          m_as_u16;
		Uint32          m_as_u32;
		Uint64          m_as_u64;
		Sint8           m_as_s8;
		Sint16          m_as_s16;
		Sint32          m_as_s32;
		Sint64          m_as_s64;
		Bool8           m_as_b8;
		Bool16          m_as_b16;
		Bool32          m_as_b32;
		Bool64          m_as_b64;
		Array<AstConst> m_as_tuple;
		StringView      m_as_string;
		ConstArray      m_as_array;
	};

	void drop() {
		if (m_kind == Kind::TUPLE) {
			m_as_tuple.~Array<AstConst>();
		} else if (m_kind == Kind::ARRAY) {
			m_as_array.elems.~Array<AstConst>();
		}
	}
};

// Compile time casting logic for AstConst
template<typename T>
Maybe<AstConst> AstConst::to() const noexcept {
	auto cast = [this](auto value) -> Maybe<AstConst> {
		// TODO(dweiler): We need a larger integer type to safely check the bounds.
		return AstConst { range(), static_cast<T>(value) };
	};
	switch (m_kind) {
	case Kind::NONE: return AstConst { range() };
	case Kind::U8:   return cast(as_u8());
	case Kind::U16:  return cast(as_u16());
	case Kind::U32:  return cast(as_u32());
	case Kind::U64:  return cast(as_u64());
	case Kind::S8:   return cast(as_s8());
	case Kind::S16:  return cast(as_s16());
	case Kind::S32:  return cast(as_s32());
	case Kind::S64:  return cast(as_s64());
	case Kind::B8:   return cast(as_b8());
	case Kind::B16:  return cast(as_b16());
	case Kind::B32:  return cast(as_b32());
	case Kind::B64:  return cast(as_b64());
	default:
		// Cannot perform constant compile time cast
		return None{};
	}
}

} // namespace Biron

#endif // BIRON_AST_CONST_H