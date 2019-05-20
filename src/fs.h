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
#ifndef FS_H_
#define FS_H_

#include <inttypes.h>

enum {
	FSTYPE_FAT,

	NUM_FSTYPES
};

enum { FSNODE_FILE, FSNODE_DIR };

enum { FSSEEK_SET, FSSEEK_CUR, FSSEEK_END };

struct filesys;
struct fs_node;
struct fs_dirent;

struct fs_operations {
	void (*destroy)(struct filesys *fs);

	struct fs_node *(*open)(struct filesys *fs, const char *path);
	void (*close)(struct fs_node *node);

	int (*seek)(struct fs_node *node, int offs, int whence);
	int (*read)(struct fs_node *node, void *buf, int sz);
	int (*write)(struct fs_node *node, void *buf, int sz);

	int (*rewinddir)(struct fs_node *node);
	struct fs_dirent *(*readdir)(struct fs_node *node);
};

struct filesys {
	int type;
	struct fs_operations *fsop;
	void *data;
};

struct fs_node {
	struct filesys *fs;
	int type;
	void *data;
};

struct fs_dirent {
	char *name;
	void *data;
};

struct filesys *rootfs;
struct fs_node *cwdnode;	/* current working directory node */

int fs_mount(int dev, uint64_t start, uint64_t size, struct fs_node *parent);

int fs_chdir(const char *path);

struct fs_node *fs_open(const char *path);
int fs_close(struct fs_node *node);

int fs_seek(struct fs_node *node, int offs, int whence);
int fs_read(struct fs_node *node, void *buf, int sz);
int fs_write(struct fs_node *node, void *buf, int sz);

int fs_rewinddir(struct fs_node *node);
struct fs_dirent *fs_readdir(struct fs_node *node);

#endif	/* FS_H_ */
