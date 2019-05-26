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
#include <assert.h>
#include "shell.h"
#include "keyb.h"
#include "fs.h"
#include "contty.h"
#include "panic.h"

static void print_prompt(void);
static int cmd_clear(int argc, char **argv);
static int cmd_help(int argc, char **argv);

static int cmd_chdir(int argc, char **argv);
static int cmd_list(int argc, char **argv);
static int cmd_cat(int argc, char **argv);

#define INBUF_SIZE		256

static char inbuf[INBUF_SIZE];
static int input_len;
static int cursor;

int sh_init(void)
{
	printf("256boss debug shell\n");
	print_prompt();
	return 0;
}

void sh_shutdown(void)
{
}

#define MAX_ARGV	128

static char **tokenize(char *str, int *num_args)
{
	static char *argv[MAX_ARGV];
	int argc = 0;
	char *ptr, *end;

	ptr = str;
	for(;;) {
		while(*ptr && isspace(*ptr)) ptr++;
		if(!*ptr) break;

		end = ptr + 1;
		while(*end && !isspace(*end)) end++;

		if(argc < MAX_ARGV - 1) {
			argv[argc++] = ptr;
		}

		if(!*end) break;
		*end = 0;
		ptr = end + 1;
	}

	argv[argc] = 0;
	*num_args = argc;
	return argv;
}

static struct {
	const char *name;
	int (*func)(int, char**);
} commands[] = {
	{"cd", cmd_chdir},
	{"ls", cmd_list},
	{"cat", cmd_cat},
	{"clear", cmd_clear},
	{"help", cmd_help},
	{0, 0}
};

int sh_eval(const char *str)
{
	char *cmdbuf;
	char **argv;
	int i, argc;

	if(!(cmdbuf = malloc(strlen(str) + 1))) {
		panic("sh_eval: failed to allocate command-line buffer\n");
	}
	strcpy(cmdbuf, str);

	if(!(argv = tokenize(cmdbuf, &argc))) {
		free(cmdbuf);
		return -1;
	}

	for(i=0; commands[i].name; i++) {
		if(strcmp(commands[i].name, argv[0]) == 0) {
			int res = commands[i].func(argc, argv);
			free(cmdbuf);
			return res;
		}
	}

	if(argv[0]) {
		printf("unknown command: %s\n", argv[0]);
	}
	free(cmdbuf);
	return -1;
}

void sh_input(int c)
{
	switch(c) {
	case '\n':
	case '\r':
		inbuf[input_len] = 0;
		input_len = cursor = 0;
		con_putchar('\n');
		sh_eval(inbuf);
		print_prompt();
		/* TODO append to history */
		break;

	case '\t':
		/* TODO completion */
		break;

	case '\b':
		if(input_len) {
			if(cursor == input_len) {
				input_len--;
				cursor--;
			} else {
				memmove(inbuf + cursor - 1, inbuf + cursor, input_len - cursor);
				cursor--;
			}
			con_putchar(c);
		}
		break;

	case KB_UP:
		/* TODO history up */
		break;

	case KB_DOWN:
		/* TODO history down */
		break;

	case KB_LEFT:
		if(cursor > 0) {
			cursor--;
		}
		break;

	case KB_RIGHT:
		if(cursor < input_len) {
			cursor++;
		}
		break;

	case KB_HOME:
		cursor = 0;
		break;

	case KB_END:
		cursor = input_len;
		break;

	default:
		if(c < 256 && isprint(c) && input_len < INBUF_SIZE - 1) {
			if(cursor == input_len) {
				inbuf[input_len++] = c;
				cursor++;
			} else {
				memmove(inbuf + cursor + 1, inbuf + cursor, input_len - cursor);
				inbuf[cursor++] = c;
				input_len++;
			}
			con_putchar(c);
		}
	}

	assert(cursor <= input_len);
}

static void print_prompt(void)
{
	printf("> ");
}

static int cmd_clear(int argc, char **argv)
{
	con_clear();
	return 0;
}

static int cmd_help(int argc, char **argv)
{
	int i;

	printf("Available commands:\n");
	for(i=0; commands[i].name; i++) {
		printf("%s\n", commands[i].name);
	}
	return 0;
}

/* filesystem commands */
static int cmd_chdir(int argc, char **argv)
{
	if(argc != 2) {
		printf("usage: %d <directory>\n", argv[0]);
		return -1;
	}
	return chdir(argv[1]);
}

static void list_dir(DIR *dir)
{
	while((dent = readdir(dir))) {
		printf("%s\n", dent->d_name);
	}
}

static int cmd_list(int argc, char **argv)
{
	int i, num_listed = 0, opt_long = 0;
	DIR *dir;
	struct dirent *dent;

	for(i=1; i<argc; i++) {
		if(argv[i][0] == '-') {
			if(argv[i][2] == 0) {
				switch(argv[i][1]) {
				case 'l':
					opt_long = 1;
					break;

				default:
					print_ls_usage(argv[0]);
					return -1;
				}
			} else {
				print_ls_usage(argv[0]);
				return -1;
			}
		} else {
			if(!(dir = opendir(argv[i]))) {
				printf("failed to open directory: %s\n", argv[i]);
				return -1;
			}
			list_dir(dir);
			closedir(dir);
			num_listed++;
		}
	}

	if(!num_listed) {
		dir = opendir(".");
		list_dir(dir);
		closedir(dir);
		return -1;
	}
	return 0;
}

static int cmd_cat(int argc, char **argv)
{
	int i, c, num_files = 0;
	FILE *fp;
	char buf[512];
	size_t sz;

	for(i=1; i<argc; i++) {
		if(!(fp = fopen(argv[i], "rb"))) {
			printf("failed to open file: %s\n", argv[i]);
			return -1;
		}
		num_files++;

		while((sz = fread(buf, 1, sizeof buf, fp)) > 0) {
			char *ptr = buf;
			while(sz-- > 0) {
				putchar(*ptr++);
			}
		}
	}

	if(!num_files) {
		for(;;) {
			kb_wait();
			while((c = kb_getkey()) >= 0) {
				if(c == 'd' && kb_isdown(KB_CTRL)) {
					goto eof;
				}
				putchar(c);
			}
		}
eof:
	}
	return 0;
}
