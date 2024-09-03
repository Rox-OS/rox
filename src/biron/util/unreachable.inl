#ifndef BIRON_UNREACHABLE_INL
#define BIRON_UNREACHABLE_INL
#include <biron/util/types.inl>

#if !defined(BIRON_COMPILER_MSVC)
	#define BIRON_UNREACHABLE() __builtin_unreachable()
#else
	#define BIRON_UNREACHABLE() __assume(0)
#endif

#endif // BIRON_UNREACHABLE_INL