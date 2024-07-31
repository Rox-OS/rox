#ifndef BIRON_UTIL_NUMERIC_INL
#define BIRON_UTIL_NUMERIC_INL
#include <biron/util/forward.inl>

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
	Range include(Range other) noexcept {
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

} // namespace Biron

#endif // BIRON_UTIL_NUMERIC_INL