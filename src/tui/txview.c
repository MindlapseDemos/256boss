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
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include "txview.h"
#include "contty.h"
#include "keyb.h"
#include "textui.h"
#include "ui/fileview.h"


#define NCOLS	80
#define NROWS	25
#define VIS_LINES	(NROWS - 2)

#define ATTR(fg, bg)	((fg) | ((bg) << 4))
#define ATTR_TX			ATTR(LTGREY, BLACK)

#define CHAR_ATTR(c, a) \
	((uint16_t)(c) | ((uint16_t)(a) << 8))


static void draw_text(void);
static void draw_hex(void);
static void scroll_text(int dx, int dy);
static void scroll_hex(int dy);

static struct fileview *fv;
static int max_line_len;
static int scroll_x, scroll_y;
static int mode;
static int dirty;
static char *title;

int txview_open(const char *path)
{
	int i;

	if(!(fv = fv_open(path))) {
		return -1;
	}
	mode = fv->type;
	dirty = 1;
	scroll_x = scroll_y = 0;

	max_line_len = 0;
	for(i=0; i<fv->num_lines; i++) {
		if(fv->lines[i].len > max_line_len) {
			max_line_len = fv->lines[i].len;
		}
	}

	if(!(title = malloc(strlen(path) + 7))) {
		txui_set_title(path);
	} else {
		sprintf(title, "%s [%s]", path, mode == FILE_TYPE_TEXT ? "txt" : "hex");
		txui_set_title(title);
	}
	return 0;
}

void txview_close(void)
{
	free(title);
	fv_close(fv);
}

void txview_draw(void)
{
	if(!dirty) return;
	dirty = 0;

	if(mode == FILE_TYPE_TEXT) {
		draw_text();
	} else {
		draw_hex();
	}
}

#define SCROLL(x, y)	\
	do { \
		if(mode == FILE_TYPE_TEXT) { \
			scroll_text(x, y); \
		} else { \
			scroll_hex(y); \
		} \
	} while(0)

int txview_keypress(int c)
{
	switch(c) {
	case 27:
		return -1;

	case KB_DOWN:
		SCROLL(0, 1);
		break;

	case KB_UP:
		SCROLL(0, -1);
		break;

	case KB_LEFT:
		SCROLL(-1, 0);
		break;

	case KB_RIGHT:
		SCROLL(1, 0);
		break;

	case KB_PGDN:
		SCROLL(0, NROWS);
		break;

	case KB_PGUP:
		SCROLL(0, -NROWS);
		break;

	case KB_HOME:
		if(mode == FILE_TYPE_TEXT) {
			scroll_text(-scroll_x, 0);
		} else {
			scroll_hex(-scroll_y);
		}
		break;

	case KB_END:
		if(mode == FILE_TYPE_TEXT) {
			scroll_text(INT_MAX >> 2, 0);
		} else {
			scroll_hex(INT_MAX >> 2);
		}
		break;
	}
	return 0;
}

static void draw_text(void)
{
	int i, j, c, x, lidx, tabstop, scrcol;
	struct span *lptr;
	char *cptr;
	uint16_t *vptr;

	lidx = scroll_y;
	lptr = fv->lines + scroll_y;

	vptr = (uint16_t*)0xb8000 + NCOLS;

	for(i=0; i<VIS_LINES; i++) {
		cptr = lptr->start;
		x = 0;
		scrcol = 0;
		if(lidx++ < fv->num_lines) {
			for(j=0; j<lptr->len; j++) {
				if(x - scroll_x >= NCOLS) break;

				c = *cptr++;
				switch(c) {
				case '\t':
					tabstop = (x + 4) & 0xfc;
					while(x < tabstop) {
						if(x++ < scroll_x) continue;
						*vptr++ = CHAR_ATTR(' ', ATTR_TX);
						scrcol++;
					}
					break;

				case '\r':
				case '\n':
					break;

				default:
					if(x++ >= scroll_x) {
						*vptr++ = CHAR_ATTR(c, ATTR_TX);
						scrcol++;
					}
				}
			}
			lptr++;
		}

		if(scrcol < NCOLS) {
			memset16(vptr, CHAR_ATTR(' ', ATTR_TX), NCOLS - scrcol);
			vptr += NCOLS - scrcol;
		}
	}
}

static void draw_hex(void)
{
}

static void scroll_text(int dx, int dy)
{
	int newx, newy;

	newx = scroll_x + dx;
	newy = scroll_y + dy;

	if(newx > max_line_len - NCOLS) newx = max_line_len - NCOLS;
	if(newx < 0) newx = 0;

	if(newy > fv->num_lines - VIS_LINES) newy = fv->num_lines - VIS_LINES;
	if(newy < 0) newy = 0;

	if(newx == scroll_x && newy == scroll_y) return;

	scroll_x = newx;
	scroll_y = newy;
	dirty = 1;
}

static void scroll_hex(int dy)
{
}
