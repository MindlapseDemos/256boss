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
#include <ctype.h>
#include "cfgfile.h"

static char *clean_line(char *s);

struct cfglist *alloc_cfglist(void)
{
	struct cfglist *cfg;

	if(!(cfg = calloc(1, sizeof *cfg))) {
		return 0;
	}
	return cfg;
}

void free_cfglist(struct cfglist *cfg)
{
	if(!cfg) return;

	while(cfg->head) {
		struct cfgopt *opt = cfg->head;
		cfg->head = cfg->head->next;

		free(opt->key);
		free(opt->valstr);
		free(opt);
	}
	free(cfg);
}

struct cfglist *load_cfglist(const char *fname)
{
	FILE *fp;
	struct cfglist *cfg;
	char buf[256];
	char *line, *key, *val;

	if(!(fp = fopen(fname, "rb"))) {
		printf("load_cfglist: failed to open: %s\n", fname);
		return 0;
	}
	if(!(cfg = alloc_cfglist())) {
		fclose(fp);
		return 0;
	}

	while(fgets(buf, sizeof buf, fp)) {
		line = clean_line(buf);
		if(!*line) continue;

		if(!(val = strchr(line, '='))) {
			printf("load_cfglist: skipping malformed config file line: %s\n", line);
			continue;
		}
		*val++ = 0;

		key = clean_line(line);
		val = clean_line(val);

		if(!*key || !*val) {
			printf("load_cfglist: skipping malformed config file line\n");
			continue;
		}

		printf("CFG setstr(%s, %s)\n", key, val);
		if(cfg_setstr(cfg, key, val) == -1) {
			printf("load_cfglist: failed to add new key/value pair\n");
			continue;
		}
	}

	return cfg;
}

int save_cfglist(struct cfglist *cfg, const char *fname)
{
	return -1;	/* TODO */
}

struct cfgopt *cfg_getopt(struct cfglist *cfg, const char *key)
{
	struct cfgopt *opt;

	if(!cfg) return 0;

	opt = cfg->head;
	while(opt) {
		if(strcasecmp(opt->key, key) == 0) {
			break;
		}
		opt = opt->next;
	}
	return opt;
}

const char *cfg_getstr(struct cfglist *cfg, const char *key, const char *defval)
{
	struct cfgopt *opt;

	if(!cfg) return defval;

	opt = cfg->head;
	while(opt) {
		if(strcasecmp(opt->key, key) == 0) {
			return opt->valstr;
		}
		opt = opt->next;
	}
	return defval;
}

int cfg_getint(struct cfglist *cfg, const char *key, int defval)
{
	struct cfgopt *opt;

	if(!cfg) return defval;

	opt = cfg->head;
	while(opt) {
		if(strcasecmp(opt->key, key) == 0) {
			return (opt->type == CFGOPT_INT) ? opt->ival : defval;
		}
		opt = opt->next;
	}
	return defval;
}

int cfg_setstr(struct cfglist *cfg, const char *key, const char *val)
{
	char *endp;
	struct cfgopt *opt;

	if(!cfg) return -1;

	if(!(opt = malloc(sizeof *opt)) ||
			!(opt->key = malloc(strlen(key) + 1)) ||
			!(opt->valstr = malloc(strlen(val) + 1))) {
		if(opt) free(opt->key);
		free(opt);
		return -1;
	}
	strcpy(opt->key, key);
	strcpy(opt->valstr, val);
	opt->ival = strtol(val, &endp, 0);
	opt->type = (endp != val) ? CFGOPT_INT : CFGOPT_STR;

	opt->next =  0;
	if(cfg->head) {
		cfg->tail->next = opt;
		cfg->tail = opt;
	} else {
		cfg->head = cfg->tail = opt;
	}
	cfg->count++;
	return 0;
}

int cfg_setint(struct cfglist *cfg, const char *key, int val)
{
	char buf[32];

	sprintf(buf, "%d", val);
	return cfg_setstr(cfg, key, buf);
}

int cfg_remove(struct cfglist *cfg, const char *key)
{
	int res = -1;
	struct cfgopt dummy, *prev, *opt;

	if(!cfg) return -1;

	dummy.next = cfg->head;
	prev = &dummy;
	while(prev->next) {
		opt = prev->next;
		if(strcasecmp(opt->key, key) == 0) {
			prev->next = opt->next;
			free(opt->key);
			free(opt->valstr);
			free(opt);

			if(cfg->tail == opt) {
				cfg->tail = prev;
			}
			res = 0;
			break;
		}
		prev = opt;
	}
	cfg->head = dummy.next;

	return res;
}

static char *clean_line(char *s)
{
	char *endp;

	while(*s && isspace(*s)) s++;
	endp = s + strlen(s) - 1;
	while(endp > s && isspace(*endp)) {
		*endp-- = 0;
	}
	return s;
}
