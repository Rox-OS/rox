#include <string.h> // memcpy
#include <stdio.h>  // snprintf
#include <float.h>  // FLT_MANT_DIG, FLT_DECIMAL_DIG DBL_MANT_DIG, DBL_DECIMAL_DIG

#include <biron/util/string.inl>

namespace Biron {

Bool operator==(StringView lhs, StringView rhs) noexcept {
	if (lhs.data() == rhs.data()) return true;
	if (lhs.length() != rhs.length()) return false;
	for (Ulen l = lhs.length(), i = 0; i < l; i++) {
		if (lhs[i] != rhs[i]) return false;
	}
	return true;
}

Bool operator!=(StringView lhs, StringView rhs) noexcept {
	if (lhs.data() == rhs.data()) return false;
	if (lhs.length() != rhs.length()) return true;
	for (Ulen l = lhs.length(), i = 0; i < l; i++) {
		if (lhs[i] != rhs[i]) return true;
	}
	return false;
}

Bool StringView::starts_with(StringView other) const noexcept {
	if (other.length() > length()) return false;
	for (Ulen l = other.length(), i = 0; i < l; i++) {
		if (operator[](i) != other[i]) {
			return false;
		}
	}
	return true;
}

Maybe<Ulen> StringView::find_first_of(int ch) const noexcept {
	for (Ulen l = length(), i = 0; i < l; i++) {
		if (operator[](i) == ch) {
			return i;
		}
	}
	return None{};
}

Maybe<Ulen> StringView::find_last_of(int ch) const noexcept {
	Maybe<Ulen> found;
	for (Ulen l = length(), i = 0; i < l; i++) {
		if (operator[](i) == ch) {
			found = i;
		}
	}
	return found;
}

char* StringView::terminated(Allocator& allocator) const noexcept {
	if (char *dst = reinterpret_cast<char *>(allocator.allocate(m_length + 1))) {
		memcpy(dst, m_data, m_length);
		dst[m_length] = '\0';
		return dst;
	}
	return nullptr;
}

Bool StringBuilder::append(char ch) noexcept {
	if (!m_valid) return false;
	m_valid = m_buffer.push_back(ch);
	return m_valid;
}

Bool StringBuilder::append(Sint64 value) noexcept {
	if (value < 0) {
		if (!append('-')) {
			return false;
		}
		value = -value;
	}
	if (append(static_cast<Uint64>(value))) {
		return true;
	}
	pop(); // Remove '-'
	return false;
}

Bool StringBuilder::append(Uint64 value) noexcept {
	if (value == 0) {
		return append('0');
	}

	// Measure how many characters we need to append.
	Uint64 length = 0;
	for (Uint64 v = value; v; v /= 10, length++);
	Ulen offset = m_buffer.length();

	// Resize to allow that many characters to be written.
	if (!m_buffer.resize(offset + length)) {
		m_valid = false;
		return false;
	}

	// Fill the result from the end since this operates in reverse order.
	char *const fill = m_buffer.data() + offset;
	for (; value; value /= 10) {
		fill[--length] = '0' + (value % 10);
	}
	return true;
}

Bool StringBuilder::append(Float32 value) noexcept {
	char buffer[FLT_MANT_DIG + FLT_DECIMAL_DIG * 2 + 1];
	auto n = snprintf(buffer, sizeof buffer, "%f", value);
	if (n <= 0) {
		return false;
	}
	return append(StringView { buffer, Ulen(n) });
}

Bool StringBuilder::append(Float64 value) noexcept {
	char buffer[DBL_MANT_DIG + DBL_DECIMAL_DIG * 2 + 1];
	auto n = snprintf(buffer, sizeof buffer, "%f", value);
	if (n <= 0) {
		return false;
	}
	return append(StringView { buffer, Ulen(n) });
}

Bool StringBuilder::append(StringView view) noexcept {
	if (!m_valid) return false;
	Ulen offset = m_buffer.length();
	if (!m_buffer.resize(offset + view.length())) {
		m_valid = false;
		return false;
	}
	memcpy(m_buffer.data() + offset, view.data(), view.length());
	return true;
}

Bool StringBuilder::repeat(int ch, Ulen count) noexcept {
	if (!m_valid) return false;
	Ulen offset = m_buffer.length();
	if (!m_buffer.resize(offset + count)) {
		m_valid = false;
		return false;
	}
	char *const fill = m_buffer.data() + offset;
	for (Ulen i = 0; i < count; i++) {
		fill[i] = ch;
	}
	return true;
}

Bool StringBuilder::repeat(StringView view, Ulen count) noexcept {
	if (!m_valid) return false;
	Ulen offset = m_buffer.length();
	Ulen expand = view.length() * count;
	if (!m_buffer.resize(offset + expand)) {
		m_valid = false;
		return false;
	}
	char* fill = m_buffer.data() + offset;
	for (Ulen i = 0; i < count; i++) {
		memcpy(fill, view.data(), view.length());
		fill += view.length();
	}
	return true;
}

} // namespace Biron