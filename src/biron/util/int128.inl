#ifndef BIRON_INT128_H
#define BIRON_INT128_H
#include <biron/util/types.inl>

namespace Biron {

using Sint128 = __int128;
using Uint128 = unsigned __int128;
using Bool128 = Boolean<Uint128>;

} // namespace Biron

#endif // BIRON_INT128_H