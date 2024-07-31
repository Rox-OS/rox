OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)

ENTRY(kmain)

PHDRS {
  headers PT_PHDR PHDRS;
  text    PT_LOAD FILEHDR PHDRS;
  rodata  PT_LOAD;
  data    PT_LOAD;
  dynamic PT_DYNAMIC;
}

SECTIONS {
  /* Place the kernel in the top-most @ GiB of the address space */
  . = 0xffffffff80000000 + SIZEOF_HEADERS;
  .text : {
    *(.text .text.*)
  } :text

  /* Place the read only data in the next page */
  . = ALIGN(CONSTANT(MAXPAGESIZE));
  .rodata : {
    *(.rodata .rodata.*)
  } :rodata

  /* Place the read write data in the next page */
  . = ALIGN(CONSTANT(MAXPAGESIZE));
  .data : {
    *(.data .data.*)

    /* We also need to place the bootloader sections in the data section */
    KEEP(*(.requests_beg_marker))
    KEEP(*(.requests))
    KEEP(*(.requests_end_marker))
  } :data

  /* The dynamic section for relocations */
  .dynamic : {
    *(.dynamic)
  } :data :dynamic

  /* Finally map the zero section last */
  .bss : {
    *(.bss .bss.*)
    *(COMMON)
  } :data

  /* We will discard any sections that LLVM adds to the Biron object files */
  /DISCARD/ : {
    *(.eh_frame*)
    *(.note .note.*)
    *(.interp)
  }
}
