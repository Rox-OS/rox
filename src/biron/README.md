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
  * Slices: `[]T` and `[*]T`
  * Arrays: `[N]T`
  * Tuples: `{T1, ... Tn}`
  * Unions: `T1 | ... | Tn`
  * Enums: `[ E1, .... En ]`
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

## TODO
* Finish modules
* Implement pattern matching
* Implement optional types `?T` with flow-sensitive typing of optionals
* Implement parametric polymorphism (for generics)
  * `fn[T]` syntax for `fn` ?
* Implement intrinsic effects

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

## Reference
This is an incomplete and terse reference of the language.

### Comments
There are both line and block comments
```rust
// This is a line comment
/* This is a block comment */
```
> Block comments can nest

### Functions
Functions are composed of three lists and an optional return value
  * Reciever list
  * Argument list
  * Effects list

#### Simple functions
```rust
fn test() {
  // takes no recievers, no arguments, no effects, returns no values
  // call with: test()
}
fn test() -> String {
  // takes no recievers, no arguments, no effects, returns a value
  // call with: test()
  return "Hello, world";
}
fn test(value: String) -> String {
  // takes no recievers, an argument, no effects, returns a value
  // call with: test(value)
  return value;
}
fn test(a: Uint32, b: Uint32) -> Uint32 {
  // takes no recievers, multiple arguments, no effects, returns a value
  // call with: test(a, b)
  return a + b;
}
fn alloc(size: Uint64) <Allocate> -> Address {
  // takes no recievers, an argument, an Allocate effect, returns a value
  // call with: alloc(size)
  // requires: Allocate effect
}
```

#### Reciever functions
```rust
fn(obj: *Square) area() -> Real32 {
  // takes a single reciever, no arguments, no effects, returns a value
  // call with: square.area()
  return obj.w * obj.h;
}
fn(obj: *Square) area(scale: Real32) -> Real32 {
  // takes a single reciever, an argument, no effects, returns a value
  // call with: square.area(scale)
  return obj.w * obj.h * scale;
}
fn(lhs: *Spaceship, rhs: *Astroid) collide() -> Bool32 {
  // takes multiple recievers, no arguments, no effects, returns a value
  // call with: (spaceship, astroid).collide()
  // ...
  return false;
}
fn(obj: *Square) print() <IO> {
  // takes a single reciever, no arguments, an effect, returns nothing
  IO!.printf("w = %f, h = %f\n", obj.w, obj.h);
}
```


### Types
#### Primitive types
  * `Uint{8,16,32,64}`
  * `Sint{8,16,32,64}`
  * `Real{32,64}`
  * `Bool{8,16,32,64}`
  * `Address`
#### Composite types
  * `*T` pointer to `T`
  * `@T` atomic `T`
  * `[]T` bounded slice of `T`
  * `[*]T` unbounded slice of `T`
  * `[N]T` array of `N` x `T`
  * `[?]T` size-inferred array of `T`
#### Structual types
  * `{ T1, ... Tn }` tuple
  * `[ .ENUM, ... ]` enum
  * `T1 | ... | Tn` sum

```
type A = { Uint32 };         // single field tuple with unnamed field
type B = { n: Uint32 };      // single field tuple with named field
type C = { Uint32, Real32 }; // multiple field tuple
type D = { { Uint32 } };     // single field tuple with unnamed tuple field
type E = [ .A ];             // single enumerator enum type
type F = [ .A, .B ];         // multiple enumerator enum type
type G = [ .A = 10, .B ];    // with initial value
type H = Uint32 | String;    // sum type
type I = A | G | H;          // more sum type
type J = []Uint32;           // slice
type K = [10]Uint32;         // array
type L = [*]Uint32;          // unbounded slice
type M = @Uint32;            // atomic Uint32
```

> The `type name = ` syntax is used to make a type alias.

### Operators
#### Binary
* Arithmetic:
  * `a + b`
  * `a - b`
  * `a * b`
  * `a / b`
  * `a % b`
* Comparison:
  * `a == b`
  * `a != b`
* Relational:
  * `a < b`
  * `a > b`
  * `a >= b`
  * `a <= b`
* Logical (with short-circuting behavior):
  * `a || b`
  * `a && b`
* Bitwise:
  * `a & b`
  * `a | b`
  * `a ^ b`
  * `a << b` - left shift
  * `a >> b` - right shift
* Min and max:
  * `a <? b` - min(a, b)
  * `a >? b` - max(a, b)
* Type
  * `a is T` - type test
  * `a as T` - type cast
* Other
  * `a of b` - property lookup
#### Unary
* Sign:
  * `+a`
  * `-a`
* Logical:
  * `!a` - logical not
* Addressing:
  * `*a` - dereference
  * `&a` - address of
* Suffix
  * `a!` - effect