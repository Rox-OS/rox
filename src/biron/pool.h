#ifndef BIRON_POOL_H
#define BIRON_POOL_H
#include <biron/util/maybe.inl>
#include <biron/util/array.inl>
#include <biron/util/traits/is_same.inl>
#include <biron/util/traits/conditional.inl>

#include <stdio.h>

namespace Biron {

struct Allocator;

struct Pool {
	template<Bool Const>
	struct SelectIterator {
		using Type = Conditional<const Pool, Pool, Const>;
		constexpr SelectIterator() noexcept
			: m_pool{nullptr}, m_index{0}
		{
		}
		constexpr SelectIterator(Type& pool, Ulen index) noexcept
			: m_pool{&pool}, m_index{index}
		{
		}
		constexpr SelectIterator(const SelectIterator& other) noexcept = default;
		constexpr SelectIterator& operator=(const SelectIterator& other) noexcept = default;
		constexpr SelectIterator operator++() noexcept {
			do ++m_index; while (m_index < m_pool->m_object_count && !m_pool->test(m_index));
			return *this;
		}
		[[nodiscard]] constexpr Bool operator!=(const SelectIterator& other) const noexcept = default;
		[[nodiscard]] constexpr Bool operator==(const SelectIterator& other) const noexcept = default;
		[[nodiscard]] constexpr void* operator*() const noexcept {
			return m_pool->address(m_index);
		}
		void clear() noexcept {
			m_pool  = nullptr;
			m_index = 0;
		}
	private:
		Type* m_pool;
		Ulen  m_index;
	};
	using Iterator = SelectIterator<false>;
	using ConstIterator = SelectIterator<true>;

	Pool(Pool&& other) noexcept;
	~Pool() noexcept;

	static Maybe<Pool> make(Allocator& allocator, Ulen object_size, Ulen object_count) noexcept;

	[[nodiscard]] void* allocate() noexcept;
	[[nodiscard]] Bool deallocate(Uint8* addr) noexcept;

	[[nodiscard]] constexpr Iterator begin() noexcept { return { *this, 0 }; }
	[[nodiscard]] constexpr Iterator end() noexcept { return { *this, m_object_count }; }
	[[nodiscard]] constexpr ConstIterator begin() const noexcept { return { *this, 0 }; }
	[[nodiscard]] constexpr ConstIterator end() const noexcept { return { *this, m_object_count }; }

private:
	constexpr Pool(Allocator& allocator, Ulen object_size, Ulen object_count, Uint32* occupied, Uint8* storage) noexcept
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

	[[nodiscard]] constexpr Bool test(Ulen index) const noexcept {
		return m_occupied[index / 32] & (Ulen(1) << (index % 32));
	}

	[[nodiscard]] constexpr void* address(Ulen i) noexcept {
		return m_storage + m_object_size * i;
	}
	[[nodiscard]] constexpr const void* address(Ulen i) const noexcept {
		return m_storage + m_object_size * i;
	}

	Allocator& m_allocator;
	Ulen       m_object_size;
	Ulen       m_object_count;
	Uint32*    m_occupied;
	Uint8*     m_storage;
};

struct Cache {
	template<Bool Const>
	struct SelectIterator {
		using Type = Conditional<const Array<Pool>, Array<Pool>, Const>;
		constexpr SelectIterator(Type& pools, Array<Pool>::SelectIterator<Const> pool) noexcept
			: m_pools{&pools}
			, m_pool{pool}
			, m_item{}
		{
			if (pool != m_pools->end()) {
				m_item = m_pool->begin();
			}
		}
		constexpr SelectIterator operator++() noexcept {
			if (++m_item == m_pool->end()) {
				if (++m_pool == m_pools->end()) {
					m_item.clear();
				} else {
					m_item = m_pool->begin();
				}
			}
			return *this;
		}
		[[nodiscard]] constexpr Bool operator!=(const SelectIterator& other) const noexcept {
			return other.m_pool != m_pool || other.m_item != m_item;
		}
		[[nodiscard]] constexpr void* operator*() const noexcept {
			return *m_item;
		}
	private:
		Type*                              m_pools;
		Array<Pool>::SelectIterator<Const> m_pool;
		Pool::SelectIterator<Const>        m_item;
	};

	using Iterator      = SelectIterator<false>;
	using ConstIterator = SelectIterator<true>;

	constexpr Cache(Allocator& allocator, Ulen object_size, Ulen object_count) noexcept
		: m_pools{allocator}
		, m_object_size{object_size}
		, m_object_count{object_count}
	{
	}
	[[nodiscard]] void* allocate() noexcept;
	[[nodiscard]] Bool deallocate(void* addr) noexcept;
	[[nodiscard]] constexpr Allocator& allocator() const noexcept { return m_pools.allocator(); }

	[[nodiscard]] constexpr Ulen object_size() const noexcept { return m_object_size; }
	[[nodiscard]] constexpr Ulen object_capacity() const noexcept { return m_object_count; }

	[[nodiscard]] constexpr Iterator begin() noexcept { return { m_pools, m_pools.begin() }; }
	[[nodiscard]] constexpr Iterator end() noexcept { return { m_pools, m_pools.end() }; }
	[[nodiscard]] constexpr ConstIterator begin() const noexcept { return { m_pools, m_pools.begin() }; }
	[[nodiscard]] constexpr ConstIterator end() const noexcept { return { m_pools, m_pools.end() }; }

private:
	Array<Pool> m_pools;
	Ulen        m_object_size;
	Ulen        m_object_count;
};

} // namespace Biron

#endif // BIRON_POOL_H