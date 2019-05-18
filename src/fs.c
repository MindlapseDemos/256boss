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
#include "fs.h"

struct filesys *fsfat_create(int dev, uint64_t start, uint64_t size);

static struct filesys *(*createfs[])(int, uint64_t, uint64_t) = {
	fsfat_create
};

int fs_free_node(struct fs_node *node)
{
	struct filesys *fs = node->fs;
	fs->fsop->close(fs, node);
	return 0;
}

int fs_mount(int dev, uint64_t start, uint64_t size, struct fs_node *parent)
{
	int i;
	struct filesys *fs;

	if(!parent && rootfs) {
		printf("fs_mount error: root filesystem already mounted!\n");
		return -1;
	}

	for(i=0; i<NUM_FSTYPES; i++) {
		if((fs = createfs[i](dev, start, size))) {
			if(!parent) {
				rootfs = fs;
			}
			return 0;
		}
	}

	printf("failed to mount filesystem dev: %d, start %llu\n", dev, (unsigned long long)start);
	return -1;
}


struct fs_node *fs_open(const char *path)
{
	struct filesys *fs;
	struct fs_node *node;

	if(!path || !*path) {
		return 0;
	}

	if(*path == '/') {
		fs = rootfs;
	} else {
		if(!cwdnode) return 0;
		fs = cwdnode->fs;
	}

	if(!(node = fs->fsop->open(fs, path))) {
		return 0;
	}
	return node;
}

int fs_close(struct fs_node *node)
{
	struct filesys *fs = node->fs;
	fs->fsop->close(fs, node);
	return 0;
}

int fs_seek(struct fs_node *node, int offs, int whence)
{
	return -1;	/* TODO */
}

int fs_read(struct fs_node *node, void *buf, int sz)
{
	return -1;	/* TODO */
}

int fs_write(struct fs_node *node, void *buf, int sz)
{
	return -1;	/* TODO */
}

int fs_rewinddir(struct fs_node *node)
{
	return -1;	/* TODO */
}

struct fs_dirent *fs_readdir(struct fs_node *node)
{
	return 0;	/* TODO */
}
