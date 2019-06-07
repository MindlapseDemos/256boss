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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include "fsview.h"
#include "panic.h"

static int load_cur_dir(struct fsview *fsv);


struct fsview *fsv_alloc(void)
{
	struct fsview *fsv;

	if(!(fsv = calloc(1, sizeof *fsv))) {
		fprintf(stderr, "failed to allocate fsview\n");
		return 0;
	}
	if(fsv_init(fsv) == -1) {
		free(fsv);
		return 0;
	}
	return fsv;
}

void fsv_free(struct fsview *fsv)
{
	if(!fsv) return;

	fsv_destroy(fsv);
	free(fsv);
}

int fsv_init(struct fsview *fsv)
{
	return load_cur_dir(fsv);
}

void fsv_destroy(struct fsview *fsv)
{
	free(fsv->entries);
	fsv->entries = fsv->files = fsv->dirs = 0;
	fsv->num_entries = fsv->num_files = fsv->num_dirs = 0;
}

int fsv_sel(struct fsview *fsv, int s)
{
	if(s == fsv->cursel) return 0;
	if(s < 0 || s >= fsv->num_entries) return 0;

	fsv->cursel = s;
	return 1;
}

int fsv_sel_prev(struct fsview *fsv)
{
	if(fsv->cursel <= 0) return 0;
	fsv->cursel--;
	return 1;
}

int fsv_sel_next(struct fsview *fsv)
{
	if(fsv->cursel >= fsv->num_entries - 1) return 0;
	fsv->cursel++;
	return 1;
}

int fsv_sel_first(struct fsview *fsv)
{
	if(fsv->cursel <= 0) return 0;
	fsv->cursel = 0;
	return 1;
}

int fsv_sel_last(struct fsview *fsv)
{
	if(fsv->cursel >= fsv->num_entries - 1) return 0;
	fsv->cursel = fsv->num_entries - 1;
	return 1;
}

int fsv_chdir(struct fsview *fsv, const char *path)
{
	if(chdir(path) == -1) {
		return -1;
	}
	load_cur_dir(fsv);
	return 0;
}

int fsv_updir(struct fsview *fsv)
{
	if(chdir("..") == -1) {
		return -1;
	}
	load_cur_dir(fsv);
	return 0;
}

int fsv_activate(struct fsview *fsv)
{
	struct fsview_dirent *fsvent = fsv->entries + fsv->cursel;
	if(fsvent->type == DT_DIR) {
		chdir(fsvent->name);
		load_cur_dir(fsv);
		return 0;
	}

	if(fsv->openfunc) {
		return fsv->openfunc(fsvent->name);
	}
	return -1;
}


static int load_cur_dir(struct fsview *fsv)
{
	int nfiles = 0, ndirs = 0;
	DIR *dir;
	struct dirent *dent;
	struct fsview_dirent *fsvent;
	char *name;

	if(!(dir = opendir("."))) {
		panic("failed to open current dir!\n");
	}

	while((dent = readdir(dir))) {
		if(strcmp(dent->d_name, ".") == 0) {
			continue;
		}
		if(dent->d_type == DT_DIR) {
			ndirs++;
		} else {
			nfiles++;
		}
	}
	rewinddir(dir);

	if(!(fsvent = malloc((nfiles + ndirs) * sizeof *fsvent))) {
		fprintf(stderr, "failed to allocate %d entries\n", nfiles + ndirs);
		closedir(dir);
		return -1;
	}

	free(fsv->entries);
	fsv->entries = fsv->dirs = fsvent;
	fsv->files = fsvent + ndirs;
	fsv->num_entries = nfiles + ndirs;
	fsv->num_files = 0;
	fsv->num_dirs = 0;

	while((dent = readdir(dir))) {
		if(strcmp(dent->d_name, ".") == 0) {
			continue;
		}
		if(!(name = malloc(strlen(dent->d_name) + 1))) {
			fprintf(stderr, "failed to allocate entry name\n");
			closedir(dir);
			fsv_destroy(fsv);
			return -1;
		}

		if(dent->d_type == DT_DIR) {
			fsv->dirs->name = name;
			fsv->dirs->type = DT_DIR;
			fsv->dirs++;
			fsv->num_dirs++;
		} else {
			fsv->files->name = name;
			fsv->files->type = DT_REG;
			fsv->files++;
			fsv->num_files++;
		}
	}
	closedir(dir);

	fsv->cursel = 0;
	fsv->scroll = 0;
	return 0;
}
