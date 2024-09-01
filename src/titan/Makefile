BIRONDIR := ../biron
BIRONC   := $(BIRONDIR)/bironc

LDFLAGS := -m elf_x86_64
LDFLAGS += -nostdlib
LDFLAGS += -pie
LDFLAGS += -z text
LDFLAGS += -z max-page-size=0x1000
LDFLAGS += -T link.ld

titan: $(BIRONC)
	$(BIRONC) -bm -O3 main.biron
	$(CC) -ffreestanding -fno-stack-protector -c fpu.c -o fpu.o
	$(LD) main.o fpu.o $(LDFLAGS) -o $@
	rm main.o fpu.o

$(BIRONC):
	$(MAKE) -C $(BIRONDIR)

clean:
	rm -rf main.o titan

.PHONY: $(BIRONC) titan clean