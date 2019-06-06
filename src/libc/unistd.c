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
#include <string.h>
#include <errno.h>
#include "unistd.h"
#include "fs.h"

int chdir(const char *path)
{
	return fs_chdir(path);
}

char *getcwd(char *buf, int sz)
{
	char *cwd = fs_getcwd();
	int len = strlen(cwd);
	if(len + 1 > sz) {
		errno = ERANGE;
		return 0;
	}
	memcpy(buf, cwd, len + 1);
	return buf;
}
