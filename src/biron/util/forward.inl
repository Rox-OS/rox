#ifndef BIRON_FORWARD_INL
#define BIRON_FORWARD_INL
#include <biron/util/traits/remove_reference.inl>
#include <biron/util/traits/is_reference.inl>

#include <biron/util/inline.inl>

namespace Biron {

template<typename T>
[[nodiscard]] BIRON_INLINE constexpr T&&
forward(RemoveReference<T>& t) noexcept {
	return static_cast<T&&>(t);
}

template<typename T>
[[nodiscard]] BIRON_INLINE constexpr T&&
forward(RemoveReference<T>&& t) noexcept {
	static_assert(!is_lvalue_reference<T>, "Cannot forward an rvalue as an lvalue");
	return static_cast<T&&>(t);
}

} // namespace Biron

#endif // BIRON_FORWARD_INL