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
#include "dynarr.h"
#include "fileview.h"

static int gen_line_spans(struct fileview *fv);

struct fileview *fv_open(const char *path)
{
	FILE *fp;
	char *buf;
	int size;
	struct fileview *fv;

	if(!(fp = fopen(path, "rb"))) {
		return 0;
	}
	size = filesize(fp);

	if(!(buf = malloc(size))) {
		fclose(fp);
		return 0;
	}
	fread(buf, size, 1, fp);
	fclose(fp);

	if(!(fv = calloc(1, sizeof *fv))) {
		free(buf);
		return 0;
	}
	fv->data = buf;
	fv->size = size;
	fv->type = FILE_TYPE_TEXT;	/* will change to BIN in gen_line_spans if it's not text */

	if(gen_line_spans(fv) == -1) {
		free(buf);
		free(fv);
		return 0;
	}
	return fv;
}

static int gen_line_spans(struct fileview *fv)
{
	int i, newline, size = fv->size;
	char *ptr = fv->data;
	struct span span;
	void *tmp;

	if(!(fv->lines = dynarr_alloc(0, sizeof *fv->lines))) {
		return -1;
	}

	span.start = ptr;
	span.len = size;

	for(i=0; i<fv->size; i++) {
		if(*ptr == 0 || *ptr == 0xff) {
			fv->type = FILE_TYPE_BIN;
		}
		if(*ptr == '\r') {
			if(ptr[1] == '\n') {
				i++;
				ptr++;
			}
			newline = 1;
		} else if(*ptr == '\n') {
			newline = 1;
		} else {
			newline = 0;
		}
		ptr++;

		if(newline) {
			span.len = ptr - span.start;
			size -= span.len;

			if(!(tmp = dynarr_push(fv->lines, &span))) {
				dynarr_free(fv->lines);
				fv->lines = 0;
				return -1;
			}
			fv->lines = tmp;
			span.start = ptr;
			span.len = size;
		}
	}

	if(span.len) {
		if(!(tmp = dynarr_push(fv->lines, &span))) {
			dynarr_free(fv->lines);
			fv->lines = 0;
			return -1;
		}
		fv->lines = tmp;
	}

	fv->num_lines = dynarr_size(fv->lines);
	fv->lines = dynarr_finalize(fv->lines);
	return 0;
}

void fv_close(struct fileview *fv)
{
	if(!fv) return;
	free(fv->data);
	free(fv->lines);
	free(fv);
}
