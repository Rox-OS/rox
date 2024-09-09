#ifndef BIRON_IS_REFERENCEABLE_INL
#define BIRON_IS_REFERENCEABLE_INL

namespace Biron {

template<typename T>
struct Identity { using Type = T; };

template<typename T>
concept Referencable = requires {
	typename Identity<T&>;
};

} // namespace Biron

#endif // BIRON_IS_REFERENCEABLE_INL