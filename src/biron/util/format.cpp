#include <stdarg.h> // va_list, va_start, va_end

#include <biron/util/string.h>
#include <biron/util/format.h>

namespace Biron {

Bool format_va(char* dst, Ulen *n, StringView fmt, ...) noexcept {
	va_list va;
	va_start(va, fmt);

	Ulen len = 0;
	Uint64 dec = 0;
	Bool neg = false;
	char buf[sizeof(Uint64) * 8 + 1];
	for (Ulen l = fmt.length(), i = 0; i < l; i++) {
		auto ch = fmt[i];
		if (ch == '%') switch ((ch = fmt[++i])) {
		case '%':
			// %% => %
			if (dst) {
				*dst++ = '%';
			}
			len++;
			break;
		case 's':
			// %s => const char*
			for (const char* string = va_arg(va, const char *); *string; string++) {
				if (dst) {
					*dst++ = *string;
				}
				len++;
			}
			break;
		case 'S':
			// %S => const StringView*
			{
				const auto& view = *static_cast<const StringView*>(va_arg(va, const void *));
				if (dst) {
					for (auto ch : view) {
						*dst++ = ch;
					}
				}
				len += view.length();
			}
			break;
		case 'd':
			// %d => int
			if (auto v = va_arg(va, int); v < 0) {
				dec = -v;
				neg = true;
			} else {
				dec = v;
				neg = false;
			}
			if (0) {
		case 'z':
			// %zu => size_t
			if (fmt[i + 1] != 'u') {
				// Unknown format specifier %z?
				*n = len;
				return false;
			}
			dec = va_arg(va, Ulen);
			neg = false;
			i++;
			if (0) {
		case 'u':
			// %u => unsigned int
			dec = va_arg(va, unsigned int);
			neg = false;
			}}
			{
				auto p = &buf[sizeof buf - 1];
				auto d = dec;
				do {
					*(p--) = "0123456789"[d % 10];
					d /= 10;
				} while (d);
				*p = '-';
				p += neg ? 0 : 1;
				auto s = &buf[sizeof buf] - p;
				for (decltype(s) i = 0; i < s; i++) {
					if (dst) {
						*dst++ = *p++;
					}
					len++;
				}
			}
			break;
		default:
			// Unknown format specifier.
			*n = len;
			return false;
		} else {
			if (dst) {
				*dst++ = ch;
			}
			len++;
		}
	}
	*n = len;
	return true;
}

} // namespace Biron