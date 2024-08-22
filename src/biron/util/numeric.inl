#ifndef BIRON_UTIL_NUMERIC_INL
#define BIRON_UTIL_NUMERIC_INL
#include <biron/util/forward.inl>
#include <biron/util/types.inl> // Ulen

namespace Biron {

template<typename T> constexpr T
min(T&& t) {
	return forward<T>(t);
}
template<typename T> constexpr T
max(T&& t) {
	return forward<T>(t);
}

template<typename T0, typename T1, typename... Ts>
constexpr auto
min(T0&& v0, T1&& v1, Ts&&... vs) {
	return v0 < v1 ? min(v0, forward<Ts>(vs)...) : min(v1, forward<Ts>(vs)...);
}

template<typename T0, typename T1, typename... Ts>
constexpr auto
max(T0&& v0, T1&& v1, Ts&&... vs) {
	return v0 > v1 ? max(v0, forward<Ts>(vs)...) : max(v1, forward<Ts>(vs)...);
}

struct Range {
	constexpr Range(Ulen offset, Ulen length) noexcept
		: offset{offset}
		, length{length}
	{
	}
	Range include(Range other) const noexcept {
		Ulen off = min(beg(), other.beg());
		Ulen len = max(end(), other.end()) - off;
		return {off, len};
	}
	bool operator==(Range other) noexcept {
		return beg() == other.beg() && end() == other.end();
	}
	Ulen beg() const noexcept { return offset; }
	Ulen end() const noexcept { return offset + length; }
	Ulen offset = 0;
	Ulen length = 0;
};

template<typename T>
struct limits {
};

template<> struct limits<Uint8> {
	static inline constexpr const auto MIN = 0_u8;
	static inline constexpr const auto MAX = 0xff_u8;
};
template<> struct limits<Uint16> {
	static inline constexpr const auto MIN = 0_u16;
	static inline constexpr const auto MAX = 0xff'ff_u16;
};
template<> struct limits<Uint32> {
	static inline constexpr const auto MIN = 0_u32;
	static inline constexpr const auto MAX = 0xffff'ffff_u32;
};
template<> struct limits<Uint64> {
	static inline constexpr const auto MIN = 0_u64;
	static inline constexpr const auto MAX = 0xffff'ffff'ffff'ffff_u64;
};

template<> struct limits<Sint8> {
	static inline constexpr const auto MIN = -0x7f_s8 - 1_s8;
	static inline constexpr const auto MAX =  0x7f_s8;
};
template<> struct limits<Sint16> {
	static inline constexpr const auto MIN = -0x7f'ff_s16 - 1_s16;
	static inline constexpr const auto MAX =  0x7f'ff_s16;
};
template<> struct limits<Sint32> {
	static inline constexpr const auto MIN = -0x7f'ff'ff'ff_s32 - 1_s32;
	static inline constexpr const auto MAX =  0x7f'ff'ff'ff_s32;
};
template<> struct limits<Sint64> {
	static inline constexpr const auto MIN = -0x7f'ff'ff'ff'ff'ff'ff'ff_s64 - 1_s64;
	static inline constexpr const auto MAX =  0x7f'ff'ff'ff'ff'ff'ff'ff_s64;
};

} // namespace Biron

#endif // BIRON_UTIL_NUMERIC_INL