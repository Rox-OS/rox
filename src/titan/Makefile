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
	$(LD) main.o $(LDFLAGS) -o $@

$(BIRONC):
	$(MAKE) -C $(BIRONDIR)

clean:
	rm -rf main.o titan

.PHONY: $(BIRONC) titan clean