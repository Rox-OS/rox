#ifndef BIRON_STRING_INL
#define BIRON_STRING_INL
#include <biron/util/types.inl>
#include <biron/util/exchange.inl>
#include <biron/util/array.inl>
#include <biron/util/maybe.inl>

namespace Biron {

struct Allocator;

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
	constexpr StringView slice(Ulen offset, Ulen length) noexcept {
		return {m_data + offset, length};
	}
	auto data() const noexcept { return m_data; }
	auto length() const noexcept { return m_length; }
	char operator[](Ulen i) const noexcept { return m_data[i]; }
	friend Bool operator==(StringView lhs, StringView rhs) noexcept;
	friend Bool operator!=(StringView lhs, StringView rhs) noexcept;
	Bool starts_with(StringView other) const noexcept;
	Maybe<Ulen> find_first_of(int ch) const noexcept;
	char *terminated(Allocator& allocator) const noexcept;
private:
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
	Bool append(Sint32 value) noexcept;
	Bool append(Ulen value) noexcept {
		return append(static_cast<Uint32>(value));
	}
	Bool append(Uint32 value) noexcept;
	Bool append(StringView view) noexcept;
	Bool repeat(int ch, Ulen count) noexcept;
	Bool repeat(StringView view, Ulen count) noexcept;
	constexpr Bool valid() const noexcept { return m_valid; }
	constexpr StringView view() const noexcept {
		return { m_buffer.data(), m_buffer.length() };
	}
	constexpr char* data() noexcept { return m_buffer.data(); }
	constexpr const char *data() const noexcept { return m_buffer.data(); }
private:
	Array<char> m_buffer;
	Bool        m_valid;
};

} // namespace Biron

#endif // BIRON_STRING_INL