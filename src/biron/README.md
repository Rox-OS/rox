# Biron

Biron is an experimental toy systems programming language built for Rox.

## Features
* Type inference
* Effect system
* Few keywords: `fn if as of let for asm else defer union return struct`
* Rich concrete types:
  * Sized integer types: `(S|U)int{8,16,32,64}`
  * Sized boolean types: `Bool{8,16,32,64}`
  * Non-NUL-terminated UTF-8 string: `String`
  * Bit type: `Bit`. A special single-bit type. On its own is backed by a
    special unnamed `BitWord` type that is `Uint32`. When used with an array as
    in: `[N]Bit` the storage behaves like a true bit array: `[N/size of BitWord]BitWord`
    with bit indexing and addressing.
      * A bit pointer: `*Bit` is a regular pointer but with bit addressing. This
        is made possible by the fact that the base pointer must point to a
        `BitWord` (which has 4 byte alignment).
  * Bit: `Bit` actually a single bit (`[N]Bit` for array of bits with bit indexing)
    * Can take the address of a bit with a `Bit` pointer `*Bit` same size as as
      a regular pointer.
        - A `Bit` on its own (not as part of an array) is actually backed by a
          
          thus `[N]Bit` is really `[N/64]BitWord`
        - For 32-bit pointers the `Bit` uses a 64-bit word with 16--byte alignment,
          which means the address to the word with  except 58-bits of the pointer store the address to the
      word containing the bit, the remainder 6-bits encode the bit index within
      the 64-bit word
  * Pointer: `*T`
  * Slices: `[]T`
  * Arrays: `[N]T`, and enumerated`[Enum]T`
  * Tuples: `(T1, ... Tn)`
  * Structures: `struct { x: T; ... }`
  * Unions: `union { x: T; ... }`
  * Enums: `enum { A, B, C }`

  Optional slice and array bounds checking with `::`, as in
  `[_]T`
  `[_N]T` will check for underflow

* Sized integer types: 
* Memory addressing type: `Address`
* Designed to run on baremetal
  * No floating point
  * No integer divisions
* Small: ~7k lines of freestanding C++ with no build dependencies
  * Loads libLLVM dynamically at runtime.
  * Does not require C++ standard library or C++ runtime support library.