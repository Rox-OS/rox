#ifndef BIRON_NEW_INL
#define BIRON_NEW_INL
#include <biron/util/inline.inl>
#include <biron/util/types.inl>

BIRON_INLINE void* operator new(Biron::Ulen, void* p, Biron::Nat) {
	return p;
}

#endif // BIRON_NEW_INL