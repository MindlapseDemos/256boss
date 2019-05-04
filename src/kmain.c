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
#include <string.h>
#include <ctype.h>
#include "segm.h"
#include "intr.h"
#include "mem.h"
#include "keyb.h"
#include "psaux.h"
#include "timer.h"
#include "contty.h"
#include "video.h"
#include "audio.h"
#include "pci.h"
#include "bootdev.h"
#include "floppy.h"

#include "boot.h"

void test(void);

void kmain(void)
{
	init_segm();
	init_intr();

	con_init();
	kb_init();
	init_psaux();

	init_mem();

	/*init_pci();*/

	/* initialize the timer */
	init_timer();

	/*audio_init();*/

	enable_intr();

	bdev_init();

	printf("256boss initialized\n");

	for(;;) {
		int c;

		halt_cpu();
		while((c = kb_getkey()) >= 0) {
			if(isprint(c)) {
				printf("key: %d '%c'\n", c, (char)c);
			} else {
				printf("key: %d\n", c);
			}

			switch(c) {
			case ' ':
				test();
				break;

			case KB_F4:
				printf("turning floppy motors off\n");
				floppy_motors_off();
				break;

			case KB_F1:
				printf("interrupt state\n");
				printf(" IF: %s\n", get_intr_flag() ? "enabled" : "disabled");
				printf(" PIC1 mask: %x\n", get_pic_mask(0));
				printf(" PIC2 mask: %x\n", get_pic_mask(1));
				break;

			case KB_F2:
				printf("unmasking interrupts\n");
				disable_intr();
				outb(0, 0x21);
				outb(0, 0xa1);
				enable_intr();
				break;
			}

		}
		if((nticks % 250) == 0) {
			con_printf(71, 0, "[%ld]", nticks);
		}
	}
}

static unsigned char sectdata[512];

struct part_record {
	uint8_t stat;
	uint8_t first_head, first_cyl, first_sect;
	uint8_t type;
	uint8_t last_head, last_cyl, last_sect;
	uint32_t first_lba;
	uint32_t nsect_lba;
} __attribute__((packed));

#define PTABLE_OFFS		0x1be

void test(void)
{
	int i;
	struct part_record *prec;
	printf("Boot drive: %d\n", boot_drive_number);

	if(bdev_read_sect(0, sectdata) == -1) {
		printf("Failed to read sector 0\n");
		return;
	}

	if(sectdata[510] != 0x55 || sectdata[511] != 0xaa) {
		printf("invalid MBR, last bytes: %x %x\n", (unsigned int)sectdata[510],
				(unsigned int)sectdata[511]);
		return;
	}

	prec = (struct part_record*)(sectdata + PTABLE_OFFS);

	printf("Partition table\n");
	printf("---------------\n");
	for(i=0; i<4; i++) {
		/* ignore empty partitions */
		if(prec[i].type == 0) {
			continue;
		}

		printf("type: %x, start: %lu, size: %lu\n", (unsigned int)prec[i].type,
				(unsigned long)prec[i].first_lba, (unsigned long)prec[i].nsect_lba);
	}
	printf("---------------\n");
}
