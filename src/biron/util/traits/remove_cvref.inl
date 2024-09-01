#ifndef BIRON_REMOVE_CVREF_INL
#define BIRON_REMOVE_CVREF_INL
#include <biron/util/traits/remove_const.inl>
#include <biron/util/traits/remove_reference.inl>

namespace Biron {

// We do not remove 'volatile' since Biron does not make use of 'volatile'
// anywhere. We retain the name 'RemoveCVRef' though since it's expected.
template<typename T>
using RemoveCVRef = RemoveConst<RemoveReference<T>>;

} // namespace Biron

#endif // BIRON_REMOVE_CVREF_INL