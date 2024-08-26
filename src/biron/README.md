# Biron

Biron is an experimental toy systems programming language built for Rox.

## Features
* Type inference
* Few keywords: `fn if as let for asm else defer union return struct`
* Rich concrete types:
  * Sized integer types: `(S|U)int{8,16,32,64}`
  * Sized boolean types: `Bool{8,16,32,64}`
  * Memory addressing type: `Address`
    * Similar to `uintptr_t` but implicitly converts to and from `*T`
  * Pointers: `*T`, `[*]T`
  * Slices: `[]T`
  * Arrays: `[N]T`
  * Tuples: `(T1, ... Tn)`
  * Non-NUL-terminated UTF-8 string: `String`
  * Structures: `struct { x: T; ... }`
  * Unions: `union { x: T; ... }`
  * Variants: `variant { T1, T2 }`
* Designed to run on baremetal
  * No floating point
  * No integer divisions
* Small: ~8.5k lines of freestanding C++ with no build dependencies
  * Loads `libLLVM` dynamically at runtime if present.
    * Requires LLVM-17 or LLVM-18.
  * Does not require C++ standard library or C++ runtime support library.