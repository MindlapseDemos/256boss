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

#define DATA_PATH	"/.data/"

void ssfontbig(void *fb, int x, int y, int g);

static void setup_video(void);
static void draw(void);
static void draw_menu(unsigned long msec);
static void draw_tunnel(unsigned long msec);
static int draw_glyph_big(int x, int y, int c);
static int precalc_tunnel(void);
/*static int precalc_floor(void);*/

static void draw_rect(unsigned char *fb, int cx, int cy, int r, int col);
static void draw_hline(unsigned char *fb, int x, int y, int sz, int col);
static void draw_vline(unsigned char *fb, int x, int y, int sz, int col);


static unsigned char *fb;
static unsigned char *vmem = (unsigned char*)0xa0000;
static struct image img_ui, img_tex;
static unsigned long start_ticks;


#define HEADER_HEIGHT	17
#define FX_HEIGHT		(200 - HEADER_HEIGHT)
#define FX_TEX_SIZE		128
#define FX_TEX_PITCH	FX_TEX_SIZE

#define FX_PAL_OFFS		64
#define FX_PAL_SIZE		32
#define FX_FOG_LEVELS	6

#define FX_TEX_USCALE	0.5
#define FX_TEX_VSCALE	0.5

#define TUN_WIDTH		450
#define TUN_HEIGHT		300
#define TUN_PAN_XSZ		(TUN_WIDTH - 320)
#define TUN_PAN_YSZ		(TUN_HEIGHT - FX_HEIGHT)
struct tunnel {
	unsigned short x, y;
	unsigned char fog;
} *tunlut/*, *floorlut*/;


void splash_screen(void)
{
	if(!(fb = malloc(64000))) {
		printf("splash_screen: failed to allocate back buffer\n");
		return;
	}

	tunlut = 0;
	img_ui.pixels = img_tex.pixels = 0;

	if(precalc_tunnel() == -1) {
		goto err;
	}
	/*
	if(precalc_floor() == -1) {
		goto err;
	}
	*/

	if(load_image(&img_ui, DATA_PATH "256boss.png") == -1 || img_ui.bpp != 8) {
		printf("splash_screen: failed to load UI image\n");
		goto err;
	}
	if(load_image(&img_tex, DATA_PATH "sstex2.png") == -1 || img_tex.bpp != 8) {
		printf("splash_screen: failed to load texture\n");
		goto err;
	}
	image_color_offset(&img_tex, FX_PAL_OFFS);

	setup_video();
	start_ticks = nticks;

	for(;;) {
		int c;

		halt_cpu();
		while((c = kb_getkey()) >= 0) {
			switch(c) {
			case KB_F2:
				textui();
				goto end;

			case KB_F8:
				goto end;
			}
		}

		draw();
	}

end:
	con_scr_enable();
	set_vga_mode(3);
err:
	free(fb);
	free(tunlut);
	free(img_ui.pixels);
	free(img_tex.pixels);
}

static void setup_video(void)
{
	int i, j;
	struct cmapent *col;

	con_clear();	/* this has the side-effect of resetting CRTC scroll regs */
	set_vga_mode(0x13);
	con_scr_disable();

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
}

static void draw(void)
{
	unsigned long msec = MSEC_TO_TICKS(nticks - start_ticks);

	draw_tunnel(msec);
	draw_menu(msec);

	wait_vsync();
	memcpy(vmem, fb, 64000);
}

static const char *labels[] = {
	"TEXT UI",
	"VGA GUI",
	"HIRES GUI"
};

static void draw_menu(unsigned long msec)
{
	int i, x, y, max_chars, max_labels;
	int sz = msec;
	const char *tx;

	if(sz > 20) sz = 20;
	max_chars = msec / 8;
	max_labels = msec / 16;
	if(max_labels > 3) max_labels = 3;

	for(i=0; i<max_labels; i++) {
		x = 30;
		y = 60 * i + 50;
		draw_rect(fb, x, y, sz, 63);

		tx = labels[i];
		while(*tx) {
			if(tx - labels[i] >= max_chars) break;
			x += draw_glyph_big(x + 22, y - 8, *tx++);
		}
	}
}

static void draw_tunnel(unsigned long msec)
{
	int i, j, tx, ty, xoffs, yoffs;
	struct tunnel *tun;
	unsigned char *pptr;
	float t;

	t = (float)msec / 1000.0f;
	xoffs = (int)(cos(t * 3.0) * (TUN_PAN_XSZ / 2) + (TUN_PAN_XSZ / 2));
	yoffs = (int)(sin(t * 4.0) * (TUN_PAN_YSZ / 2) + (TUN_PAN_YSZ / 2));

	memcpy(fb, img_ui.pixels, HEADER_HEIGHT * 320);

	tun = tunlut + yoffs * TUN_WIDTH + xoffs;
	pptr = fb + HEADER_HEIGHT * 320;
	for(i=0; i<FX_HEIGHT; i++) {
		for(j=0; j<320; j++) {
			if(tun->fog >= FX_FOG_LEVELS) {
				*pptr++ = 0;
			} else {
				tx = ((int)tun->x * FX_TEX_SIZE) >> 10;
				ty = ((int)tun->y * FX_TEX_SIZE) >> 12;

				tx = (tx + msec * 4) >> 3;
				ty = (ty + msec * 8) >> 3;

				tx &= FX_TEX_SIZE - 1;
				ty &= FX_TEX_SIZE - 1;
				*pptr++ = img_tex.pixels[ty * FX_TEX_PITCH + tx] + tun->fog * FX_PAL_SIZE;
			}
			tun++;
		}
		tun += TUN_WIDTH - 320;
	}
}

