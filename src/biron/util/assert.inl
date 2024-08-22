#ifndef BIRON_ASSERT_INL
#define BIRON_ASSERT_INL
#include <assert.h>

#define BIRON_ASSERT(...) assert(__VA_ARGS__) // ((void)(__VA_ARGS__))

#endif // BIRON_ASSERT_INL