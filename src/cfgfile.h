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
#ifndef CFGFILE_H_
#define CFGFILE_H_

enum {
	CFGOPT_STR,
	CFGOPT_INT
};

struct cfgopt {
	char *key;
	char *valstr;
	int type;
	int ival;

	struct cfgopt *next;
};

struct cfglist {
	struct cfgopt *head, *tail;
	int count;
};

struct cfglist *alloc_cfglist(void);
void free_cfglist(struct cfglist *cfg);

struct cfglist *load_cfglist(const char *fname);
int save_cfglist(struct cfglist *cfg, const char *fname);

struct cfgopt *cfg_getopt(struct cfglist *cfg, const char *key);

const char *cfg_getstr(struct cfglist *cfg, const char *key, const char *defval);
int cfg_getint(struct cfglist *cfg, const char *key, int defval);

int cfg_setstr(struct cfglist *cfg, const char *key, const char *val);
int cfg_setint(struct cfglist *cfg, const char *key, int val);

int cfg_remove(struct cfglist *cfg, const char *key);

#endif	/* CFGFILE_H_ */
