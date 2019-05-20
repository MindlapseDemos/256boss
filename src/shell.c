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
#include <ctype.h>
#include "shell.h"
#include "keyb.h"
#include "fs.h"
#include "contty.h"

#define INBUF_SIZE		256

static char inbuf[INBUF_SIZE];
static int input_len;
static int cursor;

int sh_init(void)
{
	printf("256boss debug shell\n");
	return 0;
}

void sh_shutdown(void)
{
}

static char **tokenize(const char *str, int *argc_ptr)
{
}

int sh_eval(const char *str)
{
}

void sh_input(int c)
{
	switch(c) {
	case '\n':
	case '\r':
		inbuf[input_len] = 0;
		input_len = 0;
		sh_eval(inbuf);
		/* TODO append to history */
		con_putchar(c);
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
		}
		con_putchar(c);
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
}
