# Rox
Rox is an experimental toy operating system written in an experimental toy programming language Biron.

## Biron
Biron is an experimental toy systems programming language built for Rox.
  
You may read more about Biron [here](src/biron/README.md)

## Titan
Titan is an experimental kernel written in Biron used for the Rox operating system.
  
You may read more about Titan [here](src/titan/README.md)

## Building
To build run `make` from the `src` directory.

This will build both Biron and Titan. It will (if the first time) fetch a precompiled [Limine](https://limine-bootloader.org/) bootloader and finally build a BIOS and UEFI compatible ISO named `rox.iso` which can be booted by qemu, virtualbox, or on real AMD64 hardware.

### Dependencies
  * LLVM 17 or 18
  * [xorriso](https://www.gnu.org/software/xorriso/)
  * [qemu](https://www.qemu.org/) Optional if you want to run it.

If you're on an ArchLinux distribution these dependencies can be downloaded easily with
```
$ pacman -S llvm xorriso qemu
```

## Running
### Qemu
```
$ make run
```
> Ensure you have KVM enabled for QEMU acceleration otherwise it will run slow

### VirtualBox
Within the VirtualBox [Create Virtual Machine Wizard](https://www.virtualbox.org/manual/UserManual.html#create-vm-wizard) you can mount the `rox.iso`

### Real Hardware
You can also run it on real hardware by making a bootable USB drive
```
$ dd if=rox.iso of=/dev/sdN # Where sdN is your USB drive
```

> The ISO image supports both Legacy BIOS and EFI booting