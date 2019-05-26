#ifndef FILE_H_
#define FILE_H_

#include <errno.h>
#include "fs.h"

enum {
	MODE_READ = 1,
	MODE_WRITE = 2,
	MODE_APPEND = 4,
	MODE_CREATE = 8,
	MODE_TRUNCATE = 16
};

struct FILE {
	unsigned int mode;
	struct fs_node *fsn;
};

FILE *fopen(const char *path, const char *mode)
{
	FILE *fp;
	struct fs_node *node;
	unsigned int mflags = 0;

	while(*mode) {
		int c = *mode++;
		switch(c) {
		case 'r':
			mflags |= MODE_READ;
			if(*mode == '+') {
				mflags |= MODE_WRITE;
				mode++;
			}
			break;
		case 'w':
			mflags |= MODE_WRITE | MODE_TRUNCATE;
			if(*mode == '+') {
				mflags |= MODE_READ | MODE_CREATE;
				mode++;
			}
			break;
		case 'a':
			mflags |= MODE_WRITE | MODE_APPEND;
			if(*mode == '+') {
				mflags |= MODE_READ | MODE_CREATE;
				mode++;
			}
			break;
		case 'b':
			break;
		default:
			errno = EINVAL;
			return 0;
		}
	}

	if(!(node = fs_open(path))) {
		/* TODO: create */
		errno = ENOENT;	/* TODO */
		return 0;
	}
	if(node->type != FSNODE_FILE) {
		errno = EISDIR;
		return 0;
	}

	if(!(fp = malloc(sizeof *fp))) {
		errno = ENOMEM;
		return 0;
	}
	fp->fsn = node;
	fp->mode = mflags;

	return fp;
}

int fclose(FILE *fp)
{
	if(!fp) {
		errno = EINVAL;
		return -1;
	}

	/* TODO: cont... */
}

#endif	/* FILE_H_ */
