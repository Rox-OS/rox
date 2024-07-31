#ifndef BIRON_ALLOCATOR_INL
#define BIRON_ALLOCATOR_INL
#include <biron/util/types.inl>

namespace Biron {

struct Allocator {
	virtual void* allocate(Ulen size) noexcept = 0;
	virtual void deallocate(void* old, Ulen size) noexcept = 0;
	virtual void* scratch(Ulen size) noexcept = 0;
};

} // namespace Biron

#endif // BIRON_ALLOCATOR_INL