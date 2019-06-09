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
#include "util.h"

const char *fsizestr(size_t sz)
{
	int uidx;
	size_t frac;
	static char buf[128];
	static const char *units[] = { "b", "k", "m", "g", "t", "p" };

	if(sz < 1024) {
		sprintf(buf, "%lu", (unsigned long)sz);
		return buf;
	}

	uidx = 0;
	while(sz >= 1024 && uidx < sizeof units / sizeof *units - 1) {
		frac = ((sz & 0x3ff) * 10 + 512) >> 10;
		sz >>= 10;
		uidx++;
	}
	sprintf(buf, "%lu.%lu%s", (unsigned long)sz, (unsigned long)frac, units[uidx]);
	return buf;
}

int has_suffix(const char *name, const char *suffix)
{
	char *s = strrchr(name, '.');
	if(!s) return 0;

	return strcasecmp(s, suffix) == 0;
}
