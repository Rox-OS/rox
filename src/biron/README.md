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

## Example
```
type Rect = (x: Real32, y: Real32, w: Real32, h: Real32);
fn(self: *Rect) area() -> Real32 {
  return self.w * self.h;
}
fn(self: *Rect) print_area(title: String) {
  printf("%s: area = %f\n", self.area());
}
fn(lhs: *Rect, rhs: *Rect) collide() -> Bool32 {
  return (lhs.x >= rhs.x && lhs.x <= rhs.x + rhs.w) ||
         (rhs.x >= lhs.x && rhs.x <= lhs.x + lhs.w) ||
         (lhs.y >= rhs.y && lhs.y <= rhs.y + rhs.h) ||
         (rhs.y >= lhs.y && rhs.y <= lhs.y + lhs.h);
}
fn main() {
  let lhs = Rect{10, 10, 0, 10};
  let rhs = Rect{10, 10, 0,  0};
  lhs.print_area("lhs");
  rhs.print_area("rhs");
  if (lhs, rhs).collide() {
    printf("collision detected\n");
  } else {
    printf("no collision\n");
  }
}
```
This example packs quite a lot of punch. The first line defines a custom composite type called `Rect` which is a tuple of four fields describing the position and size of the rectangle. This is what named structures look like in Biron.

The next line defines a function `area` which takes no arguments but one reciever of type `*Rect`. This is how methods look in Biron. They're free-form and can be defined in the global scope for any set of recievers of any type.

The next function `print_area` shows that the reciever list and the argument list are separate. This function takes one argument of type `String`. You can see that it also calls `area` on the `self` reciever passed in.

The next function `area` shows Biron's static multi-dispatch capablities as it takes two arguments to collide two `Rect`.

The `main` function is a regular function as it takes no recievers. The first two lines of `main` establishes two rectangles using the aggregate initializer syntax. Here you can see that Biron is a "right-typed" language. The types of `lhs` and `rhs` are infered from the expression on the right-hand side. There is no explicit type annotations in Biron. The next two lines we call the `print_area` methods. Here you can see that `lhs` and `rhs` are the recievers of `print_area`. The next line constructs a tuple of `(lhs, rhs)` and calls off the tuple a method `collide` which will match the `collide` function.

One thing to note is in Biron there is two forms of implicit behavior done for you
  * Implicit dereferencing
  * Implicit referencing

The type of `lhs` and `rhs` is `Rect` but when you call methods which take `*Rect` Biron will implicitly pass `lhs` and `rhs` by pointer.

Likewise, the recievers need not be typed as `*Rect` they can be typed as `Rect` in which case Biron will pass `lhs` and `rhs` by copy.

If `lhs` and `rhs` are already pointer type as-in `*Rect`, the `.` syntax still works and Biron will perform an implicit dereference for you. Unlike C and C++ which defer this to a special `->` operator.