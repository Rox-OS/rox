#ifndef BIRON_TYPES_INL
#define BIRON_TYPES_INL

namespace Biron {

#if !defined(__has_builtin)
#define __has_builtin(...) 0
#endif // !defined(__has_builtin)

#if !defined(__has_feature)
#define __has_feature(...) 0
#endif // !defined(__has_feature)

#if defined(__clang__)
#define BIRON_COMPILER_CLANG
#elif defined(__GNUC__) || defined(__GNUG__)
#define BIRON_COMPILER_GCC
#elif defined(_MSC_VER)
#define BIRON_COMPILER_MSVC
#endif

using Sint8 = signed char;
using Uint8 = unsigned char;
using Sint16 = signed short;
using Uint16 = unsigned short;
using Sint32 = signed int;
using Uint32 = unsigned int;
using Sint64 = signed long long;
using Uint64 = unsigned long long;
using Float32 = float;
using Float64 = double;
using Ulen = decltype(sizeof 0);
using Bool = bool;

// Some numeric literal operators to force specific types
consteval Uint8 operator""_u8(Uint64 value) noexcept { return value; }
consteval Uint16 operator""_u16(Uint64 value) noexcept { return value; }
consteval Uint32 operator""_u32(Uint64 value) noexcept { return value; }
consteval Uint64 operator""_u64(Uint64 value) noexcept { return value; }
consteval Sint8 operator""_s8(Uint64 value) noexcept { return value; }
consteval Sint16 operator""_s16(Uint64 value) noexcept { return value; }
consteval Sint32 operator""_s32(Uint64 value) noexcept { return value; }
consteval Sint64 operator""_s64(Uint64 value) noexcept { return value; }
consteval Ulen operator""_ulen(Uint64 value) noexcept { return value; }

typedef struct {} Nat;

// Nats always compare equal.
constexpr Bool operator==(Nat, Nat) noexcept { return true; }
constexpr Bool operator!=(Nat, Nat) noexcept { return false; }

// Like the sizeof and alignof operator countof gets the count of an array.
template<typename T, Ulen E>
constexpr Ulen countof(const T (&)[E]) noexcept {
	return E;
}

// Biron has distinct typed and sized booleans which would be nice to have in
// C++ so they can participate in the overload set within the compiler itself.
// This means we cannot just make them a simple typedef of integer types since
// those would conflict. Here we define a custom Boolean type to overcome this.
template<typename T>
struct Boolean {
	constexpr Boolean(Bool value) noexcept
		: m_bool{value ? T(1) : T(0)}
	{
	}
	[[nodiscard]] constexpr operator Bool() const noexcept {
		return m_bool != 0u;
	}
	[[nodiscard]] constexpr Boolean operator&&(Boolean other) const noexcept {
		return m_bool && other.m_bool;
	}
	[[nodiscard]] constexpr Boolean operator||(Boolean other) const noexcept {
		return m_bool || other.m_bool;
	}
	[[nodiscard]] constexpr Boolean operator!() const noexcept {
		return m_bool ? false : true;
	}
private:
	T m_bool;
};

using Bool8 = Boolean<Uint8>;
using Bool16 = Boolean<Uint16>;
using Bool32 = Boolean<Uint32>;
using Bool64 = Boolean<Uint64>;

} // namespace Biron;

#endif // TYPES_H