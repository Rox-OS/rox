#ifndef BIRON_ALLOCATOR_INL
#define BIRON_ALLOCATOR_INL
#include <biron/util/types.inl>
#include <biron/util/exchange.inl>

#include <stdio.h>

namespace Biron {

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

struct TemporaryAllocator : Allocator {
	static inline constexpr const Ulen ALIGN = 16;
	static inline constexpr const Ulen MIN_CHUNK_SIZE = 2 * 1024 * 1024; // 2 MiB
	constexpr TemporaryAllocator(Allocator& allocator) noexcept
		: m_tail{nullptr}
		, m_allocator{allocator}
	{
	}
	TemporaryAllocator(TemporaryAllocator&& other) noexcept
		: m_tail{exchange(other.m_tail, nullptr)}
		, m_allocator{other.m_allocator}
	{
	}
	~TemporaryAllocator() noexcept {
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
		: m_temporary{allocator}
	{
	}
	virtual void* allocate(Ulen size) noexcept override;
	virtual void deallocate(void* old, Ulen size) noexcept override;
	void clear() noexcept;
private:
	InlineAllocator<INSITU> m_inline;
	TemporaryAllocator m_temporary;
};

} // namespace Biron

#endif // BIRON_ALLOCATOR_INL