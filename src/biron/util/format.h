#ifndef BIRON_FORMAT_H
#define BIRON_FORMAT_H
#include <biron/util/string.h>
#include <biron/util/array.inl>
#include <biron/util/traits/remove_cvref.inl>

namespace Biron {

struct Allocator;

Bool format_va(char* dst, Ulen *n, StringView fmt, ...) noexcept;

template<typename T>
struct FormatNormalize {
	constexpr T operator()(const T& value) const noexcept {
		return value;
	}
};

template<Ulen E>
struct FormatNormalize<char[E]> {
	constexpr const char* operator()(const char (&value)[E]) const noexcept {
		return value;
	}
};

template<>
struct FormatNormalize<StringView> {
	// We cannot pass StringView to format_va since it's C-style variable argument
	// We instead make a copy of the value and return a pointer to the copy. The
	// format_va function can recognize %S as const StringView* and will format it.
	// We do this instead of writing all the formatting code inside the header.
	const StringView* operator()(const StringView& value) const noexcept {
		m_capture = value;
		return &m_capture;
	}
private:
	mutable StringView m_capture;
};

template<typename... Ts>
Maybe<Array<char>> format(Allocator& allocator, StringView fmt, Ts&&... args) noexcept {
	Ulen n = 0;
	if (!format_va(nullptr,
	               &n,
	               fmt,
	               FormatNormalize<RemoveCVRef<Ts>>{}(forward<Ts>(args))...))
	{
		return None{};
	}
	Array<char> buf{allocator};
	if (!buf.resize(n)) {
		return None{};
	}
	n = buf.length();
	if (!format_va(buf.data(),
	               &n,
	               fmt,
	               FormatNormalize<RemoveCVRef<Ts>>{}(forward<Ts>(args))...))
	{
		return None{};
	}
	return buf;
}

} // namespace Biron

#endif // BIRON_FORMAT_INL