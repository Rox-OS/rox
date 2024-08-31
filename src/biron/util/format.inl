#ifndef BIRON_FORMAT_INL
#define BIRON_FORMAT_INL
#include <biron/util/array.inl>

namespace Biron {

struct Allocator;

Bool format_va(char* dst, Ulen *n, const char* fmt, ...) noexcept;

template<typename... Ts>
Maybe<Array<char>> format(Allocator& allocator, const char* fmt, Ts&&... args) noexcept {
	Ulen n = 0;
	if (!format_va(nullptr, &n, fmt, forward<Ts>(args)...)) {
		return None{};
	}
	Array<char> buf{allocator};
	if (!buf.resize(n + 1)) {
		return None{};
	}
	n = buf.length();
	if (!format_va(buf.data(), &n, fmt, forward<Ts>(args)...)) {
		return None{};
	}
	return buf;
}

} // namespace Biron

#endif // BIRON_FORMAT_INL