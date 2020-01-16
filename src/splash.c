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
static void draw_tunnel(unsigned long msec);
static int precalc_tunnel(void);


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
} *tunlut;


void splash_screen(void)
{
	if(init_datapath() == -1) {
		printf("splash_screen: failed to locate the data dir\n");
	}

	if(!(fb = malloc(64000))) {
		printf("splash_screen: failed to allocate back buffer\n");
		goto end;
	}

	tunlut = 0;
	img_ui.pixels = img_tex.pixels = 0;

	if(precalc_tunnel() == -1) {
		goto end;
	}

	if(load_image(&img_ui, datafile("256boss.png")) == -1 || img_ui.bpp != 8) {
		printf("splash_screen: failed to load UI image\n");
		goto end;
	}
	if(load_image(&img_tex, datafile("sstex2.png")) == -1 || img_tex.bpp != 8) {
		printf("splash_screen: failed to load texture\n");
		goto end;
	}
	image_color_offset(&img_tex, FX_PAL_OFFS);

	while(kb_getkey() >= 0);	/* empty any input queues */

	setup_video();
	start_ticks = nticks;

	for(;;) {
		halt_cpu();
		if(kb_getkey() >= 0) {
			break;
		}
		draw();
	}

end:
	textui();
	con_scr_enable();
	set_vga_mode(3);

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
	unsigned long msec = TICKS_TO_MSEC(nticks - start_ticks);

	draw_tunnel(msec);

	wait_vsync();
	memcpy(vmem, fb, 64000);
}

static float smoothstep(float a, float b, float x)
{
	if(x < a) return 0.0f;
	if(x >= b) return 1.0f;

	x = (x - a) / (b - a);
	return x * x * (3.0f - 2.0f * x);
}

#define EASEIN_END	2000
static void draw_tunnel(unsigned long msec)
{
	int i, j, tx, ty, xoffs, yoffs;
	struct tunnel *tun;
	unsigned char *pptr;
	float t;

	if(msec < EASEIN_END) {
		float x;
		x = smoothstep(0.0f, EASEIN_END, (float)msec);
		t = x * x * (float)EASEIN_END;
		msec = (unsigned long)t;
		t /= 1000.0f;
	} else {
		t = (float)msec / 1000.0f;
	}
	xoffs = (int)(/*cos(t * 3.0)*/0 * (TUN_PAN_XSZ / 2) + (TUN_PAN_XSZ / 2));
	yoffs = (int)(/*sin(t * 4.0)*/0 * (TUN_PAN_YSZ / 2) + (TUN_PAN_YSZ / 2));

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

				tx = (tx + msec / 2) >> 3;
				ty = (ty + msec) >> 3;

				tx &= FX_TEX_SIZE - 1;
				ty &= FX_TEX_SIZE - 1;
				*pptr++ = img_tex.pixels[ty * FX_TEX_PITCH + tx] + tun->fog * FX_PAL_SIZE;
			}
			tun++;
		}
		tun += TUN_WIDTH - 320;
	}
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
