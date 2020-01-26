version = 0.1
build = rel

csrc = $(wildcard src/*.c) \
	   $(wildcard src/splash/*.c) \
	   $(wildcard src/libc/*.c) \
	   $(wildcard src/tui/*.c) \
	   $(wildcard src/ui/*.c) \
	   $(wildcard libs/zlib/*.c) \
	   $(wildcard libs/libpng/*.c)
ssrc = $(wildcard src/*.s) \
	   $(wildcard src/splash/*.s) \
	   $(wildcard src/libc/*.s) \
	   $(wildcard src/boot/*.s)
Ssrc = $(wildcard src/*.S)
obj = $(csrc:.c=.o) $(ssrc:.s=.o) $(Ssrc:.S=.o)
dep = $(obj:.o=.d)
elf = 256boss.elf
bin = 256boss.bin

ssrc += data/bos48.s data/bos64.s data/bos96.s data/bos128.s

warn = -pedantic -Wall
inc = -Isrc -Isrc/libc -Isrc/dtx -Ilibs/libpng -Ilibs/zlib
def = -DNO_GZCOMPRESS -DPNG_NO_WRITE_SUPPORTED -DVER_STR=\"$(version)\"
gccopt = -fno-pic -ffreestanding -nostdinc -fno-builtin -ffast-math

ifeq ($(build), rel)
	opt = -O2
	def += -DNDEBUG
else
	dbg = -g
endif

CFLAGS = $(ccarch) -march=i386 $(warn) $(opt) $(dbg) $(gccopt) $(inc) $(def)
ASFLAGS = $(asarch) -march=i386 $(dbg) -nostdinc -fno-builtin $(inc)
LDFLAGS = $(ldarch) -nostdlib -T 256boss.ld -print-gc-sections

#QEMU_FLAGS = -fda floppy.img -serial file:serial.log -soundhw sb16
QEMU_FLAGS = -hda disk.img -serial file:serial.log -soundhw sb16

ifneq ($(shell uname -m), i386)
	ccarch = -m32
	asarch = --32
	ldarch = -m elf_i386
endif

.PHONY: all
all: disk.img #floppy.img

disk.img: 256boss.img blank.img
	@echo
	@echo - patching 256boss onto the blank disk image ...
	cp blank.img $@
	dd if=256boss.img of=$@ bs=512 status=none conv=notrunc
	dd if=blank.img of=$@ bs=1 seek=440 skip=440 count=70 status=none conv=notrunc

blank.img:
	@echo
	@echo - generating blank disk image with FAT partition ...
	dd if=/dev/zero of=part.img bs=1024 count=63488
	mkfs -t vfat -n 256BOSS part.img
	mmd -i part.img ::.data
	mcopy -i part.img data/* ::.data
	dd if=/dev/zero of=$@ bs=1024 count=65536
	echo start=2048 type=c | sfdisk $@
	dd if=part.img of=$@ bs=512 seek=2048 conv=notrunc
	rm part.img

floppy.img: 256boss.img
	dd if=/dev/zero of=$@ bs=512 count=2880
	dd if=$< of=$@ conv=notrunc

256boss.iso: floppy.img
	rm -rf cdrom
	git archive --format=tar --prefix=cdrom/ HEAD | tar xf -
	cp $< cdrom
	mkisofs -o $@ -R -J -V 256boss -b $< cdrom


256boss.img: bootldr.bin $(bin)
	cat bootldr.bin $(bin) >$@

# bootldr.bin will contain .boot, .boot2, .bootend, and .lowtext
bootldr.bin: $(elf)
	objcopy -O binary -j '.boot*' -j .lowtext $< $@

# the main binary will contain every section *except* those
$(bin): $(elf)
	objcopy -O binary -R '.boot*' -R .lowtext $< $@

$(elf): $(obj)
	$(LD) -o $@ $(obj) -Map link.map $(LDFLAGS)

%.o: %.S
	$(CC) -o $@ $(CFLAGS) -c $<

src/splash/data.o: src/splash/data.s data/grad

-include $(dep)

%.d: %.c
	@$(CPP) $(CFLAGS) $< -MM -MT $(@:.d=.o) >$@

.PHONY: clean
clean:
	rm -f $(obj) $(bin) 256boss.img floppy.img disk.img link.map

.PHONY: cleandep
cleandep:
	rm -f $(dep)

.PHONY: disasm
disasm: bootldr.disasm $(elf).disasm

bootldr.disasm: $(elf)
	objdump -d $< -j .boot -j .boot2 -j .lowtext -m i8086 >$@

$(elf).disasm: $(elf)
	objdump -d $< -j .startup -j .text -j .lowtext -m i386 >$@

$(elf).sym: $(elf)
	objcopy --only-keep-debug $< $@

.PHONY: run
run: $(bin)
	qemu-system-i386 $(QEMU_FLAGS)

.PHONY: debug
debug: $(bin) $(elf).sym
	qemu-system-i386 $(QEMU_FLAGS) -s -S

.PHONY: sym
sym: $(elf).sym

.PHONY: mount
mount: disk.img
	mount -o loop,offset=1048576 $< /mnt

.PHONY: data
data: blank.img
	mcopy -D o -i $<@@1M data/* ::.data
	$(MAKE)

tools/csprite/csprite:
	$(MAKE) -C tools/csprite

data/bos48.s: data/bos.png tools/csprite/csprite
	tools/csprite/csprite -coffset 64 -n bos48 -s 30x48 -r 90x48+120+224 $< >$@

data/bos64.s: data/bos.png tools/csprite/csprite
	tools/csprite/csprite -coffset 64 -n bos64 -s 40x64 -r 120x64+0+224 $< >$@

data/bos96.s: data/bos.png tools/csprite/csprite
	tools/csprite/csprite -coffset 64 -n bos96 -s 60x96 -r 180x96+0+128 $< >$@

data/bos128.s: data/bos.png tools/csprite/csprite
	tools/csprite/csprite -coffset 64 -n bos128 -s 80x128 -r 240x128+0+0 $< >$@

data/bos48.o: data/bos48.s data/bos.png
data/bos64.o: data/bos64.s data/bos.png
data/bos96.o: data/bos96.s data/bos.png
data/bos128.o: data/bos128.s data/bos.png
