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
#include <assert.h>
#include "vbe.h"
#include "video.h"
#include "keyb.h"
#include "contty.h"
#include "asmops.h"
#include "cfgfile.h"
#include "datapath.h"
#include "ui/fsview.h"

#include "widget.h"

static int video_init(void);
static void draw(void);
static int modecmp(const void *a, const void *b);

static struct video_mode vmode;
static void *fbptr;

static struct gui_gfx ggfx;
static struct gui_widget win;

int gfxui(void)
{
	if(video_init() == -1) {
		return -1;
	}

	gui_setgfx(&ggfx);
	gui_framebuffer(fbptr, vmode.width, vmode.height, vmode.width * vmode.bpp / 8);
	gui_pixelformat(vmode.bpp, vmode.rbits, vmode.gbits, vmode.bbits);

	gui_window(&win, 10, 10, 100, 100, "testwin", 0);

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

static int video_init(void)
{
	struct vbe_edid edid;
	struct video_mode vinf, *vmodes;
	int i, xres, yres, nmodes, mode_idx = -1;
	const char *vendor;
	struct cfglist *cfg;
	struct cfgopt *opt;

	if((cfg = load_cfglist(datafile("256boss.cfg")))) {
		if((opt = cfg_getopt(cfg, "resolution"))) {
			char *endp;
			xres = strtol(opt->valstr, &endp, 10);
			if(endp != opt->valstr) {
				yres = atoi(endp + 1);
			}
			mode_idx = find_video_mode_idx(xres, yres, 0);
		}
		free_cfglist(cfg);
	}

	if(mode_idx == -1 && (vendor = get_video_vendor()) && strstr(vendor, "SeaBIOS")) {
		mode_idx = find_video_mode_idx(800, 600, 0);
	}

	if(mode_idx == -1 && vbe_get_edid(&edid) == 0 && edid_preferred_resolution(&edid, &xres, &yres) == 0) {
		printf("EDID: preferred resolution: %dx%d\n", xres, yres);
		mode_idx = find_video_mode_idx(xres, yres, 0);
	}

	nmodes = video_mode_count();
	if(!(vmodes = malloc(nmodes * sizeof *vmodes))) {
		printf("failed to allocate video modes array (%d modes)\n", nmodes);
		return -1;
	}

	for(i=0; i<nmodes; i++) {
		video_mode_info(i, &vinf);
		vmodes[i] = vinf;
	}

	if(mode_idx >= 0) {
		if(!(fbptr = set_video_mode(vmodes[mode_idx].mode))) {
			printf("failed to set video mode: %x (%dx%d %dbpp)\n", mode_idx,
					vmodes[mode_idx].width, vmodes[mode_idx].height, vmodes[mode_idx].bpp);
			mode_idx = -1;
		} else {
			vmode = vmodes[mode_idx];
			printf("video mode: %x (%dx%d %dbpp)\n", vmode.mode, vmode.width,
					vmode.height, vmode.bpp);
		}
	}

	if(mode_idx == -1) {
		qsort(vmodes, nmodes, sizeof *vmodes, modecmp);

		for(i=0; i<nmodes; i++) {
			if((fbptr = set_video_mode(vmodes[i].mode))) {
				vmode = vmodes[i];
				printf("video mode: %x (%dx%d %dbpp)\n", vmode.mode, vmode.width,
						vmode.height, vmode.bpp);
				break;
			}
		}
		if(i >= nmodes) {
			printf("failed to find a suitable video mode\n");
			return -1;
		}
	}
	free(vmodes);

	return 0;
}

static void draw(void)
{
	memset(fbptr, 0x80, vmode.width * vmode.height * vmode.bpp / 8);

	win.draw(&ggfx, &win);
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
