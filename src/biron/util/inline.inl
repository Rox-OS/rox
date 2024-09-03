#ifndef BIRON_INLINE_INL
#define BIRON_INLINE_INL
#include <biron/util/types.inl>

#if !defined(BIRON_COMPILER_MSVC)
	#define BIRON_INLINE __attribute__((always_inline)) inline
#else
	#define BIRON_INLINE __forceinline
#endif

#endif // BIRON_INLINE_INL