# Biron

Biron is an experimental toy systems programming language built for Rox.

## Features
* Type inference
* Few keywords: `true false fn if as let for else type defer union break return continue`
* Rich builtin types:
  * Sized integer types: `(S|U)int{8,16,32,64}`
  * Sized floating-point types: `Real{32,64}`
  * Sized boolean types: `Bool{8,16,32,64}`
  * Memory addressing type: `Address`
    * Similar to `uintptr_t` but for working with memory addresses.
  * Pointers: `*T`
  * Slices: `[]T`
  * Arrays: `[N]T`
  * Tuples: `(T1, ... Tn)`
  * Unions: `T1 | ... | Tn`
  * Non-NUL-terminated UTF-8 string: `String`
* Structured control flow:
  * `if` `else` `else if` `for` `break` `continue`
* Array programming
* Free-form methods with static multiple-dispatch
* Designed to run on baremetal
* Small: ~10k lines of freestanding C++ with no build dependencies
  * Robust error handling
  * Embeddable
    * Custom allocators for limiting memory usage
    * Can be used directly to make a LSP for instance
  * Modular: The lexer, parser, and code generator are all separate components
  * Loads `libLLVM` dynamically at runtime if present.
    * Requires LLVM-17 or LLVM-18.
  * Does not require C++ standard library or C++ runtime support library.

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

The following is a mostly incomplete reference of the language

### Lexical elements and literals
#### Comments
Comments can be anywhere outside of a string or character literal.
  
Single line comments begin with `//`
```rust
// This is a line comment

let x = 0_u32; // Document this variable
```

Block comments begin with `/*` and end with `*/` like C. Unlike C however, block comments can be nested.
```rust
/* You can put anything inside here including code
  /* And other block comments */
  // And other line comments
*/
```

Comments are lexed as actual tokens.

#### String literals
String literals are enclosed in double quotes `"Like this"`. Special characters can be escaped with the backslash `\` character.
```rust
"Hello, World"
"\n" // This is a newline character
```
> To encode the backslash chracter literally you escape it as well `\\`.

#### Character literals
Character literals are enclosed in single quotes. Special characters can be escaped with the backslash `\` character like string literals.
```rust
'A'
'\n'
```

#### Integer literals
Integer literals come in two variants in Biron: typed and untyped. Single quotes are allowed in integer literals for better readability.
```rust
1'000'000'000'000 // One billion as an untyped integer
```

Literals can be explicitly typed with a type suffix
```rust
1'000'000'000'000_u64 // One billion as a Uint64
```

* When an integer literal begins with `0x` it's treated as a base-16 literal
* When an integer literal begins with `0b` it's treated as a base-2 literal

> There are no base-8 (octal) literals.

There are sixteen explicit type suffixes for integer literals
| Suffix | Type    |
|--------|---------|
| `_u8`  | Uint8   |
| `_s8`  | Sint8   |
| `_u16` | Uint16  |
| `_s16` | Sint16  |
| `_u32` | Uint32  |
| `_s32` | Sint32  |
| `_u64` | Uint64  |
| `_s64` | Sint64  |

#### Floating-point Literals
Floating-point literals come in two variants in Biron: typed and untyped. Single quotes are allowed in floating-point literals for better readability just like integer literals.
```rust
100'000.0
```

Literals can be explicitly typed with a type suffix
```rust
100'000.0_f64
```

There are two explicit type suffixes for floating-point literals
| Suffix | Type   |
|--------|--------|
| `_f32` | Real32 |
| `_f64` | Real64 |

### Let statement
A `let` statement declares a new variable in the current scope.
```rust
let x = 10_u32;
```

There is no way to specify a type on a `let` statement. It's always inferred from the expression on the right-hand side.

The declaration of a variable must be unique within a scope
```rust
let x = 10_u32;
let x = 20_u32; // Not allowed since 'x' is already declared
```

### Assignment statements
The assignment statement assigns a new value to a variable.
```rust
let x = 123_u32;
x = 637_u32; // Assigns a new value to 'x'
```

### Control flow statements
#### The `for` statement
The `for` statement has the following syntax:

```rust
for <stmt>;? <expr>;? <stmt>? {
  // ...
}
```

Where `?` here denotes an optional statement or expression. When all elements are omitted the loop is an infinite one.
```rust
for {
  // Infinite loop
}
```

Something like the following is equivalent to C's `while` loop.
```rust
for expr {

}
```

Then of course you have the tried and true C style `for` loop
```rust
for let i = 0; i < 10; i = i + 1 {
  // Runs for 10 iterations
}
```

Loops in Biron also have an optional `else` clause like [Python 3](https://book.pythontips.com/en/latest/for_-_else.html). The `else` clause executes only when the for loop completes normally. Where normally here means when the loop expression no longer evaluates true as opposed to being terminated early by a `break`. This lets you write code like the following
```rust
for let n = 2; n < 10; n = n + 1 {
  for let x = 2; x < n; x = x + 1 {
    if n % x == 0 {
      printf("%d equals %d * %d\n", n, x, n/x);
    }
  } else {
    printf("%s is not a prime number\n", n);
  }
}
```