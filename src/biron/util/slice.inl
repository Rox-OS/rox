#ifndef BIRON_SLICE_INL
#define BIRON_SLICE_INL
#include <biron/util/types.inl>

namespace Biron {

template<typename T>
struct Slice {
	constexpr Slice() noexcept : m_data{nullptr}, m_length{0} {}
	constexpr Slice(T *data, Ulen length) noexcept : m_data{data}, m_length{length} {}
	template<Ulen E>
	constexpr Slice(T (&data)[E]) noexcept : m_data{data}, m_length{E} {}
	constexpr T& operator[](Ulen n) const noexcept { return m_data[n]; }
private:
	T*   m_data;
	Ulen m_length;
};

} // namespace Biron

#endif // BIRON_SLICE_INL