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
#include <limits.h>
#include <ctype.h>
#include <unistd.h>
#include <dirent.h>
#include "textui.h"
#include "video.h"
#include "contty.h"
#include "keyb.h"
#include "comloader.h"
#include "asmops.h"
#include "panic.h"
#include "fsview.h"
#include "util.h"

#define NCOLS	80
#define NROWS	25

#define FSVIEW_X	0
#define FSVIEW_Y	1
#define FSVIEW_COLS	50
#define FSVIEW_ROWS	23

#define SIZECOL_LEN	10

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
#define COL_FSVIEW_SELBAR	BROWN
#define COL_FSVIEW_FILE		LTGREY
#define COL_FSVIEW_DIR		WHITE
#define COL_FSVIEW_EXEC_256	LTGREEN
#define COL_FSVIEW_EXEC		LTRED

static void init_scr(void);
static int openfile(const char *path);
static void draw_topbar(void);
static void draw_clock(void);
static void draw_statusbar(void);
static void draw_dirview(int x, int y, int w, int h, int first, int last);

static void fill_rect(int x, int y, int w, int h, uint16_t c);
static void draw_frame(const char *title, int x, int y, int w, int h, unsigned char fg, unsigned char bg);
static void invalidate(int idx);

static void cancel_search(void);

static uint16_t *vmem = (uint16_t*)0xb8000;
static unsigned char orig_attr;

static int dirty_start = -1, dirty_end = -1;
static int dirty_status;

#define MAX_SEARCH_LEN	32
static char search[MAX_SEARCH_LEN + 1];
static int search_len;


static void init_scr(void)
{
	*search = 0;
	search_len = 0;

	set_vga_mode(3);
	con_show_cursor(0);
	con_clear();	/* to reset scrolling */

	memset16(vmem, CHAR_ATTR(G_CHECKER, ATTR_BG), NCOLS * NROWS);

	draw_topbar();
	draw_dirview(FSVIEW_X, FSVIEW_Y, FSVIEW_COLS, FSVIEW_ROWS, -1, -1);
	draw_statusbar();

	dirty_start = dirty_end = -1;
	dirty_status = 0;
}

#define INVALIDATE()	\
	do { \
		if(fsview.scroll != scr) { \
			invalidate(-1); \
		} else { \
			invalidate(sel); \
			invalidate(fsview.cursel); \
		} \
	} while(0)

int textui(void)
{
	int sel, scr, tmp;

	orig_attr = con_getattr();

	fsview.num_vis = FSVIEW_ROWS - 2;
	fsview.openfile = openfile;
	fsv_keep_vis(&fsview, fsview.cursel);

	init_scr();


	for(;;) {
		int c;

		halt_cpu();
		while((c = kb_getkey()) >= 0) {
			sel = fsview.cursel;
			scr = fsview.scroll;

			switch(c) {
			case KB_DOWN:
				if(fsv_sel_next(&fsview)) {
					INVALIDATE();
				}
				cancel_search();
				break;

			case KB_UP:
				if(fsv_sel_prev(&fsview)) {
					INVALIDATE();
				}
				cancel_search();
				break;

			case KB_HOME:
				if(fsv_sel_first(&fsview)) {
					INVALIDATE();
				}
				cancel_search();
				break;

			case KB_END:
				if(fsv_sel_last(&fsview)) {
					INVALIDATE();
				}
				cancel_search();
				break;

			case KB_PGDN:
				tmp = fsview.cursel + fsview.num_vis;
				fsv_sel(&fsview, tmp >= fsview.num_entries ? fsview.num_entries - 1 : tmp);
				fsview.scroll = fsview.cursel;
				INVALIDATE();
				cancel_search();
				break;

			case KB_PGUP:
				tmp = fsview.cursel - fsview.num_vis;
				fsv_sel(&fsview, tmp < 0 ? 0 : tmp);
				fsview.scroll = fsview.cursel;
				INVALIDATE();
				cancel_search();
				break;

			case '\b':
				if(search_len > 0) {
					search[--search_len] = 0;
					dirty_status = 1;
				} else {
					fsv_updir(&fsview);
					invalidate(-1);
					cancel_search();
				}
				break;

			case '\n':
				fsv_activate(&fsview);
				invalidate(-1);
				cancel_search();
				break;

			case 27:
				cancel_search();
				break;

			case KB_F8:
				goto end;

			default:
				if(isprint(c)) {
					if(search_len < MAX_SEARCH_LEN) {
						search[search_len++] = c;
						search[search_len] = 0;
						dirty_status = 1;

						if(fsv_sel_match(&fsview, search)) {
							INVALIDATE();
						}
					}
				}
			}
		}

		if(dirty_start != -1) {
			draw_dirview(FSVIEW_X, FSVIEW_Y, FSVIEW_COLS, FSVIEW_ROWS, dirty_start, dirty_end);
			dirty_start = dirty_end = -1;
		}
		if(dirty_status) {
			draw_statusbar();
			dirty_status = 0;
		}
		draw_clock();
	}

end:
	con_setattr(orig_attr);
	con_clear();
	con_show_cursor(1);
	return 0;
}

