/*
pcboot - bootable PC demo/game kernel
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
#ifndef DIRENT_H_
#define DIRENT_H_

typedef struct DIR DIR;

struct dirent {
	char d_name[256];
};

DIR *opendir(const char *path);
int closedir(DIR *dir);

void rewinddir(DIR *dir);

struct dirent *readdir(DIR *dir);

#endif	/* DIRENT_H_ */
