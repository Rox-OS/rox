#ifndef BIRON_MAYBE_INL
#define BIRON_MAYBE_INL
#include <biron/util/either.inl>
#include <biron/util/traits/is_same.inl>
namespace Biron {

template<typename T>
struct Maybe;

struct None {};

template<typename T>
concept MaybeCopyable = requires(const T& object) {
	{ object.copy() } -> Same<Maybe<T>>;
};

template<typename T>
struct Maybe {
	static constexpr Maybe none() noexcept {
		return None {};
	}
	constexpr Maybe() noexcept : m_either{Nat{}} {}
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
		return *new (drop(), Nat{}) Maybe{move(value)};
	}
	constexpr Maybe& operator=(Maybe&& other) noexcept {
		return *new (drop(), Nat{}) Maybe{move(other)};
	}
	constexpr Maybe& operator=(const T& value) noexcept
		requires CopyConstructible<T>
	{
		return *new (drop(), Nat{}) Maybe{value};
	}
	constexpr Maybe& operator=(const Maybe& other) noexcept
		requires CopyConstructible<T>
	{
		return *new (drop(), Nat{}) Maybe{other};
	}
	[[nodiscard]] constexpr T& some() noexcept { return m_either.lhs(); }
	[[nodiscard]] constexpr const T& some() const noexcept { return m_either.lhs(); }
	[[nodiscard]] constexpr T& operator*() noexcept { return some(); }
	[[nodiscard]] constexpr const T& operator*() const noexcept { return some(); }
	[[nodiscard]] constexpr T* operator->() noexcept { return &some(); }
	[[nodiscard]] constexpr const T* operator->() const noexcept { return &some(); }
	[[nodiscard]] constexpr auto is_some() const noexcept { return m_either.is_lhs(); }
	[[nodiscard]] constexpr auto is_none() const noexcept { return !is_some(); }
	[[nodiscard]] constexpr explicit operator bool() const noexcept { return is_some(); }
	constexpr void reset() noexcept { drop(); }
	constexpr void reset(T&& value) noexcept { new (drop(), Nat{}) Maybe{move(value)}; }
	constexpr void reset(Maybe&& other) noexcept { new (drop(), Nat{}) Maybe{move(other)}; }
	constexpr void reset(const T& value) noexcept
		requires CopyConstructible<T> { new (drop(), Nat{}) Maybe{value}; }
	constexpr void reset(const Maybe& other) noexcept
		requires CopyConstructible<T> { new (drop(), Nat{}) Maybe{other}; }
	template<typename... Ts>
	constexpr T& emplace(Ts&&... args) noexcept {
		return m_either.emplace_lhs(forward<Ts>(args)...);
	}
	[[nodiscard]] constexpr Bool operator==(const Maybe& other) const noexcept {
		return other.m_either == m_either;
	}
	Maybe copy() const noexcept
		requires CopyConstructible<T> || MaybeCopyable<T>
	{
		if (is_none()) {
			return None{};
		}
		if constexpr (CopyConstructible<T>) {
			return Maybe { *this };
		} else if constexpr (MaybeCopyable<T>) {
			auto result = some().copy();
			if (!result) {
				return None{};
			}
			return Maybe { move(*result) };
		}
		return None{};
	}
private:
	constexpr Maybe* drop() noexcept { m_either.reset(); return this; }
	Either<T, Nat> m_either;
};

} // namespace Biron

#endif // BIRON_MAYBE_INL