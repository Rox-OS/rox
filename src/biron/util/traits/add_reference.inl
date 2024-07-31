#ifndef BIRON_ADD_REFERENCE_INL
#define BIRON_ADD_REFERENCE_INL
#include <biron/util/traits/is_referenceable.inl>

namespace Biron {

template<typename T, auto = is_referencable<T>>
struct _AddLValueReference {
	using Type = T;
};

template<typename T>
struct _AddLValueReference<T, true> {
	using Type = T&;
};

template<typename T, auto = is_referencable<T>>
struct _AddRValueReference {
	using Type = T;
};

template<typename T>
struct _AddRValueReference<T, true> {
	using Type = T&&;
};

template<typename T>
using AddLValueReference = typename _AddLValueReference<T>::Type;

template<typename T>
using AddRValueReference = typename _AddRValueReference<T>::Type;

} // namespace Biron

#endif // BIRON_ADD_REFERENCE_INL