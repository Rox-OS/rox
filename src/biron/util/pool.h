#ifndef BIRON_POOL_H
#define BIRON_POOL_H
#include <biron/util/maybe.inl>
#include <biron/util/array.inl>
#include <biron/util/traits/conditional.inl>

namespace Biron {

struct Allocator;

struct Pool {
	template<Bool Const>
	struct SelectIterator {
		using PoolType = Conditional<const Pool, Pool, Const>;
		using ItemType = Conditional<const void*, void*, Const>;
		constexpr SelectIterator() noexcept
			: m_pool{nullptr}, m_index{0}
		{
		}
		constexpr SelectIterator(PoolType& pool, Ulen index) noexcept
			: m_pool{&pool}, m_index{index}
		{
		}
		constexpr SelectIterator(const SelectIterator& other) noexcept = default;
		constexpr SelectIterator& operator=(const SelectIterator& other) noexcept = default;
		constexpr SelectIterator operator++() noexcept {
			do ++m_index; while (m_index < m_pool->m_object_count && !m_pool->test(m_index));
			return *this;
		}
		[[nodiscard]] constexpr Bool operator!=(const SelectIterator& other) const noexcept {
			return m_pool != other.m_pool || m_index != other.m_index;
		}
		[[nodiscard]] constexpr Bool operator==(const SelectIterator& other) const noexcept {
			return m_pool == other.m_pool && m_index == other.m_index;
		}
		[[nodiscard]] constexpr ItemType operator*() const noexcept {
			// NOTE(dweiler): There should be a cleaner way to express this without
			// checking each dereference since iterators are not dereferenced when the
			// end is reached but the way Cache::SelectIterator operator++ works makes
			// this really difficult to downright impossible to model. Revisit this.
			return m_pool ? m_pool->address(m_index) : nullptr;
		}
	private:
		PoolType* m_pool;
		Ulen      m_index;
	};
	using Iterator = SelectIterator<false>;
	using ConstIterator = SelectIterator<true>;

	Pool(Pool&& other) noexcept;
	constexpr Pool(const Pool&) noexcept = delete;
	~Pool() noexcept;

	constexpr Pool& operator=(const Pool&) noexcept = delete;
	constexpr Pool& operator=(Pool&&) noexcept = delete;

	static Maybe<Pool> make(Allocator& allocator, Ulen object_size, Ulen object_count) noexcept;

	[[nodiscard]] void* allocate() noexcept;
	[[nodiscard]] Bool deallocate(Uint8* addr) noexcept;

	[[nodiscard]] constexpr Iterator begin() noexcept { return { *this, 0 }; }
	[[nodiscard]] constexpr Iterator end() noexcept { return { *this, m_object_count }; }
	[[nodiscard]] constexpr ConstIterator begin() const noexcept { return { *this, 0 }; }
	[[nodiscard]] constexpr ConstIterator end() const noexcept { return { *this, m_object_count }; }

private:
	friend struct Cache;

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
		using PoolType = Conditional<const Array<Pool>, Array<Pool>, Const>;
		constexpr SelectIterator(PoolType& pools, Array<Pool>::SelectIterator<Const> pool) noexcept
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
					m_item = {};
				} else {
					m_item = m_pool->begin();
				}
			}
			return *this;
		}
		[[nodiscard]] constexpr Bool operator!=(const SelectIterator& other) const noexcept {
			return other.m_pool != m_pool || other.m_item != m_item;
		}
		[[nodiscard]] constexpr auto operator*() const noexcept {
			return *m_item;
		}
	private:
		PoolType*                          m_pools;
		Array<Pool>::SelectIterator<Const> m_pool;
		Pool::SelectIterator<Const>        m_item;
	};

	using Iterator      = SelectIterator<false>;
	using ConstIterator = SelectIterator<true>;

	constexpr Cache(Allocator& allocator, Ulen object_size, Ulen object_count) noexcept
		: m_pools{allocator}
		, m_object_size{object_size}
		, m_object_count{object_count}
		, m_length{0}
	{
	}

	Cache(Cache&&) noexcept = default;

	[[nodiscard]] void* allocate() noexcept;
	[[nodiscard]] Bool deallocate(void* addr) noexcept;
	[[nodiscard]] constexpr Allocator& allocator() const noexcept { return m_pools.allocator(); }

	[[nodiscard]] constexpr Ulen object_size() const noexcept { return m_object_size; }
	[[nodiscard]] constexpr Ulen object_capacity() const noexcept { return m_object_count; }

	[[nodiscard]] constexpr Iterator begin() noexcept { return { m_pools, m_pools.begin() }; }
	[[nodiscard]] constexpr Iterator end() noexcept { return { m_pools, m_pools.end() }; }
	[[nodiscard]] constexpr ConstIterator begin() const noexcept { return { m_pools, m_pools.begin() }; }
	[[nodiscard]] constexpr ConstIterator end() const noexcept { return { m_pools, m_pools.end() }; }

	[[nodiscard]] constexpr void* operator[](Ulen index) noexcept {
		return m_pools[index / m_object_count].address(index % m_object_count);
	}
	[[nodiscard]] constexpr const void* operator[](Ulen index) const noexcept {
		return m_pools[index / m_object_count].address(index % m_object_count);
	}

	[[nodiscard]] constexpr Ulen length() const noexcept {
		return m_length;
	}

	[[nodiscard]] constexpr Bool empty() const noexcept {
		return m_length == 0;
	}

	template<typename T, typename... Ts>
	[[nodiscard]] T* make(Ts&&... args) noexcept {
		if (auto addr = allocate()) {
			return new (addr, Nat{}) T{forward<Ts>(args)...};
		}
		return nullptr;
	}

private:
	Array<Pool> m_pools;
	Ulen        m_object_size;
	Ulen        m_object_count;
	Ulen        m_length;
};

} // namespace Biron

#endif // BIRON_POOL_H