#ifndef BIRON_INT128_H
#define BIRON_INT128_H
#include <biron/util/types.inl>

namespace Biron {

#if defined(BIRON_COMPILER_MSVC)
	using Sint128 = __int64;
	using Uint128 = unsigned __int64;
	using Bool128 = Boolean<Uint128>;
#else
	using Sint128 = __int128;
	using Uint128 = unsigned __int128;
	using Bool128 = Boolean<Uint128>;
#endif

} // namespace Biron

#endif // BIRON_INT128_H