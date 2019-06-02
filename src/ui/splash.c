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
/* Splash screen showing how to change between UI modes and how to fallback
 * to text-mode if necessary. Uses mode 13h to ensure maximum compatibility.
 */
#include <string.h>
#include "asmops.h"
#include "keyb.h"
#include "video.h"

#define DATA_PATH	"/.data/"

static void setup_video(void);
static void draw(void);

static unsigned char *vmem = (unsigned char*)0xa0000;

void splash_screen(void)
{
	setup_video();

	for(;;) {
		int c;

		halt_cpu();
		while((c = kb_getkey()) >= 0) {
			switch(c) {
			case KB_F8:
				goto end;
			}
		}

		wait_vsync();
		draw();
	}

end:
	set_vga_mode(3);
}

static void setup_video(void)
{
	int i;
	set_vga_mode(0x13);

	for(i=0; i<256; i++) {
		set_pal_entry(i, i, i, i);
	}
}

static void draw(void)
{
	memset(vmem, 0, 64000);
}
