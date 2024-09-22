#ifndef BIRON_ALLOCATOR_H
#define BIRON_ALLOCATOR_H
#include <biron/util/types.inl>
#include <biron/util/exchange.inl>

#include <atomic>

namespace Biron {

template<typename T>
using Atomic = std::atomic<T>;

struct System;

struct Allocator {
	virtual void* allocate(Ulen size) noexcept = 0;
	virtual void deallocate(void* old, Ulen size) noexcept = 0;

	template<typename T, typename... Ts>
	[[nodiscard]] T* make(Ts&&... args) noexcept {
		if (auto addr = allocate(sizeof(T))) {
			return new (addr, Nat{}) T{forward<Ts>(args)...};
		}
		return nullptr;
	}
};

struct SystemAllocator : Allocator {
	constexpr SystemAllocator(const System& system) noexcept
		: m_system{system}
		, m_peak_bytes{0}
		, m_current_bytes{0}
	{
	}
	virtual void* allocate(Ulen size) noexcept override;
	virtual void deallocate(void* old, Ulen size) noexcept override;
	[[nodiscard]] Ulen peak() const noexcept { return m_peak_bytes.load(); }
private:
	const System& m_system;
	Atomic<Uint64> m_peak_bytes;
	Atomic<Uint64> m_current_bytes;
};

template<Ulen E>
struct InlineAllocator : Allocator {
	static inline constexpr const Ulen ALIGN = 16;
	static inline constexpr const Ulen CAPACITY = ((E + ALIGN - 1) / ALIGN) * ALIGN;
	constexpr InlineAllocator() noexcept : m_nat{}, m_offset{0} {}
	constexpr InlineAllocator(const InlineAllocator&) noexcept = delete;
	constexpr InlineAllocator(InlineAllocator&&) noexcept = delete;
	virtual void* allocate(Ulen size) noexcept override {
		const Ulen bytes = ((size + ALIGN - 1) / ALIGN) * ALIGN;
		if (m_offset + bytes >= CAPACITY) {
			return nullptr;
		}
		void *const result = &m_data[m_offset];
		m_offset += bytes;
		return result;
	}
	virtual void deallocate(void* old, Ulen size) noexcept override {
		if (old && &m_data[m_offset] == old) {
			m_offset -= size;
		}
	}
	[[nodiscard]] Bool owns(const void* old, Ulen size) noexcept {
		if (!old) return false;
		const auto data = reinterpret_cast<const Uint8*>(old);
		return data >= m_data && data + size < &m_data[CAPACITY];
	}
	constexpr void clear() noexcept {
		m_offset = 0;
	}
private:
	union {
		Nat m_nat;
		alignas(16) Uint8 m_data[CAPACITY];
	};
	Ulen m_offset = 0;
};

struct ArenaAllocator : Allocator {
	static inline constexpr const Ulen ALIGN = 16;
	static inline constexpr const Ulen MIN_CHUNK_SIZE = 2 * 1024 * 1024; // 2 MiB
	constexpr ArenaAllocator(Allocator& allocator) noexcept
		: m_tail{nullptr}
		, m_allocator{allocator}
	{
	}
	constexpr ArenaAllocator(ArenaAllocator&& other) noexcept
		: m_tail{exchange(other.m_tail, nullptr)}
		, m_allocator{other.m_allocator}
	{
	}
	~ArenaAllocator() noexcept {
		clear();
	}
	virtual void* allocate(Ulen size) noexcept override;
	virtual void deallocate(void* old, Ulen size) noexcept override;
	void clear() noexcept;
private:
	struct alignas(ALIGN) Chunk {
		Ulen   capacity;
		Ulen   offset;
		Chunk* next;
		Chunk* prev;
		Uint8  data[];
	};
	Chunk* m_tail;
	Allocator& m_allocator;
};

struct ScratchAllocator : Allocator {
	static inline constexpr const Ulen INSITU = 16384; // 16 KiB
	constexpr ScratchAllocator(Allocator& allocator) noexcept
		: m_arena{allocator}
	{
	}
	virtual void* allocate(Ulen size) noexcept override;
	virtual void deallocate(void* old, Ulen size) noexcept override;
	void clear() noexcept;
private:
	InlineAllocator<INSITU> m_inline;
	ArenaAllocator m_arena;
};

} // namespace Biron

#endif // BIRON_ALLOCATOR_INL