static int gstart[] = {
	2, 0, 0, 0, 1, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};
static int gwidth[] = {
	16, 16, 16, 16, 12, 16, 15, 16, 6, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 13, 14, 14, 16, 14, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16, 16, 16
};

static int draw_glyph_big(int x, int y, int c)
{
	int idx = -1;

	if(c >= 'A' && c <= 'Z') {
		idx = c - 'A';
	} else if(c >= '1' && c <= '9') {
		idx = c - '1' + 26;
	} else if(c == '0') {
		idx = 35;
	} else {
		return 8;
	}

	ssfontbig(fb, x - gstart[idx], y, idx);
	return gwidth[idx];
}

#define TUN_ASPECT	((float)TUN_WIDTH / (float)TUN_HEIGHT)
static int precalc_tunnel(void)
{
	int i, j;
	struct tunnel *tun;

	if(!(tunlut = malloc(TUN_WIDTH * TUN_HEIGHT * sizeof *tunlut))) {
		printf("failed to allocate tunnel buffer\n");
		return -1;
	}
	tun = tunlut;

	for(i=0; i<TUN_HEIGHT; i++) {
		float dy = 2.0f * (float)i / (float)TUN_HEIGHT - 1.0f;
		for(j=0; j<TUN_WIDTH; j++) {
			float dx = (2.0f * (float)j / (float)TUN_WIDTH - 1.0f) * TUN_ASPECT;
			float tu = atan2(dy, dx) / M_PI * 0.5 + 0.5;
			float r = sqrt(dx * dx + dy * dy);
			float tv = r == 0.0f ? 65536.0f : 1.0f / r;

			float dither_tv = tv + (0.8 * ((float)rand() / (float)RAND_MAX) - 0.4);
			int fog = (int)(dither_tv * 1.15 - 0.25);
			if(fog < 0) fog = 0;

			tun->x = (unsigned short)(tu * 65536.0f * FX_TEX_USCALE);
			tun->y = (unsigned short)(tv * 65536.0f * FX_TEX_VSCALE);

			tun->fog = fog >= FX_FOG_LEVELS ? FX_FOG_LEVELS : fog;
			tun++;
		}
	}

	return 0;
}

/*
static int precalc_floor(void)
{
	int i, j;
	struct tunnel *flut;

	if(!(floorlut = malloc(TUN_WIDTH * TUN_HEIGHT * sizeof *floorlut))) {
		printf("failed to allocate floor buffer\n");
		return -1;
	}
	flut = floorlut;

	for(i=0; i<TUN_HEIGHT; i++) {
		for(j=0; j<TUN_WIDTH; j++) {
			float x = (float)j / TUN_WIDTH - 0.5f;
			float z = (float)(i + 40) / TUN_HEIGHT;
			float tv = 1.0 / z;
			float tu = x * 0.4 / z;

			float dither_tv = tv + (0.8 * ((float)rand() / (float)RAND_MAX) - 0.4);
			int fog = (int)(dither_tv * 1.15 - 0.25);
			if(fog < 0) fog = 0;

			flut->x = (unsigned short)(tu * 65536.0f * FX_TEX_USCALE);
			flut->y = (unsigned short)(tv * 65536.0f * FX_TEX_VSCALE);

			flut->fog = fog >= FX_FOG_LEVELS ? FX_FOG_LEVELS : fog;
			flut++;
		}
	}

	return 0;
}
*/

static void draw_rect(unsigned char *fb, int cx, int cy, int r, int col)
{
	int x0, y0, x1, y1;

	x0 = cx - r;
	x1 = cx + r;
	y0 = cy - r;
	y1 = cy + r;

	draw_hline(fb, x0, y0, x1 - x0, col);
	draw_hline(fb, x0, y1 - 1, x1 - x0, col);
	draw_vline(fb, x0, y0, y1 - y0, col);
	draw_vline(fb, x1 - 1, y0, y1 - y0, col);
}


static void draw_hline(unsigned char *fb, int x, int y, int sz, int col)
{
	if(y < 0 || y >= 200) return;
	if(x < 0) {
		sz += x;
		x = 0;
	}
	if(x + sz >= 320) {
		sz = 320 - x;
	}
	memset(fb + y * 320 + x, col, sz);
}

static void draw_vline(unsigned char *fb, int x, int y, int sz, int col)
{
	int i;

	if(x < 0 || x >= 320) return;
	if(y < 0) {
		sz += y;
		y = 0;
	}
	if(y + sz >= 200) {
		sz = 200 - y;
	}

	fb += y * 320 + x;

	for(i=0; i<sz; i++) {
		*fb = col;
		fb += 320;
	}
}
