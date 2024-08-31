#include <stdio.h> // vsnprintf
#include <stdarg.h> // va_list, va_start, va_end

#include <biron/util/format.inl>

namespace Biron {

Bool format_va(char* dst, Ulen *n, const char* fmt, ...) noexcept {
	va_list va;
	va_start(va, fmt);
	auto l = vsnprintf(dst, *n, fmt, va);
	va_end(va);
	if (l <= 0) {
		return false;
	}
	*n = static_cast<Ulen>(l);
	return true;
}

} // namespace Biron