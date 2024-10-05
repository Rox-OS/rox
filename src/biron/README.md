# Biron

Biron is an experimental toy [systems programming language](https://en.wikipedia.org/wiki/System_programming_language) built for Rox.

## Features
* [Procedrual programming](https://en.wikipedia.org/wiki/Procedural_programming)
  * Named and anonymous functions
* [Absence-based Object Oriented programming](https://en.wikipedia.org/wiki/Object-oriented_programming#Absence)
  * Functions have an optional reciever argument list
    * Completely free-form.
    * Similar to Go's [Method sets](https://go.dev/wiki/MethodSets)
    * Alows for [Mixins](https://en.wikipedia.org/wiki/Mixin)
* [Structured programming](https://en.wikipedia.org/wiki/Structured_programming)
  * `if` `else` `for` `break` `continue` `defer` `return` `yield`
* [Modular programming with modules](https://en.wikipedia.org/wiki/Modular_programming)
  * `module` declarations and `import`.
* [Bi-directional type inference](https://en.wikipedia.org/wiki/Type_inference)
  * Recursively infers from both unknown and known types
* [Static structual type system](https://en.wikipedia.org/wiki/Structural_type_system)
  * Aggregate types of the same layout are the same type.
  * Strongly typed: No implicit type conversions of any kind except for untyped integer and floating-point literals.
* [Polymorphic effect system](https://en.wikipedia.org/wiki/Effect_system)
  * [Nominally typed](https://en.wikipedia.org/wiki/Nominal_type_system) effects required for all side effects.
  * Some builtin effects
    * `MemoryOrder` - Atomic operation memory order
    * `Read`        - Reads global memory
    * `Write`       - Writes global memory
  * Effects are established with `using` statement.
* [Algebaic data types](https://en.wikipedia.org/wiki/Algebraic_data_type)
  * Sum types with [flow-sensitive typing](https://en.wikipedia.org/wiki/Flow-sensitive_typing)
    * Test an expresison's type with `is` operator.
* [Array programming](https://en.wikipedia.org/wiki/Array_programming)
  * Recursive and implicitly vectorized.
* [Static multiple dispatch](https://en.wikipedia.org/wiki/Multiple_dispatch)
  * Absense-based object oriented function calls.
* Consistent set of builtin types
  * Sized integer types: `(S|U)int{8,16,32,64}`
  * Sized floating-point types: `Real{32,64}`
  * Sized boolean types: `Bool{8,16,32,64}`
  * Pointers: `*T`
  * Slices: `[]T`
  * Arrays: `[N]T`
  * Tuples: `(T1, ... Tn)`
  * Unions: `T1 | ... | Tn`
  * Atomics: `@T`
  * Addressing: `Address`
    * Similar to `uintptr_t` but for working with memory addresses and can be casted to any pointer type.
  * Non-NUL-terminated and immutable UTF-8 string: `String`
* Designed to run on baremetal
* Small
  * ~12k lines of freestanding C++ with no build dependencies.
    * Does not require the C++ standard library or C++ runtime support library.
  * Robust error handling
  * Embeddable
    * Custom allocators for limiting memory usage
    * Can be used directly to make a LSP for instance
  * Sandboxable
    * All interation with the system is done through the system interface which is replacable.
  * Modular: The lexer, parser, and code generator are all separate components which do not depend on each other.
  * Loads `libLLVM` dynamically at runtime if present. Not linked during build so can easily be built and deployed anywhere.
    * Requires LLVM-17, LLVM-18, or LLVM-19 to be present at runtime.

## Building

### Linux
On Linux you can build with
```
$ make
```

### Windows
On Windows you can build with
```
$ cl.exe /I..\ /std:c++20 Zc:__cplusplus unity.cxx
```

## TODO
* Finish modules
* Implement pattern matching
* Implement parametric polymorphism (for generics)
* Implement intrinsic effects