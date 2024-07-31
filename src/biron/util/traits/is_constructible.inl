#ifndef BIRON_IS_CONSTRUCTIBLE_INL
#define BIRON_IS_CONSTRUCTIBLE_INL
#include <biron/util/traits/add_reference.inl>

namespace Biron {

template<typename T>
inline constexpr auto is_copy_constructible = 
	__is_constructible(T, AddLValueReference<const T>);

template<typename T>
inline constexpr auto is_move_constructible =
	__is_constructible(T, AddRValueReference<T>);

template<typename T>
concept CopyConstructible = is_copy_constructible<T>;

template<typename T>
concept MoveConstructible = is_move_constructible<T>;

} // namespace Biron

#endif // BIRON_IS_CONSTRUCTIBLE_INL