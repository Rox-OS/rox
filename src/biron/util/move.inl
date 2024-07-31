#ifndef BIRON_MOVE_INL
#define BIRON_MOVE_INL
#include <biron/util/traits/remove_reference.inl>

#include <biron/util/inline.inl>

namespace Biron {

template<typename T>
[[nodiscard]] BIRON_INLINE constexpr RemoveReference<T>&&
move(T&& t) noexcept {
	return static_cast<RemoveReference<T>&&>(t);
}

} // namespace Biron

#endif // BIRON_MOVE_INL