static int openfile(const char *path)
{
	if(has_suffix(path, ".com")) {
		if(load_com_binary(path) == -1) {
			return -1;
		}
		con_setattr(orig_attr);
		set_vga_mode(3);
		run_com_binary();
		init_scr();
		return 0;
	} else {
		/* TODO view file contents */
	}
	return -1;
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

	if(search_len > 0) {
		con_printf(0, NROWS - 1, "Search: %s", search);
	} else {
		con_printf(0, NROWS - 1, "Hint: type to search, esc to cancel, enter to activate");
	}
}

static void draw_dirview(int x, int y, int w, int h, int first, int last)
{
	int i, nlines, max_nlines, row, col;
	int namecol_len;
	char buf[PATH_MAX];
	unsigned char attr_file, attr_dir;
	struct fsview_dirent *eptr;
	int eidx;
	uint16_t *rowptr;

	attr_file = ATTR(COL_FSVIEW_FILE, COL_FSVIEW_BG);
	attr_dir = ATTR(COL_FSVIEW_DIR, COL_FSVIEW_BG);

	namecol_len = w - 2 - SIZECOL_LEN;
	max_nlines = h - 2;

	if(first == -1 || (first <= 0 && last >= fsview.num_entries - 1)) {
		first = 0;
		last = INT_MAX;

		fill_rect(x, y, w, h, CHAR_COL(' ', COL_FSVIEW_FILE, COL_FSVIEW_BG));
		draw_frame(getcwd(buf, PATH_MAX), x, y, w, h, COL_FSVIEW_FRM, COL_FSVIEW_BG);
	}

	nlines = fsview.num_entries - fsview.scroll;
	if(nlines > max_nlines) {
		nlines = max_nlines;
	}

	eidx = fsview.scroll;
	eptr = fsview.entries + fsview.scroll;

	row = y + 1;
	rowptr = vmem + row * NCOLS;

	for(i=0; i<max_nlines; i++) {
		if(eidx >= first && eidx <= last) {
			unsigned char attr = attr_dir;

			if(eidx < fsview.num_entries && eptr->type != DT_DIR) {
				attr = attr_file;
				if(has_suffix(eptr->name, ".com")) {
					if(eptr->size <= 256) {
						attr = (attr & 0xf0) | COL_FSVIEW_EXEC_256;
					} else {
						attr = (attr & 0xf0) | COL_FSVIEW_EXEC;
					}
				}
			}

			if(eidx == fsview.cursel) {
				attr = (attr & 0xf) | (COL_FSVIEW_SELBAR << 4);
			}
			con_setattr(attr);

			col = x + 1;
			row = y + 1 + i;
			memset16(rowptr + col, CHAR_ATTR(' ', attr), w - 2);

			if(eidx < fsview.num_entries) {
				con_printf(col, row, "%s", eptr->name);
				if(eptr->type == DT_DIR) {
					con_printf(col + namecol_len + 1, row, "<DIR>");
				} else {
					con_printf(col + namecol_len + 1, row, "%8s", fsizestr(eptr->size));
				}
			}

			attr = (attr & 0xf0) | COL_FSVIEW_FRM;
			rowptr[namecol_len] = CHAR_ATTR(G_VLINE, attr);
		}

		eidx++;
		eptr++;
		rowptr += NCOLS;
	}
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
	ptr[w - SIZECOL_LEN - 2] = CHAR_ATTR(G_T_HDBL_TEE, attr);
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
	ptr[w - SIZECOL_LEN - 2] = CHAR_ATTR(G_B_TEE, attr);
}

static void invalidate(int idx)
{
	if(idx == -1) {
		dirty_start = 0;
		dirty_end = fsview.num_entries - 1;
		return;
	}
	if(dirty_start == -1) {
		dirty_start = dirty_end = idx;
		return;
	}
	if(idx < dirty_start) dirty_start = idx;
	if(idx > dirty_end) dirty_end = idx;
}

static void cancel_search(void)
{
	if(search_len > 0) {
		*search = 0;
		search_len = 0;
		dirty_status = 1;
	}
}
