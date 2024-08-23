#ifndef BIRON_EXCHANGE_INL
#define BIRON_EXCHANGE_INL
#include <biron/util/move.inl>
#include <biron/util/forward.inl>

namespace Biron {

template<typename T1, typename T2 = T1>
[[nodiscard]] BIRON_INLINE constexpr T1 exchange(T1& obj, T2&& new_value) noexcept {
	T1 old_value = move(obj);
	obj = forward<T2>(new_value);
	return old_value;
}

} // namespace Biron

#endif // BIRON_EXCHANGE_INL