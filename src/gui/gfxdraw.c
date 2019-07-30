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
#include <string.h>
#include "gfxdraw.h"

static void clear8(struct gui_gfx *g);
static void clear16(struct gui_gfx *g);
static void clear24(struct gui_gfx *g);
static void clear32(struct gui_gfx *g);

static void putpixel8(struct gui_gfx *g, int x, int y);
static void putpixel16(struct gui_gfx *g, int x, int y);
static void putpixel24(struct gui_gfx *g, int x, int y);
static void putpixel32(struct gui_gfx *g, int x, int y);

static void rect8(struct gui_gfx *g, int x, int y, int w, int h);
static void rect16(struct gui_gfx *g, int x, int y, int w, int h);
static void rect24(struct gui_gfx *g, int x, int y, int w, int h);
static void rect32(struct gui_gfx *g, int x, int y, int w, int h);

static void hline8(struct gui_gfx *g, int x, int y, int len);
static void hline16(struct gui_gfx *g, int x, int y, int len);
static void hline24(struct gui_gfx *g, int x, int y, int len);
static void hline32(struct gui_gfx *g, int x, int y, int len);

static void vline8(struct gui_gfx *g, int x, int y, int len);
static void vline16(struct gui_gfx *g, int x, int y, int len);
static void vline24(struct gui_gfx *g, int x, int y, int len);
static void vline32(struct gui_gfx *g, int x, int y, int len);

static void line8(struct gui_gfx *g, int x0, int y0, int x1, int y1);
static void line16(struct gui_gfx *g, int x0, int y0, int x1, int y1);
static void line24(struct gui_gfx *g, int x0, int y0, int x1, int y1);
static void line32(struct gui_gfx *g, int x0, int y0, int x1, int y1);

static void blit8(struct gui_gfx *g, int x, int y, struct gui_image *img);
static void blit16(struct gui_gfx *g, int x, int y, struct gui_image *img);
static void blit24(struct gui_gfx *g, int x, int y, struct gui_image *img);
static void blit32(struct gui_gfx *g, int x, int y, struct gui_image *img);

struct gui_draw draw_cmap8 = {
	clear8,
	putpixel8,
	rect8,
	hline8,
	vline8,
	line8,
	blit8
};

struct gui_draw draw_rgb16 = {
	clear16,
	putpixel16,
	rect16,
	hline16,
	vline16,
	line16,
	blit16
};

struct gui_draw draw_rgb24 = {
	clear24,
	putpixel24,
	rect24,
	hline24,
	vline24,
	line24,
	blit24
};

struct gui_draw draw_rgb32 = {
	clear32,
	putpixel32,
	rect32,
	hline32,
	vline32,
	line32,
	blit32
};

static void clear8(struct gui_gfx *g)
{
	int i;
	unsigned char *pptr = g->fb.pixels;

	for(i=0; i<g->fb.height; i++) {
		memset(pptr, g->fb.width, g->color);
		pptr += g->fb.pitch;
	}
}

static void clear16(struct gui_gfx *g)
{
	int i;
	uint16_t *pptr = g->fb.pixels;

	for(i=0; i<g->fb.height; i++) {
		memset16(pptr, g->fb.width, g->color);
		pptr += g->fb.pitch / 2;
	}
}

static void clear24(struct gui_gfx *g)
{
	int i, j;
	unsigned char cr, cg, cb;
	unsigned char *pptr = g->fb.pixels;

	cr = g->color;
	cg = g->color >> 8;
	cb = g->color >> 16;

	for(i=0; i<g->fb.height; i++) {
		unsigned char *p = pptr;
		for(j=0; j<g->fb.width; j++) {
			*p++ = cr;
			*p++ = cg;
			*p++ = cb;
		}
		pptr += g->fb.pitch;
	}
}

static void clear32(struct gui_gfx *g)
{
	int i, j;
	unsigned char *pptr = g->fb.pixels;

	for(i=0; i<g->fb.height; i++) {
		unsigned char *p = pptr;
		for(j=0; j<g->fb.width; j++) {
			*p++ = g->color;
		}
		pptr += g->fb.pitch / 4;
	}
}


static void putpixel8(struct gui_gfx *g, int x, int y)
{
	unsigned char *p = g->fb.pixels;
	p[y * g->fb.pitch + x] = g->color;
}

