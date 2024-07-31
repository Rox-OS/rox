#ifndef BIRON_UTIL_IS_BASE_OF_INL
#define BIRON_UTIL_IS_BASE_OF_INL

namespace Biron {

template<typename B, typename D>
inline constexpr auto is_base_of = __is_base_of(B, D);

template<typename D, typename B>
concept DerivedFrom = is_base_of<B, D>;

} // namespace Biron

#endif // BIRON_UTIL_IS_BASE_OF_INL