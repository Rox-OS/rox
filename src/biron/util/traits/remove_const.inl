#ifndef BIRON_REMOVE_CONST
#define BIRON_REMOVE_CONST

namespace Biron {

template<typename T> struct _RemoveConst {
	using Type = T;
};

template<typename T> struct _RemoveConst<const T> {
	using Type = T;
};

template<typename T>
using RemoveConst = typename _RemoveConst<T>::Type;

} // namespace Biron

#endif // BIRON_REMOVE_CONST