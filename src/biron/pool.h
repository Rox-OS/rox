#ifndef BIRON_POOL_H
#define BIRON_POOL_H
#include <biron/util/maybe.inl>
#include <biron/util/allocator.inl>
#include <biron/util/array.inl>

#include <string.h>

namespace Biron {

struct Allocator;

struct Pool {
	Pool(Pool&& other) noexcept
		: m_allocator{other.m_allocator}
		, m_object_size{exchange(other.m_object_size, 0)}
		, m_object_count{exchange(other.m_object_count, 0)}
		, m_occupied{exchange(other.m_occupied, nullptr)}
		, m_storage{exchange(other.m_storage, nullptr)}
	{
	}

	~Pool() {
		m_allocator.deallocate(m_storage, m_object_size * m_object_count);
		m_allocator.deallocate(m_occupied, m_object_count * 4);
	}

	static Maybe<Pool> create(Allocator& allocator, Ulen object_size, Ulen object_count) noexcept {
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

	[[nodiscard]] void* allocate() noexcept {
		for (Ulen i = 0; i < m_object_count; i++) {
			if (test(i)) {
				continue;
			}
			mark(i);
			return m_storage + m_object_size * i;
		}
		return nullptr;
	}

	[[nodiscard]] Bool deallocate(Uint8* addr) noexcept {
		if (addr < m_storage ||
		    addr > m_storage + m_object_count * m_object_size)
		{
			return false;
		}
		clear((addr - m_storage) / m_object_size);
		return true;
	}

private:
	Pool(Allocator& allocator, Ulen object_size, Ulen object_count, Uint32* occupied, Uint8* storage) noexcept
		: m_allocator{allocator}
		, m_object_size{object_size}
		, m_object_count{object_count}
		, m_occupied{occupied}
		, m_storage{storage}
	{
	}

	void mark(Ulen index) noexcept {
		m_occupied[index / 32] |= (Ulen(1) << (index % 32));
	}

	void clear(Ulen index) noexcept {
		m_occupied[index / 32] &= ~(Ulen(1) << (index % 32));
	}

	[[nodiscard]] Bool test(Ulen index) const noexcept {
		return m_occupied[index / 32] & (Ulen(1) << (index % 32));
	}

	Allocator& m_allocator;
	Ulen       m_object_size;
	Ulen       m_object_count;
	Uint32*    m_occupied;
	Uint8*     m_storage;
};

struct Cache {
	constexpr Cache(Allocator& allocator, Ulen object_size, Ulen object_count) noexcept
		: m_pools{allocator}
		, m_object_size{object_size}
		, m_object_count{object_count}
	{
	}

	[[nodiscard]] Ulen object_size() const noexcept { return m_object_size; }
	[[nodiscard]] Ulen object_capacity() const noexcept { return m_object_count; }

	void* allocate() noexcept {
		for (Ulen l = m_pools.length(), i = 0; i < l; i++) {
			if (auto addr = m_pools[i].allocate()) {
				return addr;
			}
		}
		auto pool = Pool::create(m_pools.allocator(),
		                         m_object_size,
		                         m_object_count);
		if (!pool || !m_pools.push_back(move(*pool))) {
			return nullptr;
		}
		return allocate();
	}

	void deallocate(void* addr) noexcept {
		for (Ulen l = m_pools.length(), i = 0; i < l; i++) {
			if (m_pools[i].deallocate(static_cast<Uint8*>(addr))) {
				return;
			}
		}
	}

private:
	Array<Pool> m_pools;
	Ulen        m_object_size;
	Ulen        m_object_count;
};

} // namespace Biron

#endif // BIRON_POOL_H