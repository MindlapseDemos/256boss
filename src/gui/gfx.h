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
#ifndef GUI_GFX_H_
#define GUI_GFX_H_

#include <inttypes.h>

struct gui_image {
	void *pixels;
	int width, height, bpp;
	int pitch;
	unsigned int rshift, rmask;
	unsigned int gshift, gmask;
	unsigned int bshift, bmask;
};

struct gui_gfx;

struct gui_draw {
	void (*clear)(struct gui_gfx *g);
	void (*putpixel)(struct gui_gfx *g, int x, int y);
	void (*rect)(struct gui_gfx *g, int x, int y, int w, int h);
	void (*hline)(struct gui_gfx *g, int x, int y, int len);
	void (*vline)(struct gui_gfx *g, int x, int y, int len);
	void (*line)(struct gui_gfx *g, int x0, int y0, int x1, int y1);
	void (*blit)(struct gui_gfx *g, int x, int y, struct gui_image *img);
};

struct gui_gfx {
	struct gui_image fb;
	struct gui_draw draw;

	uint32_t color;
};

void gui_setgfx(struct gui_gfx *g);
struct gui_gfx *gui_getgfx(void);

void gui_framebuffer(void *pix, int w, int h, int pitch);
void gui_pixelformat(int bpp, int rbits, int gbits, int bbits);

#endif	/* GUI_GFX_H_ */
