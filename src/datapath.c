/*
256boss - bootable launcher for 256byte intros
Copyright (C) 2018-2020  John Tsiombikas <nuclear@member.fsf.org>

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
#include <dirent.h>
#include "datapath.h"
#include "panic.h"

#define DATADIR	".data"

static char pathbuf[512];
static char *pathend;
static int max_fname_sz;

int init_datapath(void)
{
	DIR *dir, *datadir;
	struct dirent *dent;

	if(pathend) return 0;

	if(!(dir = opendir("/"))) {
		return -1;
	}

	while((dent = readdir(dir))) {
		int len = sprintf(pathbuf, "/%s/" DATADIR, dent->d_name);
		if((datadir = opendir(pathbuf))) {
			max_fname_sz = sizeof pathbuf - (pathend - pathbuf) - 1;
			if(max_fname_sz <= 0) {
				panic("huge datapath");
			}
			printf("datapath: %s\n", pathbuf);
			closedir(datadir);
			closedir(dir);
			pathend = pathbuf + len;
			*pathend++ = '/';
			return 0;
		}
	}

	closedir(dir);
	return -1;
}

char *datafile(const char *fname)
{
	if(!pathend) {
		strcpy(pathbuf, fname);
		return pathbuf;
	}

	int len = strlen(fname);
	if(len > max_fname_sz) len = max_fname_sz;
	memcpy(pathend, fname, len);
	pathend[len] = 0;

	return pathbuf;
}
