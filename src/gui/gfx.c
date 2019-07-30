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
#include <assert.h>
#include "gfx.h"
#include "gfxdraw.h"
#include "panic.h"

static struct gui_gfx *gfx;

void gui_setgfx(struct gui_gfx *g)
{
	gfx = g;
}

struct gui_gfx *gui_getgfx(void)
{
	return gfx;
}

void gui_framebuffer(void *pix, int w, int h, int pitch)
{
	gfx->fb.pixels = pix;
	gfx->fb.width = w;
	gfx->fb.height = h;
	gfx->fb.pitch = pitch;
}

static unsigned int mask_bits(int n)
{
	int i;
	unsigned int mask = 0;
	for(i=0; i<n; i++) {
		mask |= 1 << n;
	}
	return mask;
}

void gui_pixelformat(int bpp, int rbits, int gbits, int bbits)
{
	assert(rbits + gbits + bbits <= bpp);
	gfx->fb.rshift = 0;
	gfx->fb.rmask = mask_bits(rbits);
	gfx->fb.gshift = rbits;
	gfx->fb.gmask = mask_bits(gbits) << rbits;
	gfx->fb.bshift = rbits + gbits;
	gfx->fb.bmask = mask_bits(bbits) << (rbits + gbits);

	switch(bpp) {
	case 8:
		gfx->draw = draw_cmap8;
		break;

	case 15:
	case 16:
		gfx->draw = draw_rgb16;
		break;

	case 24:
		gfx->draw = draw_rgb24;
		break;

	case 32:
		gfx->draw = draw_rgb32;
		break;

	default:
		panic("GUI graphics: unsupported pixel format (%d bpp %d/%d/%d)\n", bpp,
				rbits, gbits, bbits);
	}
}
