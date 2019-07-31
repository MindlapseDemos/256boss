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
#include "widget.h"

static void draw_window(struct gui_gfx *g, struct gui_widget *w);

int gui_window(struct gui_widget *w, int x, int y, int width, int height, const char *name, struct gui_widget *parent)
{
	if(!(w->name = calloc(1, strlen(name) + 1))) {
		return -1;
	}
	strcpy(w->name, name);

	w->type = GUI_WINDOW;
	w->state = GUI_VISIBLE | GUI_ACTIVE;
	w->x = x;
	w->y = y;
	w->width = width;
	w->height = height;

	w->draw = draw_window;
	return 0;
}

static void draw_window(struct gui_gfx *g, struct gui_widget *w)
{
	g->color = 0xff0000;
	g->draw.rect(g, w->x, w->y, w->width, w->height);
}
