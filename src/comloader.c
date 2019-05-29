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

extern int run_com_entry;

int run_com_binary(void)
{
	uint16_t *ivt = 0;
	struct int86regs regs = {0};
	uint32_t entry_addr;

	/* setup real mode interrupt handler for running COM files */
	entry_addr = (uint32_t)&run_com_entry;
	ivt[COMRUN_INT * 2] = entry_addr;	/* offs */
	ivt[COMRUN_INT * 2 + 1] = 0;		/* seg */

	int86(COMRUN_INT, &regs);
	return regs.eax;
}
