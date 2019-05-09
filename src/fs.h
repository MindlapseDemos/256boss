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

struct filesys;
struct fs_dir;

struct fs_operations {
	void (*destroy)(struct filesys *fs);

	struct fs_dir *(*opendir)(struct filesys *fs, const char *path);
	void (*closedir)(struct filesys *fs, struct fs_dir *dir);
};

struct filesys {
	int type;
	struct fs_operations *fsop;
	struct fs_dir *root;
	void *data;
};

struct fs_dir {
	struct filesys *fs, *mnt;
	struct fs_dirent *ent;
	int num_ent;
	void *data;
};

struct filesys *rootfs;

int fs_mount(int dev, uint64_t start, uint64_t size, struct fs_dir *parent);

#endif	/* FS_H_ */
