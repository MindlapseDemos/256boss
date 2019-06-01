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
#include "drawtext.h"

#define DATA_PATH	"/.data/"

static void setup_video(void);
static void draw(void);
static void glyphdraw(struct dtx_vertex *v, int vcount, struct dtx_pixmap *pixmap, void *cls);

static unsigned char *vmem = (unsigned char*)0xa0000;
static struct dtx_font *font;
static int fontsz;

void splash_screen(void)
{
	struct dtx_glyphmap *gmap;

	setup_video();

	if(!(font = dtx_open_font_glyphmap(DATA_PATH "fontlow.glyphmap")) ||
			!(gmap = dtx_get_glyphmap(font, 0))) {
		set_vga_mode(3);
		printf("Failed to load font: " DATA_PATH "%s\n");
		return;
	}
	fontsz = dtx_get_glyphmap_ptsize(gmap);
	dtx_use_font(font, fontsz);
	dtx_target_user(glyphdraw, 0);

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

	dtx_position(50, 100);
	dtx_printf("hello world!");
	dtx_flush();
}

static void glyphdraw(struct dtx_vertex *v, int vcount, struct dtx_pixmap *pixmap, void *cls)
{
	int i, j, k, num = vcount / 6;
	int x, y, w, h;
	int tx, ty;
	unsigned char *fbptr;
	unsigned char *sptr;

	for(i=0; i<num; i++) {
		tx = v[0].s * pixmap->width;
		ty = v[2].t * pixmap->height;
		w = v[2].s * pixmap->width - tx;
		h = v[0].t * pixmap->width - ty;

		x = v[0].x;
		y = v[0].y - h;

		v += 6;
		if(w <= 0 || h <= 0) continue;

		fbptr = vmem + y * 320 + x;
		sptr = pixmap->pixels + ty * pixmap->width + tx;

		for(j=0; j<h; j++) {
			for(k=0; k<w; k++) {
				if(sptr[k]) {
					fbptr[k] = sptr[k];
				}
			}
			fbptr += 320;
			sptr += pixmap->width;
		}
	}
}
