#ifndef BIRON_MAYBE_INL
#define BIRON_MAYBE_INL
#include <biron/util/either.inl>

namespace Biron {

struct None {};

template<typename T>
struct Maybe {
	static constexpr Maybe none() { return None {}; }
	constexpr Maybe() noexcept = default;
	constexpr Maybe(None) noexcept : Maybe{} {}
	constexpr Maybe(T&& value) noexcept : m_either{move(value)} {}
	constexpr Maybe(Maybe&& other) noexcept : m_either{move(other.m_either)} {}
	constexpr Maybe(const T& other) noexcept
		requires CopyConstructible<T>
		: m_either{other}
	{
	}
	constexpr Maybe(const Maybe& other) noexcept
		requires CopyConstructible<T>
		: m_either{other.m_either}
	{
	}
	~Maybe() noexcept { drop(); }
	constexpr Maybe& operator=(T&& value) noexcept {
		return *new(drop(), Nat{}) Maybe{move(value)};
	}
	constexpr Maybe& operator=(Maybe&& other) noexcept {
		return *new(drop(), Nat{}) Maybe{move(other)};
	}
	constexpr Maybe& operator=(const T& value) noexcept
		requires CopyConstructible<T>
	{
		return *new(drop(), Nat{}) Maybe{value};
	}
	[[nodiscard]] constexpr T& some() noexcept { return m_either.lhs(); }
	[[nodiscard]] constexpr const T& some() const noexcept { return m_either.lhs(); }
	[[nodiscard]] constexpr T& operator*() noexcept { return some(); }
	[[nodiscard]] constexpr const T& operator*() const noexcept { return some(); }
	[[nodiscard]] constexpr T* operator->() noexcept { return &some(); }
	[[nodiscard]] constexpr const T* operator->() const noexcept { return &some(); }
	[[nodiscard]] constexpr auto is_some() const noexcept { return m_either.is_lhs(); }
	[[nodiscard]] constexpr auto is_none() const noexcept { return !is_some(); }
	[[nodiscard]] constexpr operator bool() const noexcept { return is_some(); }
	constexpr void reset() noexcept { drop(); }
	template<typename... Ts> constexpr T& emplace(Ts&&... args) noexcept {
		return m_either.emplace_lhs(forward<Ts>(args)...);
	}
	[[nodiscard]] constexpr Bool operator==(const Maybe& other) const noexcept {
		return other.m_either == m_either;
	}
private:
	constexpr Maybe* drop() noexcept { m_either.reset(); return this; }
	Either<T, Nat> m_either;
};

template<typename T>
concept MaybeCopyable = requires(const T& object) {
	{ object.copy() } -> Same<Maybe<T>>;
};

} // namespace Biron

#endif // BIRON_MAYBE_INL