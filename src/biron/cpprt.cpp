#include <stdlib.h> // abort
#include <stdio.h> // fprintf, stderr

#include <biron/util/types.inl>

// Cannot disable this C++ runtime when ASAN is used as ASAN depends on it.
#if !__has_feature(address_sanitizer) && !defined(__AFL_COMPILER)

// These need to be in namespace std because the compiler generates mangled
// names assuming so.
namespace std {
	using size_t = Biron::Ulen;
	enum class align_val_t : size_t {};
	struct nothrow_t {
		explicit nothrow_t() = default;
	};
} // namespace std

extern "C" {
	void __cxa_pure_virtual() {
		fprintf(stderr, "Pure virtual function call\n");
		abort();
	}

	// C++ allows concurrent initialize of static constants. We do not rely on this
	// behavior so just stub them out
	using namespace Biron;

	struct Guard {
		Uint8 done;
		Uint8 pending;
		Uint8 padding[62];
	};
	static_assert(sizeof(Guard) == 64);

	int __cxa_guard_acquire(Guard* guard) noexcept {
		if (guard->done) {
			return 0;
		}
		if (guard->pending) {
			fprintf(stderr, "Recursive initialization of statics are not supported");
			abort();
		}
		guard->pending = 1;
		return 1;
	}

	void __cxa_guard_release(Guard* guard) noexcept {
		guard->done = 1;
	}
} // extern "C"

// We bring them into the global namespace though since that is where the
// new and delete operators must be defined.
using std::size_t;
using std::align_val_t;
using std::nothrow_t;

// Simple macros to help stamp out disabled new and delete operators.
#define NEW_TEMPLATE(...) \
	void* operator __VA_ARGS__ { \
		fprintf(stderr, "operator " #__VA_ARGS__ " is disabled\n"); \
		abort(); \
	}

#define DELETE_TEMPLATE(...) \
	void operator __VA_ARGS__ { \
		fprintf(stderr, "operator " #__VA_ARGS__ " is disabled\n"); \
		abort(); \
	}

// Which are then called twice since there is array and non-array variants.
#define NEW(...) \
	NEW_TEMPLATE(new(__VA_ARGS__)) \
	NEW_TEMPLATE(new[](__VA_ARGS__))

#define DELETE(...) \
	DELETE_TEMPLATE(delete(__VA_ARGS__)) \
	DELETE_TEMPLATE(delete[](__VA_ARGS__))

// Replaceable allocation functions.
NEW(size_t)
NEW(size_t, align_val_t)

// Non-allocating placement allocation functions.
NEW(size_t, void*);

// Replacable non-throwing allocation functions.
NEW(size_t, const nothrow_t&)
NEW(size_t, align_val_t, const nothrow_t&)

// Replacable usual deallocation functions.
DELETE(void*, size_t);

// Need to add noexcept specification on these four in particular
DELETE_TEMPLATE(delete(void*) noexcept);
DELETE_TEMPLATE(delete[](void*) noexcept);
DELETE_TEMPLATE(delete(void*, align_val_t) noexcept);
DELETE_TEMPLATE(delete[](void*, align_val_t) noexcept);

DELETE(void*, size_t, align_val_t)
DELETE(void*, const nothrow_t&);
DELETE(void*, align_val_t, const nothrow_t&)

#endif // !__has_feature(address_sanitizer)