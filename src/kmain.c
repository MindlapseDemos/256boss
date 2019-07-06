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
#include "config.h"
#include "segm.h"
#include "intr.h"
#include "mem.h"
#include "keyb.h"
#include "psaux.h"
#include "timer.h"
#include "rtc.h"
#include "contty.h"
#include "video.h"
#include "audio.h"
#include "pci.h"
#include "bootdev.h"
#include "floppy.h"
#include "part.h"
#include "fs.h"
#include "kbregs.h"
#include "shell.h"
#include "ui/fsview.h"

#ifdef ENABLE_GDB_STUB
#include "serial.h"
void set_debug_traps(void);
#endif

void splash_screen(void);

static void mount_boot_fs(void);
static void print_intr_state(void);

void kmain(void)
{
	init_segm();
	init_intr();

#ifdef ENABLE_GDB_STUB
	if(ser_open(GDB_SERIAL_PORT, 9600, SER_8N1) >= 0) {
		set_debug_traps();
		asm("int $3");
	}
#endif

	con_init();
	kb_init();
	init_psaux();

	init_mem();

	/*init_pci();*/

	/* initialize the timer */
	init_timer();
	init_rtc();

	/*audio_init();*/

	enable_intr();

	bdev_init();

	mount_boot_fs();

	//fsv_init(&fsview);

#ifdef AUTOSTART_GUI
	if(!kb_isdown(KB_F8)) {
		splash_screen();
	}
#endif

	/* debug shell. we end up here if we hold down F8 during startup */
	sh_init();

	for(;;) {
		int c;

		halt_cpu();
		while((c = kb_getkey()) >= 0) {
			switch(c) {
			case KB_F4:
				printf("turning floppy motors off\n");
				floppy_motors_off();
				break;

			case KB_F1:
				print_intr_state();
				break;

			case KB_F2:
				printf("unmasking interrupts\n");
				disable_intr();
				outb(0, 0x21);
				outb(0, 0xa1);
				enable_intr();
				break;

			case KB_F5:
				init_pci();
				break;

			default:
				sh_input(c);
			}

		}
		if((nticks % 250) == 0) {
			con_printf(71, 0, "[%ld]", nticks);
		}
	}
}

static void mount_boot_fs(void)
{
	struct fs_node *n;
	char buf[32];
	int sz;

	fs_mount(DEV_MEMDISK, 0, 0, 0);
	n = fs_open("/foo", FSO_CREATE);
	fs_write(n, "hello\n", 6);
	fs_close(n);

	printf("printing file contents:\n");
	n = fs_open("/foo", 0);
	sz = fs_read(n, buf, sizeof buf);
	fs_close(n);

	buf[sz] = 0;
	printf("%s\n", buf);

	/*
	int i, npart, num_mounts = 0;
	struct partition ptab[32];

	if((npart = read_partitions(-1, ptab, sizeof ptab / sizeof *ptab)) <= 0) {
		return;
	}

	print_partition_table(ptab, npart);

	for(i=0; i<npart; i++) {
		if(fs_mount(-1, ptab[i].start_sect, ptab[i].size_sect, 0) != -1) {
			num_mounts++;
		}
	}
	*/
}

static void print_intr_state(void)
{
	printf("interrupt state\n");
	printf(" IF: %s\n", get_intr_flag() ? "enabled" : "disabled");
	printf(" PIC1 mask: %x\n", get_pic_mask(0));
	printf(" PIC2 mask: %x\n", get_pic_mask(1));
}
