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
#ifndef FSVIEW_H_
#define FSVIEW_H_

struct fsview_dirent {
	char *name;
	int type;
	unsigned long size;
};

struct fsview {
	int show_hidden;
	int scroll;
	int num_vis;

	struct fsview_dirent *entries, *files, *dirs;
	int num_entries, num_files, num_dirs;

	int cursel;

	int (*openfile)(const char *path);
};

struct fsview fsview;

struct fsview *fsv_alloc(void);
void fsv_free(struct fsview *fsv);

int fsv_init(struct fsview *fsv);
void fsv_destroy(struct fsview *fsv);

/* the sel functions return 0 if nothing changed, 1 otherwise */
int fsv_sel(struct fsview *fsv, int s);
int fsv_sel_prev(struct fsview *fsv);
int fsv_sel_next(struct fsview *fsv);
int fsv_sel_first(struct fsview *fsv);
int fsv_sel_last(struct fsview *fsv);
int fsv_sel_match(struct fsview *fsv, const char *str);

int fsv_keep_vis(struct fsview *fsv, int idx);

int fsv_chdir(struct fsview *fsv, const char *path);
int fsv_updir(struct fsview *fsv);
int fsv_activate(struct fsview *fsv);

#endif	/* FSVIEW_H_ */
