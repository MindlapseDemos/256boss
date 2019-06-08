/*
256boss - bootable launcher for 256byte intros
Copyright (C) 2018-2019  John Tsiombikas <nuclear@member.fsf.org>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY, without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#include <stdio.h>
#include "comloader.h"
#include "boot.h"
#include "int86.h"
#include "intr.h"

#define COMRUN_INT	0xf0

int load_com_binary(const char *path)
{
	FILE *fp;
	unsigned char *dest = low_mem_buffer + 256;
	int max_size = (unsigned char*)0xa0000 - dest;
	int sz;

	printf("com loader: max size: %d\n", max_size);

	if(!(fp = fopen(path, "rb"))) {
		printf("com loader: failed to open file: %s\n", path);
		return -1;
	}
	sz = fread(dest, 1, max_size, fp);
	fclose(fp);

	printf("com loader: loaded %d bytes\n", sz);
	return 0;
}

#define ORIG_IRQ_OFFS	8
#define KBIRQ	1
#define KBINTR	(KBIRQ + ORIG_IRQ_OFFS)

extern int run_com_entry;
extern int rm_keyb_intr;
extern int dos_int21h_entry;

struct vector {
	uint16_t offs, seg;
} __attribute__((packed));

int run_com_binary(void)
{
	int intr;
	struct vector *ivt = 0;
	struct int86regs regs = {0};

	/* Restore original PIC mapping
	 * otherwise the DOS int21h interrupt and keyboard IRQ1 would conflict
	 * The pmode mapping is restored at the end of int86
	 */
	intr = get_intr_flag();
	disable_intr();
	prog_pic(8);

	/* setup real mode interrupt handler for running COM files */
	ivt[COMRUN_INT].seg = 0;
	ivt[COMRUN_INT].offs = (uint32_t)&run_com_entry;
	ivt[KBINTR].seg = 0;
	ivt[KBINTR].offs = (uint32_t)&rm_keyb_intr;
	ivt[0x21].seg = 0;
	ivt[0x21].offs = (uint32_t)&dos_int21h_entry;

	int86(COMRUN_INT, &regs);
	set_intr_flag(intr);
	return regs.eax;
}
