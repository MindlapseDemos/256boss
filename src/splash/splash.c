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
#include "data.h"
#include "psys.h"

static void setup_video(void);
static void draw(long msec);
static void draw_tunnel(long msec);
static int precalc_tunnel(void);
static void draw_psys(struct emitter *psys, long msec);
static void setup_psys_cmap(long msec);


static unsigned char *fb;
static unsigned char *vmem = (unsigned char*)0xa0000;
static struct image img_ui, img_tex;
static long start_ticks;
static struct cmapent tunpal[256];

enum {ST_TUNNEL, ST_FLAME} state;

#define TUN_FADEOUT_START	10000
#define TUN_FADEOUT_DUR		1000
#define TUN_DUR				(TUN_FADEOUT_START + TUN_FADEOUT_DUR)

#define FLAME_DUR			4000
#define FLAME_FADEIN_DUR	500

#define SPLASH_DUR			(TUN_DUR + FLAME_DUR)


#define HEADER_HEIGHT	17
#define FX_HEIGHT		(200 - HEADER_HEIGHT)
#define FX_TEX_SIZE		128
#define FX_TEX_PITCH	(FX_TEX_SIZE << 2)

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

static struct emitter psys;


void splash_screen(void)
{
	long msec;

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
	img_tex.width = img_tex.height;

	create_emitter(&psys, 8192);
	psys.spawn_rate = SPAWN_PER_SEC(4000, 0);
	psys.plife = 1000;
	psys.damping = 1.0;
	psys.grav_y = -20;
	psys.x = 160;
	psys.y = 100;
	psys.x_range = 5;
	psys.y_range = 5;
	psys.plife_range = 500;
	psys.pcol_start = 63;
	psys.pcol_end = 0;
	psys.curve_cv = (float*)curve_256;
	psys.curve_num_cv = sizeof curve_256 / sizeof *curve_256;
	psys.curve_scale_x = 45.0f;
	psys.curve_scale_y = -45.0f;

	while(kb_getkey() >= 0);	/* empty any input queues */

	setup_video();
	start_ticks = nticks;
	msec = 0;

	while(msec < SPLASH_DUR) {
		halt_cpu();
		if(kb_getkey() >= 0) {
			break;
		}

		msec = TICKS_TO_MSEC(nticks - start_ticks);
		draw(msec);
	}

end:
	textui();
	con_scr_enable();
	set_vga_mode(3);

	destroy_emitter(&psys);
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
		tunpal[i].r = col->r;
		tunpal[i].g = col->g;
		tunpal[i].b = col->b;
		col++;
	}

	col = img_tex.cmap;
	for(i=0; i<img_tex.cmap_ncolors; i++) {
		for(j=0; j<FX_FOG_LEVELS; j++) {
			int r = (int)col->r * (FX_FOG_LEVELS - j) / FX_FOG_LEVELS;
			int g = (int)col->g * (FX_FOG_LEVELS - j) / FX_FOG_LEVELS;
			int b = (int)col->b * (FX_FOG_LEVELS - j) / FX_FOG_LEVELS;
			int idx = i + FX_PAL_OFFS + FX_PAL_SIZE * j;
			set_pal_entry(idx, r, g, b);
			tunpal[idx].r = r;
			tunpal[idx].g = g;
			tunpal[idx].b = b;
		}
		col++;
	}
}

static void draw(long msec)
{
	if(msec < TUN_DUR) {
		draw_tunnel(msec);
	} else if(msec - TUN_DUR < FLAME_DUR) {
		float t;

		msec -= TUN_DUR;
		if(msec <= FLAME_FADEIN_DUR) {
			setup_psys_cmap(msec);
		}
		memset(fb, 0, 64000);

		t = (float)msec / (float)FLAME_DUR * 3.0f;
		if(t > 1.0f) t = 1.0f;
		psys.curve_tend = t;
		psys.spawn_rate = SPAWN_PER_SEC((long)(t * 3000) + 200, 0);

		update_psys(&psys, msec);
		draw_psys(&psys, msec);
	}

	wait_vsync();
	memcpy(vmem, fb, 64000);
}

