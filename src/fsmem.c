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
#include <errno.h>
#include "fs.h"
#include "panic.h"

#define MAX_NAME	120

struct memfs_node;
struct memfs_file;
struct memfs_dir;

struct memfs {
	struct memfs_node *rootdir;
};

struct memfs_dir {
	struct memfs_node *clist, *ctail;
	struct memfs_node *cur;
};

struct memfs_file {
	char *data;
	long cur_pos;
	long size, max_size;
};

struct memfs_node {
	int type;
	char name[MAX_NAME + 4];
	union {
		struct memfs_file file;
		struct memfs_dir dir;
	};
	struct memfs_dir *parent;
	struct memfs_node *next;
};


static void destroy(struct filesys *fs);

static struct fs_node *open(struct filesys *fs, const char *path, unsigned int flags);
static void close(struct fs_node *node);
static long fsize(struct fs_node *node);
static int seek(struct fs_node *node, int offs, int whence);
static long tell(struct fs_node *node);
static int read(struct fs_node *node, void *buf, int sz);
static int write(struct fs_node *node, void *buf, int sz);
static int rewinddir(struct fs_node *node);
static struct fs_dirent *readdir(struct fs_node *node);

static struct memfs_node *alloc_node(int type);
static void free_node(struct memfs_node *node);

static struct memfs_node *find_entry(struct memfs_node *dir, const char *name);
static void add_child(struct memfs_node *dir, struct memfs_node *n);


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
	if(!(memfs->rootdir = alloc_node(FSNODE_DIR))) {
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
	free_node((struct memfs_node*)memfs->rootdir);
	free(memfs);
	free(fs);
}

static struct fs_node *open(struct filesys *fs, const char *path, unsigned int flags)
{
	struct fs_node *fsn;
	struct memfs_node *node, *parent;
	struct memfs *memfs = fs->data;
	char name[MAX_NAME + 1];

	if(path[0] == '/') {
		node = memfs->rootdir;
		path = fs_path_skipsep((char*)path);
	} else {
		if(cwdnode->fs->type != FSTYPE_MEM) {
			return 0;
		}
		node = cwdnode->data;
	}

	while(*path) {
		if(node->type != FSNODE_DIR) {
			/* we have more path components, yet the last one wasn't a dir */
			errno = ENOTDIR;
			return 0;
		}

		path = fs_path_next(path, name, sizeof name);

		if(!(node = find_entry(node, name))) {
			if(*path || !(flags & FSO_CREATE)) {
				errno = ENOENT;
				return 0;
			}
			/* create and add */
			parent = node;
			if(!(node = alloc_node((flags & FSO_DIR) ? FSNODE_DIR : FSNODE_FILE))) {
				errno = ENOMEM;
				return 0;
			}
			add_child(parent, node);
			return create_fsnode(fs, node);
		}
	}

	return create_fsnode(fs, node);
}

static struct fs_node *create_fsnode(struct filesys *fs, struct memfs_node *n)
{
	struct fs_node *fsn;
	struct memfs_node *node;

	if(!(fsn = malloc(sizeof *fsn))) {
		errno = ENOMEM;
		return 0;
	}
	if(!(node = malloc(sizeof *node))) {
		errno = ENOMEM;
		free(fsn);
		return 0;
	}
	*node = n;

	fsn->fs = fs;
	fsn->type = node->type;
	fsn->data = node;
	return fsn;
}

static void close(struct fs_node *node)
{
	free(node->data);
	free(node);
}

static long fsize(struct fs_node *node)
{
	struct memfs_node *n;
	if(node->type != FSNODE_FILE) {
		return -1;
	}
	n = node->data;
	return n->file.size;
}

static int seek(struct fs_node *node, int offs, int whence)
{
	struct memfs_node *n;
	long new_pos;

	if(!node || node->type != FSNODE_FILE) {
		return -1;
	}

	n = node->data;

	switch(whence) {
	case FSSEEK_SET:
		new_pos = offs;
		break;

	case FSSEEK_CUR:
		new_pos = node->file.cur_pos + offs;
		break;

	case FSSEEK_END:
		new_pos = node->file.size + offs;
		break;

	default:
		return -1;
	}

	if(new_pos < 0) new_pos = 0;

	file->cur_pos = new_pos;
	return 0;
}

static long tell(struct fs_node *node)
{
	struct memfs_node *n;

	if(!node || node->type != FSNODE_FILE) {
		return -1;
	}
	n = node->data;
	return n->file.cur_pos;
}

static int read(struct fs_node *node, void *buf, int sz)
{
	struct memfs_node *n;

	if(!node || !buf || sz < 0 || node->type != FSNODE_FILE) {
		return -1;
	}

	n = node->data;

	if(sz > n->file.size - n->file.cur_pos) {
		sz = n->file.size - n->file_cur_pos;
	}
	memcpy(buf, n->file.data + n->file.cur_pos, sz);
	n->file.cur_pos += sz;
	return sz;
}

static int write(struct fs_node *node, void *buf, int sz)
{
	struct memfs_node *n;
	int total_sz, new_max_sz;
	void *tmp;

	if(!node || !buf || sz < 0 || node->type != FSNODE_FILE) {
		return -1;
	}

	n = node->data;
	total_sz = n->file.cur_pos + sz;
	if(total_sz > n->file.max_size) {
		if(total_sz < n->file.max_size * 2) {
			new_max_sz = n->file.max_size * 2;
		} else {
			new_max_sz = total_sz;
		}
		if(!(tmp = realloc(n->file.data, new_max_sz))) {
			errno = ENOSPC;
			return -1;
		}
		n->file.data = tmp;
		n->file.max_size = new_max_sz;
	}

	memcpy(n->file.data + n->file.cur_pos, buf, sz);
	n->file.cur_pos += sz;
	return sz;
}

static int rewinddir(struct fs_node *node)
{
	return -1;
}

static struct fs_dirent *readdir(struct fs_node *node)
{
	return 0;
}


static struct memfs_node *alloc_node(int type)
{
	struct memfs_node *node;

	if(!(node = calloc(1, sizeof *node))) {
		return 0;
	}
	node->type = type;
	return node;
}

static void free_node(struct memfs_node *node)
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

static struct memfs_dirent *find_entry(struct memfs_dir *dir, const char *name)
{
	int i;

	for(i=0; i<dir->num_ent; i++) {
		if(strcasecmp(dir->ent[i].name, name) == 0) {
			return dir->ent + i;
		}
	}
	return 0;
}

static void add_child(struct memfs_dir *dir, struct memfs_node *n)
{
	if(dir->clist) {
		dir->ctail->next = n;
		dir->ctail = n;
	} else {
		dir->clist = dir->ctail = n;
	}
	n->parent = dir;
}
