#include <string.h> // memset

#include <biron/util/pool.h>

namespace Biron {

Pool::Pool(Pool&& other) noexcept
	: m_allocator{other.m_allocator}
	, m_object_size{exchange(other.m_object_size, 0)}
	, m_object_count{exchange(other.m_object_count, 0)}
	, m_occupied{exchange(other.m_occupied, nullptr)}
	, m_storage{exchange(other.m_storage, nullptr)}
{
}

Pool::~Pool() noexcept {
	m_allocator.deallocate(m_storage, m_object_size * m_object_count);
	m_allocator.deallocate(m_occupied, m_object_count * 4);
}

Maybe<Pool> Pool::make(Allocator& allocator, Ulen object_size, Ulen object_count) noexcept {
	object_count = (object_count + 32 - 1) & -32;

	const auto objs_bytes = object_count * object_size;
	const auto objs_data = allocator.allocate(objs_bytes);
	if (!objs_data) {
		return None{};
	}

	const auto bits = object_count / 32;
	const auto bits_bytes = bits * 4;
	const auto bits_data = allocator.allocate(bits_bytes);
	if (!bits_data) {
		allocator.deallocate(objs_data, objs_bytes);
		return None{};
	}

	memset(bits_data, 0, bits_bytes);

	return Pool {
		allocator,
		object_size,
		object_count,
		static_cast<Uint32*>(bits_data),
		static_cast<Uint8*>(objs_data),
	};
}

void* Pool::allocate() noexcept {
	for (Ulen i = 0; i < m_object_count; i++) {
		if (test(i)) {
			continue;
		}
		mark(i);
		return address(i);
	}
	return nullptr;
}

Bool Pool::deallocate(Uint8* addr) noexcept {
	if (addr < m_storage ||
			addr > m_storage + m_object_count * m_object_size)
	{
		return false;
	}
	clear((addr - m_storage) / m_object_size);
	return true;
}

void* Cache::allocate() noexcept {
	for (auto& pool : m_pools) {
		if (auto addr = pool.allocate()) {
			return addr;
		}
	}
	auto pool = Pool::make(m_pools.allocator(),
	                       m_object_size,
	                       m_object_count);
	if (!pool || !m_pools.push_back(move(*pool))) {
		return nullptr;
	}
	return allocate();
}

Bool Cache::deallocate(void* addr) noexcept {
	for (auto& pool : m_pools) {
		if (pool.deallocate(static_cast<Uint8*>(addr))) {
			return true;
		}
	}
	return false;
}

} // namespace Biron