#define FADEIN_DUR	3000
#define EASEIN_START	2000
#define EASEIN_DUR	2000
#define TUN_FLASH_R	221
#define TUN_FLASH_G	234
#define TUN_FLASH_B	239
static void draw_tunnel(long msec)
{
	int i, j, tx, ty, xoffs, yoffs;
	struct tunnel *tun;
	unsigned char *pptr;
	float shake, t;
	int blursel, bluroffs;
	long anmt;

	if(msec < FADEIN_DUR) {
		for(i=FX_PAL_OFFS; i<256; i++) {
			int r = tunpal[i].r * msec / FADEIN_DUR;
			int g = tunpal[i].g * msec / FADEIN_DUR;
			int b = tunpal[i].b * msec / FADEIN_DUR;
			set_pal_entry(i, r, g, b);
		}
	} else if(msec >= TUN_FADEOUT_START && msec < TUN_FADEOUT_START + TUN_FADEOUT_DUR) {
		for(i=0; i<256; i++) {
			int tm = msec - TUN_FADEOUT_START;
			int r = (int)tunpal[i].r + (TUN_FLASH_R - (int)tunpal[i].r) * tm / TUN_FADEOUT_DUR;
			int g = (int)tunpal[i].g + (TUN_FLASH_G - (int)tunpal[i].g) * tm / TUN_FADEOUT_DUR;
			int b = (int)tunpal[i].b + (TUN_FLASH_B - (int)tunpal[i].b) * tm / TUN_FADEOUT_DUR;
			set_pal_entry(i, r, g, b);
		}
	}

	anmt = msec * msec / 3000;

	blursel = (msec - 500) / 1800;
	if(blursel < 0) blursel = 0;
	if(blursel > 3) blursel = 3;
	bluroffs = blursel * FX_TEX_SIZE;

	t = (float)msec / 1000.0f;
	shake = (t - 2) * 0.08;
	if(shake < 0.0f) shake = 0.0f;
	xoffs = (int)(cos(t * 3.0) * shake * (TUN_PAN_XSZ / 2) + (TUN_PAN_XSZ / 2));
	yoffs = (int)(sin(t * 4.0) * shake * (TUN_PAN_YSZ / 2) + (TUN_PAN_YSZ / 2));

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

				//tx = (tx + anmt / 2) >> 3;
				tx >>= 3;
				ty = (ty + anmt) >> 3;

				tx &= FX_TEX_SIZE - 1;
				ty &= FX_TEX_SIZE - 1;

				*pptr++ = img_tex.pixels[bluroffs + ty * FX_TEX_PITCH + tx] + tun->fog * FX_PAL_SIZE;
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

static void draw_psys(struct emitter *psys, long msec)
{
	int i;
	struct particle *p;

	p = psys->plist;
	for(i=0; i<psys->pmax; i++) {
		if(p->life > 0) {
			int x = p->x;
			int y = p->y;
			if(x >= 0 && y >= 0 && x < 320 && y < 200) {
				unsigned char *pptr = fb + y * 320 + x;
				int val, pcol = p->col / 3;
				/*
				int pix = *pptr + p->col;
				if(pix > 255) pix = 255;
				*pptr = pix;
				*/
				val = *pptr + pcol;
				*pptr = val > 63 ? 63 : val;
				pcol >>= 1;
				val = pptr[1] + pcol;
				pptr[1] = val > 63 ? 63 : val;
				val = pptr[-1] + pcol;
				pptr[-1] = val > 63 ? 63 : val;
				val = pptr[320] + pcol;
				pptr[320] = val > 63 ? 63 : val;
				val = pptr[-320] + pcol;
				pptr[-320] = val > 63 ? 63 : val;
			}
		}
		p++;
	}
}

static void setup_psys_cmap(long msec)
{
	int i;

	for(i=0; i<64; i++) {
		int r = 255 + (firepal[i][0] - 255) * msec / FLAME_FADEIN_DUR;
		int g = 255 + (firepal[i][1] - 255) * msec / FLAME_FADEIN_DUR;
		int b = 255 + (firepal[i][2] - 255) * msec / FLAME_FADEIN_DUR;
		set_pal_entry(i, r, g, b);
		//set_pal_entry(i, firepal[i][0], firepal[i][1], firepal[i][2]);
	}
}
