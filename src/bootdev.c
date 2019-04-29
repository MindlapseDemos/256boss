#include <stdio.h>
#include <string.h>
#include "bootdev.h"
#include "boot.h"
#include "int86.h"

struct disk_access {
	unsigned char pktsize;
	unsigned char zero;
	uint16_t num_sectors;	/* max 127 on some implementations */
	uint16_t boffs, bseg;
	uint32_t lba_low, lba_high;
} __attribute__((packed));

enum {OP_READ, OP_WRITE};

static int bios_ext_rw_sect(int dev, uint64_t lba, int op, void *buf);

static int have_bios_ext;

void bdev_init(void)
{
	struct int86regs regs;

	memset(&regs, 0, sizeof regs);
	regs.eax = 0x4100;	/* function 41h: check int 13h extensions */
	regs.ebx = 0x55aa;
	regs.edx = boot_drive_number;

	int86(0x13, &regs);
	if(regs.flags & FLAGS_CARRY) {
		printf("BIOS does not support int13h extensions (LBA access)\n");
		have_bios_ext = 0;
	} else {
		printf("BIOS supports int13h extensions (LBA access)\n");
		have_bios_ext = 1;
	}
}

/* TODO: fallback to CHS I/O if extended calls are not available */
int bdev_read_sect(uint64_t lba, void *buf)
{
	return bios_ext_rw_sect(boot_drive_number, lba, OP_READ, buf);
}

int bdev_write_sect(uint64_t lba, void *buf)
{
	return bios_ext_rw_sect(boot_drive_number, lba, OP_WRITE, buf);
}


static int bios_ext_rw_sect(int dev, uint64_t lba, int op, void *buf)
{
	struct int86regs regs;
	struct disk_access *dap = (struct disk_access*)low_mem_buffer;
	uint32_t addr = (uint32_t)low_mem_buffer;
	uint32_t xaddr = (addr + sizeof *dap + 65535) & 0xffff0000;
	void *xbuf = (void*)xaddr;
	int func;

	if(op == OP_READ) {
		func = 0x42;	/* function 42h: extended read sector (LBA) */
	} else {
		func = 0x43;	/* function 43h: extended write sector (LBA) */
		memcpy(xbuf, buf, 512);
	}

	dap->pktsize = sizeof *dap;
	dap->zero = 0;
	dap->num_sectors = 1;
	dap->boffs = 0;
	dap->bseg = xaddr >> 4;
	dap->lba_low = (uint32_t)lba;
	dap->lba_high = (uint32_t)(lba >> 32);

	memset(&regs, 0, sizeof regs);
	regs.eax = func << 8;
	regs.ds = addr >> 4;
	regs.esi = 0;
	regs.edx = dev;

	int86(0x13, &regs);

	if(regs.flags & FLAGS_CARRY) {
		return -1;
	}

	if(op == OP_READ) {
		memcpy(buf, xbuf, 512);
	}
	return 0;
}

