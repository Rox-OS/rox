#ifndef BIRON_ERROR_INL
#define BIRON_ERROR_INL
#include <biron/util/maybe.inl>
namespace Biron {

struct Error {
  constexpr Error() noexcept = default;
  constexpr Error(decltype(nullptr)) noexcept {}
  constexpr Error(None) noexcept {}
  operator decltype(nullptr)() const noexcept { return nullptr; }
  operator Bool() const noexcept { return false; }
  operator None() const noexcept { return None{}; }
  template<typename T>
  operator Maybe<T>() const noexcept { return None{}; }
};

} // namespace Biron

#endif // BIRON_ERROR_INL