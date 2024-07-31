# Biron

Biron is an experimental toy systems programming language built for Rox.

## Features
* Rich inline assembly support for amd64
* Parametric polymorphsim
* Type inference
* Few keywords: `fn asm if let else for as`
* Pointers: `*T`, Tuples: `(T1, T2, ...)`, Arrays: `[N]T`, and Slices: `[]T`
* Non-null-terminated UTF-8 string type: `String`
* Sized integer types: `(S|U)int{8,16,32,64}`
* Memory addressing type: `Address`
* Designed to run on baremetal
  * No floating point
  * No integer divisions
* Small: ~5k lines of freestanding C++ with no build dependencies
  * Loads libLLVM dynamically at runtime.