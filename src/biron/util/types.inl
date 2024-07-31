#ifndef BIRON_TYPES_INL
#define BIRON_TYPES_INL

namespace Biron {

#if !defined(__has_builtin)
#define __has_builtin(...)
#endif // !defined(__has_builtin)

using Sint8 = signed char;
using Uint8 = unsigned char;
using Sint16 = signed short;
using Uint16 = unsigned short;
using Sint32 = signed int;
using Uint32 = unsigned int;
using Sint64 = signed long long;
using Uint64 = unsigned long long;
using Ulen = decltype(sizeof 0);
using Bool = bool;
typedef struct{} Nat;

} // namespace Biron;

#endif // TYPES_H