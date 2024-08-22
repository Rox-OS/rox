#ifndef BIRON_TYPES_INL
#define BIRON_TYPES_INL

namespace Biron {

#if !defined(__has_builtin)
#define __has_builtin(...)
#endif // !defined(__has_builtin)

#if !defined(__has_feature)
#define __has_feature(...)
#endif // !defined(__has_feature)

using Sint8 = signed char;
using Uint8 = unsigned char;
using Sint16 = signed short;
using Uint16 = unsigned short;
using Sint32 = signed int;
using Uint32 = unsigned int;
using Sint64 = signed long long;
using Uint64 = unsigned long long;
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

typedef struct {} Nat;

// Nats always compare equal.
constexpr Bool operator==(Nat, Nat) noexcept { return true; }
constexpr Bool operator!=(Nat, Nat) noexcept { return false; }

// Like the sizeof and alignof operator countof gets the count of some type.
template<typename T, Ulen E>
constexpr Ulen countof(const T (&)[E]) noexcept {
	return E;
}

} // namespace Biron;

#endif // TYPES_H