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
#include <stdlib.h>
#include <string.h>
#include "vbe.h"
#include "video.h"
#include "keyb.h"
#include "contty.h"
#include "asmops.h"
#include "ui/fsview.h"

static void draw(void);
static int modecmp(const void *a, const void *b);

static struct video_mode vmode;
static void *fbptr;

int gfxui(void)
{
	struct vbe_edid edid;
	struct video_mode vinf, *vmodes;
	int i, xres, yres, nmodes, mode = -1;

	if(vbe_get_edid(&edid) == 0 && edid_preferred_resolution(&edid, &xres, &yres) == 0) {
		printf("EDID: preferred resolution: %dx%d\n", xres, yres);
		mode = find_video_mode(xres, yres, 32);
	}

	if(mode == -1) {
		nmodes = video_mode_count();
		if(!(vmodes = malloc(nmodes * sizeof *vmodes))) {
			printf("failed to allocate video modes array (%d modes)\n", nmodes);
			return -1;
		}

		for(i=0; i<nmodes; i++) {
			video_mode_info(i, &vinf);
			vmodes[i] = vinf;
		}

		qsort(vmodes, nmodes, sizeof *vmodes, modecmp);

		for(i=0; i<nmodes; i++) {
			if((fbptr = set_video_mode(vmodes[i].mode))) {
				vmode = vmodes[i];
				break;
			}
		}
		free(vmodes);
		if(i >= nmodes) {
			printf("failed to find a suitable video mode\n");
			return -1;
		}
	}

	for(;;) {
		int c;

		halt_cpu();
		while((c = kb_getkey()) >= 0) {
			/* global overrides for all views */
			switch(c) {
			case KB_F3:
			case KB_F4:
				break;

			case KB_F8:
				goto end;
			}

			/*
			if(keypress(c) == -1) {
				draw = fsview_draw;
				keypress = fsview_keypress;
			}
			*/
		}

		draw();
	}

end:
	con_scr_enable();
	set_vga_mode(3);
	con_clear();
	con_show_cursor(1);
	return 0;
}

static void draw(void)
{
	memset(fbptr, 0xff, vmode.width * vmode.height * vmode.bpp / 8);
}

static int modecmp(const void *a, const void *b)
{
	const struct video_mode *ma = a;
	const struct video_mode *mb = b;
	unsigned long aval = ma->width | (ma->height << 16);
	unsigned long bval = mb->width | (mb->height << 16);

	if(aval != bval) {
		return bval - aval;
	}
	return mb->bpp - ma->bpp;
}
