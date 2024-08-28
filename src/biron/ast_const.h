#ifndef BIRON_AST_CONST_H
#define BIRON_AST_CONST_H

#include <biron/util/numeric.inl>
#include <biron/util/array.inl>
#include <biron/util/string.inl>
#include <biron/util/int128.inl>
#include <biron/util/unreachable.inl>

namespace Biron {

struct Cg;
struct CgValue;

struct AstType;

// Compile time typed constants
struct AstConst {
	enum class Kind {
		NONE,
		U8, U16, U32, U64, // digit+
		S8, S16, S32, S64, // digit+
		B8, B16, B32, B64,
		F32, F64,
		TUPLE,             // (...)
		STRING,            // ".*"
		ARRAY,             // {}
	};

	constexpr AstConst(Range range) noexcept
		: m_range{range}, m_kind{Kind::NONE}, m_as_nat{} {}

	constexpr AstConst(Range range, Kind kind, Uint128 value) noexcept
		: m_range{range}, m_kind{kind}, m_as_uint{value} {}
	constexpr AstConst(Range range, Kind kind, Sint128 value) noexcept
		: m_range{range}, m_kind{kind}, m_as_sint{value} {}
	constexpr AstConst(Range range, Kind kind, Bool128 value) noexcept
		: m_range{range}, m_kind{kind}, m_as_bool{value} {}

	constexpr AstConst(Range range, Uint8 value) noexcept
		: AstConst{range, Kind::U8, static_cast<Uint128>(value)} {}
	constexpr AstConst(Range range, Uint16 value) noexcept
		: AstConst{range, Kind::U16, static_cast<Uint128>(value)} {}
	constexpr AstConst(Range range, Uint32 value) noexcept
		: AstConst{range, Kind::U32, static_cast<Uint128>(value)} {}
	constexpr AstConst(Range range, Uint64 value) noexcept
		: AstConst{range, Kind::U64, static_cast<Uint128>(value)} {}

	constexpr AstConst(Range range, Sint8 value) noexcept
		: AstConst{range, Kind::S8, static_cast<Sint128>(value)} {}
	constexpr AstConst(Range range, Sint16 value) noexcept
		: AstConst{range, Kind::S16, static_cast<Sint128>(value)} {}
	constexpr AstConst(Range range, Sint32 value) noexcept
		: AstConst{range, Kind::S32, static_cast<Sint128>(value)} {}
	constexpr AstConst(Range range, Sint64 value) noexcept
		: AstConst{range, Kind::S64, static_cast<Sint128>(value)} {}

	constexpr AstConst(Range range, Bool8 value) noexcept
		: AstConst{range, Kind::B8, static_cast<Bool128>(value ? true : false)} {}
	constexpr AstConst(Range range, Bool16 value) noexcept
		: AstConst{range, Kind::B16, static_cast<Bool128>(value ? true : false)} {}
	constexpr AstConst(Range range, Bool32 value) noexcept
		: AstConst{range, Kind::B32, static_cast<Bool128>(value ? true : false)} {}
	constexpr AstConst(Range range, Bool64 value) noexcept
		: AstConst{range, Kind::B64, static_cast<Bool128>(value ? true : false)} {}

	constexpr AstConst(Range range, Float32 value) noexcept
		: m_range{range}, m_kind{Kind::F32}, m_as_f32{value} {}
	constexpr AstConst(Range range, Float64 value) noexcept
		: m_range{range}, m_kind{Kind::F64}, m_as_f64{value} {}

	constexpr AstConst(Range range, Array<AstConst>&& tuple) noexcept
		: m_range{range}, m_kind{Kind::TUPLE}, m_as_tuple{move(tuple)} {}

	constexpr AstConst(Range range, StringView string) noexcept
		: m_range{range}, m_kind{Kind::STRING}, m_as_string{string} {}

	constexpr AstConst(Range range, AstType* base, Array<AstConst>&& array) noexcept
		: m_range{range}, m_kind{Kind::ARRAY}, m_as_array{base, move(array)} {}

	AstConst(AstConst&& other) noexcept;
	~AstConst() noexcept { drop(); }

