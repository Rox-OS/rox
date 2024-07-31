#ifndef BIRON_IS_REFERENCE_INL
#define BIRON_IS_REFERENCE_INL

namespace Biron {

template<typename T>
struct _IsLValueReference {
	static inline constexpr auto value = false;
};

template<typename T>
struct _IsLValueReference<T&> {
	static inline constexpr auto value = true;
};

template<typename T>
struct _IsRValueReference {
	static inline constexpr auto value = false;
};

template<typename T>
struct _IsRValueReference<T&&> {
	static inline constexpr auto value = true; 
};

template<typename T>
inline constexpr auto is_lvalue_reference = _IsLValueReference<T>::value;

template<typename T>
inline constexpr auto is_rvalue_reference = _IsRValueReference<T>::value;

} // namespace Biron

#endif // BIRON_IS_REFERENCE_INL