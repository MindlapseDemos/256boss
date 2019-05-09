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

enum {
	FSNODE_DIR,
	FSNODE_FILE
};

struct filesystem;

struct fs_node {
	struct filesystem *fs;
	int type;
	int nref;
	void *ndata;
	struct fs_node *mnt;
};

struct fs_operations {
	void *(*create)(int dev, uint64_t start, uint64_t size);
	void (*destroy)(void *fsdata);

	struct fs_node *(*lookup)(void *fsdata, const char *path);
};

struct filesystem {
	int type;
	struct fs_node *parent;
	struct fs_operations *fsop;
	void *fsdata;
};

struct fs_node *fs_root;

int fs_mount(int dev, uint64_t start, uint64_t size, struct fs_node *parent);

#endif	/* FS_H_ */