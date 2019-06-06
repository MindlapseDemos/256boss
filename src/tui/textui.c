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
#include <time.h>
#include <dirent.h>
#include "textui.h"
#include "video.h"
#include "contty.h"
#include "keyb.h"
#include "asmops.h"

#define NCOLS	80
#define NROWS	25

#define CHAR_COL(c, fg, bg) \
	((uint16_t)(c) | ((uint16_t)(fg) << 8) | ((uint16_t)(bg) << 12))

#define CHAR_ATTR(c, a) \
	((uint16_t)(c) | ((uint16_t)(a) << 8))

#define ATTR(fg, bg)	((fg) | ((bg) << 4))

#define ATTR_BG			ATTR(GREY, BLACK)
#define ATTR_TOPBAR		ATTR(LTGREY, RED)
#define ATTR_STBAR		ATTR(BLACK, CYAN)

#define COL_FSVIEW_BG		BLACK
#define COL_FSVIEW_FRM		YELLOW
#define COL_FSVIEW_FILE		LTGREY
#define COL_FSVIEW_DIR		WHITE

static void draw_topbar(void);
static void draw_clock(void);
static void draw_statusbar(void);
static void draw_dirview(int x, int y, int w, int h);

static void fill_rect(int x, int y, int w, int h, uint16_t c);
static void draw_frame(const char *title, int x, int y, int w, int h, unsigned char fg, unsigned char bg);

static uint16_t *vmem = (uint16_t*)0xb8000;


int textui(void)
{
	unsigned char orig_attr;

	set_vga_mode(3);
	orig_attr = con_getattr();
	con_show_cursor(0);
	con_clear();	/* to reset scrolling */

	memset16(vmem, CHAR_ATTR(G_CHECKER, ATTR_BG), NCOLS * NROWS);

	draw_topbar();
	draw_dirview(2, 1, 60, 23);
	draw_statusbar();

	for(;;) {
		int c;

		halt_cpu();
		while((c = kb_getkey()) >= 0) {
			switch(c) {
			case KB_F8:
				goto end;
			}
		}

		draw_clock();
	}

end:
	con_setattr(orig_attr);
	con_clear();
	con_show_cursor(1);
	return 0;
}

static void draw_topbar(void)
{
	con_setattr(ATTR_TOPBAR | FG_BRIGHT);
	memset16(vmem, CHAR_ATTR(' ', ATTR_TOPBAR), NCOLS);
	con_printf(0, 0, "256boss");

	draw_clock();
}

static void draw_clock(void)
{
	time_t t;
	struct tm *tm;

	t = time(0);
	tm = localtime(&t);

	con_setattr(ATTR_TOPBAR);
	con_printf(73, 0, "[%02d:%02d]", tm->tm_hour, tm->tm_min);
}

static void draw_statusbar(void)
{
	con_setattr(ATTR_STBAR);
	memset16(vmem + NCOLS * (NROWS - 1), CHAR_ATTR(' ', ATTR_STBAR), NCOLS);
	con_printf(0, NROWS - 1, "status bar ...");
}

static void draw_dirview(int x, int y, int w, int h)
{
	DIR *dir;
	struct dirent *dent;
	int curline;
	int namecol_len, sizecol_len;
	char buf[80];
	unsigned char attr_file, attr_dir;

	attr_file = ATTR(COL_FSVIEW_FILE, COL_FSVIEW_BG);
	attr_dir = ATTR(COL_FSVIEW_DIR, COL_FSVIEW_BG);

	sizecol_len = 8;
	namecol_len = w - sizecol_len;

	fill_rect(x, y, w, h, CHAR_COL(' ', COL_FSVIEW_FILE, COL_FSVIEW_BG));
	draw_frame("foo", x, y, w, h, COL_FSVIEW_FRM, COL_FSVIEW_BG);

	if(!(dir = opendir("."))) {
		return;
	}

	curline = y + 1;
	while((dent = readdir(dir)) && curline < y + h - 1) {
		con_setattr(attr_file);
		con_printf(x + 1, curline, "%s", dent->d_name);
		curline++;
	}
	closedir(dir);
}

static void fill_rect(int x, int y, int w, int h, uint16_t c)
{
	int i;
	uint16_t *fb = vmem + y * NCOLS + x;

	for(i=0; i<h; i++) {
		memset16(fb, c, w);
		fb += NCOLS;
	}
}

static void draw_frame(const char *title, int x, int y, int w, int h, unsigned char fg, unsigned char bg)
{
	int i, tlen, maxtlen;
	char tbuf[80];
	uint16_t *ptr;
	uint16_t attr = ATTR(fg, bg);

	if(w < 4) w = 4;
	if(w > 80) w = 80;
	maxtlen = w - 4;

	tlen = strlen(title);
	if(tlen > maxtlen) {
		memcpy(tbuf, title, maxtlen - 3);
		memcpy(tbuf + maxtlen - 3, "...", 3);
		tbuf[maxtlen] = 0;
		title = tbuf;
		tlen = maxtlen;
	}

	/* draw top frame + title */
	con_setattr(attr);
	ptr = vmem + y * NCOLS + x;
	ptr[0] = CHAR_ATTR(G_UL_HDBL, attr);
	con_printf(x + 1, y, " %s ", title);
	memset16(ptr + tlen + 3, CHAR_ATTR(G_HDBL, attr), w - tlen - 4);
	ptr[w - 1] = CHAR_ATTR(G_UR_HDBL, attr);

	ptr += NCOLS;
	/* draw left/right frame */
	for(i=0; i<h - 2; i++) {
		ptr[0] = ptr[w - 1] = CHAR_ATTR(G_VLINE, attr);
		ptr += NCOLS;
	}

	/* draw bottom frame */
	ptr[0] = CHAR_ATTR(G_LL_CORNER, attr);
	ptr[w - 1] = CHAR_ATTR(G_LR_CORNER, attr);
	memset16(ptr + 1, CHAR_ATTR(G_HLINE, attr), w - 2);
}
