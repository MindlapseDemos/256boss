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
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>
#include "asmops.h"
#include "keyb.h"
#include "video.h"
#include "image.h"
#include "contty.h"
#include "timer.h"
#include "tui/textui.h"
#include "gui/gfxui.h"
#include "datapath.h"

static void setup_video(void);
static void draw(void);


static unsigned char *fb;
static unsigned char *vmem = (unsigned char*)0xa0000;
static struct image img_ui;
static unsigned long start_ticks;


void splash_screen(void)
{
	if(init_datapath() == -1) {
		printf("splash_screen: failed to locate the data dir\n");
	}

	if(!(fb = malloc(64000))) {
		printf("splash_screen: failed to allocate back buffer\n");
		goto end;
	}

	if(load_image(&img_ui, datafile("256boss.png")) == -1 || img_ui.bpp != 8) {
		printf("splash_screen: failed to load UI image\n");
		goto end;
	}

	setup_video();
	start_ticks = nticks;

	while(kb_getkey() >= 0);	/* drain the input queue */

	for(;;) {
		int c;

		halt_cpu();
		while((c = kb_getkey()) >= 0) {
			goto end;
		}

		draw();
	}

end:
	textui();
	con_scr_enable();
	set_vga_mode(3);
/*err:*/
	free(fb);
	free(img_ui.pixels);
}

static void setup_video(void)
{
	/*
	int i, j;
	struct cmapent *col;
	*/

	con_clear();	/* this has the side-effect of resetting CRTC scroll regs */
	set_vga_mode(0x13);
	con_scr_disable();

	/*
	col = img_ui.cmap;
	for(i=0; i<img_ui.cmap_ncolors; i++) {
		set_pal_entry(i, col->r, col->g, col->b);
		col++;
	}

	col = img_tex.cmap;
	for(i=0; i<img_tex.cmap_ncolors; i++) {
		for(j=0; j<FX_FOG_LEVELS; j++) {
			int r = (int)col->r * (FX_FOG_LEVELS - j) / FX_FOG_LEVELS;
			int g = (int)col->g * (FX_FOG_LEVELS - j) / FX_FOG_LEVELS;
			int b = (int)col->b * (FX_FOG_LEVELS - j) / FX_FOG_LEVELS;
			set_pal_entry(i + FX_PAL_OFFS + FX_PAL_SIZE * j, r, g, b);
		}
		col++;
	}
	*/
}

static void draw(void)
{
	unsigned long msec = MSEC_TO_TICKS(nticks - start_ticks);


	wait_vsync();
	memcpy(vmem, fb, 64000);
}
