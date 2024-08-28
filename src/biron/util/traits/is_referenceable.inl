#ifndef BIRON_IS_REFERENCEABLE_INL
#define BIRON_IS_REFERENCEABLE_INL
#include <biron/util/traits/is_same.inl>

namespace Biron {

#if __has_builtin(__is_referencable)
template<typename T>
inline constexpr auto is_referencable = __is_referenceable(T);
#else
struct _Referencable {
	struct Fail {};
	template<typename T> static T& test(int) noexcept;
	template<typename T> static Fail test(...) noexcept;
};
template<typename T>
inline constexpr auto is_referencable = 
	!is_same<decltype(_Referencable::test<T>(0)), _Referencable::Fail>;
#endif // !__has_builtin(__is_referencable)

template<typename T>
concept Referencable = is_referencable<T>;

} // namespace Biron

#endif // BIRON_IS_REFERENCEABLE_INL