# Rox
Rox is an experimental toy operating system written in an experimental toy programming language Biron.

## Biron
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
* Small: ~5k lines of C++

## Titan
Titan is an experimental kernel written in Biron used for the Rox operating system.

## Building
To build run `make` from the `src` directory.

This will build both Biron and Titan. It will (if the first time) fetch a precompiled [Limine](https://limine-bootloader.org/) bootloader and finally build a BIOS and UEFI compatible ISO named `rox.iso` which can be booted by qemu, virtualbox, or on real AMD64 hardware.

### Dependencies
  * LLVM 17 or 18
  * [xorriso](https://www.gnu.org/software/xorriso/)
  * [qemu](https://www.qemu.org/)

If you're on an ArchLinux distribution these dependencies can be downloaded easily with
```
$ pacman -S llvm xorriso qemu
```

## Running
To run Rox in qemu
```
$ make run
```
> You can also mount the `rox.iso` in Virtualbox.

Finally, you can run it on real hardware by making a bootable USB drive.
```
$ dd if=rox.iso of=/dev/sdN # Where sdN is your USB drive
```