	[[nodiscard]] constexpr Kind kind() const noexcept { return m_kind; }
	[[nodiscard]] constexpr Range range() const noexcept { return m_range; }

	[[nodiscard]] constexpr Bool is_uint() const noexcept { return m_kind >= Kind::U8 && m_kind <= Kind::U64; }
	[[nodiscard]] constexpr Bool is_sint() const noexcept { return m_kind >= Kind::S8 && m_kind <= Kind::S64; }
	[[nodiscard]] constexpr Bool is_bool() const noexcept { return m_kind >= Kind::B8 && m_kind <= Kind::B64; }
	[[nodiscard]] constexpr Bool is_real() const noexcept { return m_kind >= Kind::F32 && m_kind <= Kind::F64; }
	[[nodiscard]] constexpr Bool is_tuple() const noexcept { return m_kind == Kind::TUPLE; }
	[[nodiscard]] constexpr Bool is_array() const noexcept { return m_kind == Kind::ARRAY; }

	[[nodiscard]] constexpr Uint128 as_uint() const noexcept { return m_as_uint; }
	[[nodiscard]] constexpr Sint128 as_sint() const noexcept { return m_as_sint; }
	[[nodiscard]] constexpr Bool128 as_bool() const noexcept { return m_as_bool; }

	[[nodiscard]] constexpr Float32 as_f32() const noexcept { return m_as_f32; }
	[[nodiscard]] constexpr Float64 as_f64() const noexcept { return m_as_f64; }

	[[nodiscard]] constexpr Bool is_integral() const noexcept {
		return is_uint() || is_sint();
	}

	[[nodiscard]] constexpr const Array<AstConst>& as_tuple() const noexcept { return m_as_tuple; }
	[[nodiscard]] constexpr StringView as_string() const noexcept { return m_as_string; }

	Maybe<AstConst> copy() const noexcept;

	template<typename T>
	Maybe<T> to() const noexcept;

	Maybe<CgValue> codegen(Cg& cg) const noexcept;

private:
	struct ConstArray {
		constexpr ConstArray(AstType* type, Array<AstConst>&& elems) noexcept
			: type{type}, elems{move(elems)}
		{
		}
		constexpr ConstArray(ConstArray&&) noexcept = default;
		AstType*        type;
		Array<AstConst> elems;
	};

	Range m_range;
	Kind m_kind;

	union {
		Nat             m_as_nat;
		Sint128         m_as_sint;
		Uint128         m_as_uint;
		Bool128         m_as_bool;
		Float32         m_as_f32;
		Float64         m_as_f64;
		Array<AstConst> m_as_tuple;
		StringView      m_as_string;
		ConstArray      m_as_array;
	};

	void drop() noexcept {
		if (m_kind == Kind::TUPLE) {
			m_as_tuple.~Array<AstConst>();
		} else if (m_kind == Kind::ARRAY) {
			m_as_array.elems.~Array<AstConst>();
		}
	}
};

// Compile time casting logic for AstConst
template<typename T>
Maybe<T> AstConst::to() const noexcept {
	switch (m_kind) {
	case Kind::NONE: return None{};
	case Kind::U8:   return T(m_as_uint);
	case Kind::U16:  return T(m_as_uint);
	case Kind::U32:  return T(m_as_uint);
	case Kind::U64:  return T(m_as_uint);
	case Kind::S8:   return T(m_as_sint);
	case Kind::S16:  return T(m_as_sint);
	case Kind::S32:  return T(m_as_sint);
	case Kind::S64:  return T(m_as_sint);
	case Kind::B8:   return T(m_as_bool ? true : false);
	case Kind::B16:  return T(m_as_bool ? true : false);
	case Kind::B32:  return T(m_as_bool ? true : false);
	case Kind::B64:  return T(m_as_bool ? true : false);
	case Kind::F32:  return T(m_as_f64);
	case Kind::F64:  return T(m_as_f32);
	default:
		// Cannot perform constant compile time cast
		return None{};
	}
}

} // namespace Biron

#endif // BIRON_AST_CONST_H