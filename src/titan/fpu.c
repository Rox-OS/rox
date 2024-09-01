void enable_fpu() {
	unsigned long cr4;
	asm volatile ("mov %%cr4, %0" : "=r"(cr4));
	cr4 |= 0x200;
	asm volatile ("mov %0, %%cr4" :: "r"(cr4));
	unsigned short m = 0x37f;
	asm volatile ("fldcw %0" :: "m"(m));
}
