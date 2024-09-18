#ifndef BIRON_STRING_H
#define BIRON_STRING_H
#include <biron/util/types.inl>
#include <biron/util/exchange.inl>
#include <biron/util/array.inl>
#include <biron/util/maybe.inl>

namespace Biron {

struct Allocator;

template<typename T>
concept StringLiteral = Same<T, const char*> || Same<T, char*>;

struct StringView {
	constexpr StringView(const char *data, Ulen len) noexcept
		: m_data{data}
		, m_length{len}
	{
	}
	constexpr StringView() noexcept
		: StringView{"", 0}
	{
	}
	template<Ulen E>
	constexpr StringView(const char (&data)[E]) noexcept
		: StringView{data, E - 1}
	{
	}
	constexpr StringView(StringView&& other) noexcept
		: m_data{exchange(other.m_data, "")}
		, m_length{exchange(other.m_length, 0)}
	{
	}
	constexpr StringView(const StringView& other) noexcept
		: m_data{other.m_data}
		, m_length{other.m_length}
	{
	}
	template<StringLiteral T>
	StringView(const T string) noexcept { assign(string); }
	[[nodiscard]] constexpr StringView slice(Ulen offset, Ulen length) const noexcept {
		return {m_data + offset, length};
	}
	[[nodiscard]] constexpr StringView slice(Ulen offset) const noexcept {
		return {m_data + offset, m_length - offset};
	}
	StringView& operator=(const StringView& name) noexcept {
		m_data = name.m_data;
		m_length = name.m_length;
		return *this;
	}
	[[nodiscard]] constexpr const char* data() const noexcept { return m_data; }
	[[nodiscard]] constexpr Ulen length() const noexcept { return m_length; }
	[[nodiscard]] constexpr const char& operator[](Ulen i) const noexcept { return m_data[i]; }
	[[nodiscard]] constexpr Bool empty() const noexcept { return m_length == 0; }
	friend Bool operator==(StringView lhs, StringView rhs) noexcept;
	friend Bool operator!=(StringView lhs, StringView rhs) noexcept;
	[[nodiscard]] Bool starts_with(StringView other) const noexcept;
	Maybe<Ulen> find_first_of(int ch) const noexcept;
	Maybe<Ulen> find_last_of(int ch) const noexcept;
	char *terminated(Allocator& allocator) const noexcept;
	[[nodiscard]] constexpr const char* begin() const noexcept { return m_data; }
	[[nodiscard]] constexpr const char* end() const noexcept { return m_data + m_length; }
private:
	void assign(const char* string) noexcept;

	const char *m_data;
	Ulen        m_length;
};

struct StringBuilder {
	constexpr StringBuilder(Allocator& allocator) noexcept
		: m_buffer{allocator}
		, m_valid{true}
	{
	}
	void pop() noexcept { m_buffer.pop_back(); }
	Bool append(char ch) noexcept;
	Bool append(Sint32 value) noexcept { return append(static_cast<Sint64>(value)); }
	Bool append(Uint32 value) noexcept { return append(static_cast<Uint64>(value)); }
	Bool append(Sint64 value) noexcept;
	Bool append(Uint64 value) noexcept;
	Bool append(Float32 value) noexcept;
	Bool append(Float64 value) noexcept;
	Bool append(StringView view) noexcept;
	Bool repeat(int ch, Ulen count) noexcept;
	Bool repeat(StringView view, Ulen count) noexcept;
	constexpr Bool valid() const noexcept { return m_valid; }
	constexpr StringView view() const noexcept {
		return { m_buffer.data(), m_buffer.length() };
	}
	constexpr char* data() noexcept { return m_buffer.data(); }
	constexpr const char *data() const noexcept { return m_buffer.data(); }
	constexpr Ulen length() const noexcept { return m_buffer.length(); }
private:
	Array<char> m_buffer;
	Bool        m_valid;
};

} // namespace Biron

#endif // BIRON_STRING_INL