
EXE_LIST = xorriso qemu-system-x86_64
EXT_TEST := $(foreach exec,$(EXE_LIST),\
	$(if $(shell command -v $(exec)),some string,\
		$(error Could not find "$(exec)" in $$PATH. Please install which ever package provides this before continuing)))

# xorriso flags
XIFLAGS := -as mkisofs
XIFLAGS += -b boot/limine/limine-bios-cd.bin
XIFLAGS += -no-emul-boot
XIFLAGS += -boot-load-size 4
XIFLAGS += -boot-info-table
XIFLAGS += --efi-boot boot/limine/limine-uefi-cd.bin
XIFLAGS += -efi-boot-part
XIFLAGS += --efi-boot-image
XIFLAGS += --protective-msdos-label
XIFLAGS += iso

# qemu flags
QFLAGS := -M q35
QFLAGS += -m 2G
QFLAGS += -cdrom rox.iso
QFLAGS += -boot d
QFLAGS += -no-reboot
QFLAGS += -d int,page
QFLAGS += -rtc base=localtime,clock=vm,driftfix=slew
QFLAGS += -D qemu.log

# If we support KVM then use KVM for QEMU acceleration
KVM := $(shell lsmod | grep kvm)
ifeq ($(.SHELLSTATUS),0)
	QFLAGS += -accel kvm
endif

rox.iso: .limine/limine titan
	rm -rf iso
	mkdir -p iso/boot/limine
	mkdir -p iso/EFI/BOOT
	cp limine.cfg iso/boot/limine
	cp titan/titan iso/boot
	cp .limine/limine-bios.sys iso/boot/limine
	cp .limine/limine-bios-cd.bin iso/boot/limine
	cp .limine/limine-uefi-cd.bin iso/boot/limine
	cp .limine/BOOTX64.EFI iso/EFI/BOOT/
	xorriso $(XIFLAGS) -o $@ >/dev/null 2>&1
	.limine/limine bios-install $@ >/dev/null 2>&1
	@rm -rf iso

.limine/limine:
	rm -rf .limine
	git clone https://github.com/limine-bootloader/limine.git --branch=v7.x-binary --depth=1 .limine
	$(MAKE) -C .limine

titan:
	$(MAKE) -C titan

run: rox.iso
	qemu-system-x86_64 $(QFLAGS)

clean:
	$(MAKE) -C titan clean
	$(MAKE) -C biron clean
	rm -rf iso rox.iso

purge:
	$(MAKE) -C biron purge
	rm -rf .limine
	rm -rf iso
	rm -rf rox.iso

.PHONY: titan run clean purge