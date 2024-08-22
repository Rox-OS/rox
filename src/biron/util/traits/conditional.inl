#ifndef BIRON_CONDITION_INL
#define BIRON_CONDITION_INL
#include <biron/util/types.inl>

namespace Biron {

template<typename T, typename F, Bool = false>
struct _Conditional {
	using Type = F;
};

template<typename T, typename F>
struct _Conditional<T, F, true> {
	using Type = T;
};

template<typename T, typename F, Bool B>
using Conditional = typename _Conditional<T, F, B>::Type;

} // namespace Biron

#endif // BIRON_CONDITION_INL