static void putpixel16(struct gui_gfx *g, int x, int y)
{
	uint16_t *p = g->fb.pixels;
	p[y * g->fb.pitch / 2 + x] = g->color;
}

static void putpixel24(struct gui_gfx *g, int x, int y)
{
	unsigned char *p = (unsigned char*)g->fb.pixels + y * g->fb.pitch + x * 3;
	p[0] = g->color;
	p[1] = g->color >> 8;
	p[2] = g->color >> 16;
}

static void putpixel32(struct gui_gfx *g, int x, int y)
{
	uint32_t *p = g->fb.pixels;
	p[y * g->fb.pitch / 4 + x] = g->color;
}

#define BOUNDRECT(g, x, y, w, h) \
	do { \
		if((x) < 0) { \
			(w) += (x); \
			(x) = 0; \
		} \
		if((y) < 0) { \
			(h) += (y); \
			(y) = 0; \
		} \
		if((x) + (w) > (g)->fb.width) { \
			(w) = (g)->fb.width - (x); \
		} \
		if((y) + (h) > (g)->fb.height) { \
			(h) = (g)->fb.height - (y); \
		} \
	} while(0)

static void rect8(struct gui_gfx *g, int x, int y, int w, int h)
{
	int i;
	unsigned char *pptr;

	BOUNDRECT(g, x, y, w, h);

	pptr = (unsigned char*)g->fb.pixels + y * g->fb.pitch + x;
	for(i=0; i<h; i++) {
		memset(pptr, w, g->color);
		pptr += g->fb.pitch;
	}
}

static void rect16(struct gui_gfx *g, int x, int y, int w, int h)
{
	int i;
	uint16_t *pptr;

	BOUNDRECT(g, x, y, w, h);

	pptr = (uint16_t*)g->fb.pixels + y * g->fb.pitch / 2 + x;
	for(i=0; i<h; i++) {
		memset16(pptr, w, g->color);
		pptr += g->fb.pitch / 2;
	}
}

static void rect24(struct gui_gfx *g, int x, int y, int w, int h)
{
	int i, j;
	unsigned char cr, cg, cb;
	unsigned char *pptr;

	BOUNDRECT(g, x, y, w, h);

	cr = g->color;
	cg = g->color >> 8;
	cb = g->color >> 16;

	pptr = (unsigned char*)g->fb.pixels + y * g->fb.pitch + x * 3;
	for(i=0; i<h; i++) {
		unsigned char *p = pptr;
		for(j=0; j<w; j++) {
			*p++ = cr;
			*p++ = cg;
			*p++ = cb;
		}
		pptr += g->fb.pitch;
	}
}

static void rect32(struct gui_gfx *g, int x, int y, int w, int h)
{
	int i, j;
	uint32_t *pptr;

	BOUNDRECT(g, x, y, w, h);

	pptr = (uint32_t*)g->fb.pixels + y * g->fb.pitch / 4 + x;
	for(i=0; i<h; i++) {
		for(j=0; j<w; j++) {
			pptr[j] = g->color;
		}
		pptr += g->fb.pitch / 4;
	}
}


static void hline8(struct gui_gfx *g, int x, int y, int len)
{
}

static void hline16(struct gui_gfx *g, int x, int y, int len)
{
}

static void hline24(struct gui_gfx *g, int x, int y, int len)
{
}

static void hline32(struct gui_gfx *g, int x, int y, int len)
{
}


static void vline8(struct gui_gfx *g, int x, int y, int len)
{
}

static void vline16(struct gui_gfx *g, int x, int y, int len)
{
}

static void vline24(struct gui_gfx *g, int x, int y, int len)
{
}

static void vline32(struct gui_gfx *g, int x, int y, int len)
{
}


static void line8(struct gui_gfx *g, int x0, int y0, int x1, int y1)
{
}

static void line16(struct gui_gfx *g, int x0, int y0, int x1, int y1)
{
}

static void line24(struct gui_gfx *g, int x0, int y0, int x1, int y1)
{
}

static void line32(struct gui_gfx *g, int x0, int y0, int x1, int y1)
{
}


static void blit8(struct gui_gfx *g, int x, int y, struct gui_image *img)
{
}

static void blit16(struct gui_gfx *g, int x, int y, struct gui_image *img)
{
}

static void blit24(struct gui_gfx *g, int x, int y, struct gui_image *img)
{
}

static void blit32(struct gui_gfx *g, int x, int y, struct gui_image *img)
{
}
