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
#ifndef WIDGET_H_
#define WIDGET_H_

#include "gfx.h"

enum gui_widget_type {
	GUI_WIDGET,
	GUI_WINDOW,
	GUI_LABEL,
	GUI_BUTTON
};

enum {
	GUI_VISIBLE		= 1,
	GUI_HOVER		= 2,
	GUI_FOCUSED		= 4,
	GUI_ACTIVE		= 8
};

struct gui_widget;

typedef void (*gui_draw_func)(struct gui_gfx *g, struct gui_widget*);

struct gui_widget {
	enum gui_widget_type type;

	int x, y, width, height;
	char *name;
	unsigned int state;

	struct widget *parent, *clist;
	struct widget *next;

	gui_draw_func draw;
};

int gui_window(struct gui_widget *w, int x, int y, int width, int height, const char *name, struct gui_widget *parent);

#endif	/* WIDGET_H_ */
