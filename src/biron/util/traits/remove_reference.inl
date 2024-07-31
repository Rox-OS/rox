#ifndef BIRON_REMOVE_REFERENCE_INL
#define BIRON_REMOVE_REFERENCE_INL

namespace Biron {

template<typename T> struct _RemoveReference {
	using Type = T;
};

template<typename T> struct _RemoveReference<T&> {
	using Type = T;
};

template<typename T> struct _RemoveReference<T&&> {
	using Type = T;
};

template<typename T> using RemoveReference = typename _RemoveReference<T>::Type;

} // namespace Biron

#endif // BIRON_REMOVE_REFERENCE_INL