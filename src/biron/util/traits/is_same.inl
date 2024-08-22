#ifndef BIRON_IS_SAME_INL
#define BIRON_IS_SAME_INL

namespace Biron {

template<typename T1, typename T2>
struct _IsSame {
	static inline constexpr auto value = false;
};

template<typename T>
struct _IsSame<T, T> {
	static inline constexpr auto value = true;
};

template<typename T1, typename T2>
inline constexpr auto is_same = _IsSame<T1, T2>::value;

template<typename T1, typename T2>
concept Same = is_same<T1, T2>;

} // namespace Biron

#endif // BIRON_IS_SAME_INL