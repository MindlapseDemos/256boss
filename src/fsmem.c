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
#include <assert.h>
#include "fs.h"
#include "panic.h"

#define MAX_NAME	120

union memfs_node;
struct memfs_file;
struct memfs_dir;

struct memfs {
	struct memfs_dir *rootdir;
};

struct memfs_dirent {
	char name[MAX_NAME + 4];
	union memfs_node *node;
};

struct memfs_dir {
	int type;
	struct memfs_dirent *ent;
	int num_ent, max_ent;
};

struct memfs_file {
	int type;
	char *data;
	int size, max_size;
};

union memfs_node {
	int type;
	struct memfs_file file;
	struct memfs_dir dir;
};


static void destroy(struct filesys *fs);

static struct fs_node *open(struct filesys *fs, const char *path);
static void close(struct fs_node *node);
static long fsize(struct fs_node *node);
static int seek(struct fs_node *node, int offs, int whence);
static long tell(struct fs_node *node);
static int read(struct fs_node *node, void *buf, int sz);
static int write(struct fs_node *node, void *buf, int sz);
static int rewinddir(struct fs_node *node);
static struct fs_dirent *readdir(struct fs_node *node);

static union memfs_node *alloc_node(int type);
static void free_node(union memfs_node *node);

static struct fs_operations fs_mem_ops = {
	destroy,
	open, close,

	fsize,
	seek, tell,
	read, write,

	rewinddir, readdir
};


struct filesys *fsmem_create(int dev, uint64_t start, uint64_t size)
{
	struct filesys *fs;
	struct memfs *memfs;

	if(dev != DEV_MEMDISK) {
		return 0;
	}

	if(!(memfs = malloc(sizeof *memfs))) {
		panic("MEMFS: create failed to allocate memory\n");
	}
	if(!(memfs->rootdir = (struct memfs_dir*)alloc_node(FSNODE_DIR))) {
		panic("MEMFS: failed to allocate root dir\n");
	}

	if(!(fs = malloc(sizeof *fs))) {
		panic("MEMFS: failed to allocate memory for the filesystem structure\n");
	}
	fs->type = FSTYPE_MEM;
	fs->fsop = &fs_mem_ops;
	fs->data = memfs;

	return fs;
}

static void destroy(struct filesys *fs)
{
	struct memfs *memfs = fs->data;
	free_node((union memfs_node*)memfs->rootdir);
	free(memfs);
	free(fs);
}

static struct fs_node *open(struct filesys *fs, const char *path)
{
	return 0;
}

static void close(struct fs_node *node)
{
}

static long fsize(struct fs_node *node)
{
	return 0;
}

static int seek(struct fs_node *node, int offs, int whence)
{
	return 0;
}

static long tell(struct fs_node *node)
{
	return 0;
}

static int read(struct fs_node *node, void *buf, int sz)
{
	return -1;
}

static int write(struct fs_node *node, void *buf, int sz)
{
	return -1;
}

static int rewinddir(struct fs_node *node)
{
	return -1;
}

static struct fs_dirent *readdir(struct fs_node *node)
{
	return 0;
}


static union memfs_node *alloc_node(int type)
{
	union memfs_node *node;

	if(!(node = calloc(1, sizeof *node))) {
		return 0;
	}
	node->type = type;
	return node;
}

static void free_node(union memfs_node *node)
{
	int i;

	if(!node) return;

	switch(node->type) {
	case FSNODE_FILE:
		free(node->file.data);
		break;

	case FSNODE_DIR:
		for(i=0; i<node->dir.num_ent; i++) {
			free_node(node->dir.ent[i].node);
		}
		free(node->dir.ent);
		break;
	}
}
