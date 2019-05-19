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
#include <inttypes.h>
#include <ctype.h>
#include <assert.h>
#include "fs.h"
#include "bootdev.h"
#include "boot.h"
#include "panic.h"

#define MAX_NAME	195

#define DIRENT_UNUSED	0xe5

#define DENT_IS_NULL(dent)	(((unsigned char*)(dent))[0] == 0)
#define DENT_IS_UNUSED(dent)	(((unsigned char*)(dent))[0] == DIRENT_UNUSED)

#define ATTR_RO		0x01
#define ATTR_HIDDEN	0x02
#define ATTR_SYSTEM	0x04
#define ATTR_VOLID	0x08
#define ATTR_DIR	0x10
#define ATTR_ARCHIVE	0x20
#define ATTR_LFN	0xf


enum { FAT12, FAT16, FAT32, EXFAT };
static const char *typestr[] = { "fat12", "fat16", "fat32", "exfat" };

struct fat_dirent;
struct fat_dir;

struct fatfs {
	int type;
	int dev;
	uint64_t start_sect;
	uint32_t size;
	int cluster_size;
	int fat_size;
	uint32_t fat_sect;
	uint32_t root_sect;
	int root_size;
	uint32_t first_data_sect;
	uint32_t num_data_sect;
	uint32_t num_clusters;
	char label[12];

	void *fat;
	struct fat_dir *rootdir;
	unsigned int clust_mask;
	int clust_shift;
};

struct bparam {
	uint8_t jmp[3];
	unsigned char fmtid[8];
	uint16_t sect_bytes;
	uint8_t cluster_size;
	uint16_t reserved_sect;
	uint8_t num_fats;
	uint16_t num_dirent;
	uint16_t num_sectors;
	uint8_t medium;
	uint16_t fat_size;
	uint16_t track_size;
	uint16_t num_heads;
	uint32_t num_hidden;
	uint32_t num_sectors32;
} __attribute__((packed));

struct bparam_ext16 {
	uint8_t driveno;
	uint8_t ntflags;
	uint8_t signature;
	uint32_t volume_id;
	char label[11];
	char sysid[8];
} __attribute__((packed));

struct bparam_ext32 {
	uint32_t fat_size;
	uint16_t flags;
	uint16_t version;
	uint32_t root_clust;
	uint16_t fsinfo_sect;
	uint16_t backup_boot_sect;
	char junk[12];
	uint8_t driveno;
	uint8_t ntflags;
	uint8_t signature;
	uint32_t volume_id;
	char label[11];
	char sysid[8];
} __attribute__((packed));

struct fat_dirent {
	char name[11];
	uint8_t attr;
	uint8_t junk;
	uint8_t ctime_tenths;
	uint16_t ctime_halfsec;
	uint16_t ctime_date;
	uint16_t atime_date;
	uint16_t first_cluster_high;	/* 0 for FAT12 and FAT16 */
	uint16_t mtime_hsec;
	uint16_t mtime_date;
	uint16_t first_cluster_low;
	uint32_t size_bytes;
} __attribute__((packed));

struct fat_lfnent {
	uint8_t seq;
	uint16_t part1[5];
	uint8_t attr;
	uint8_t type;
	uint8_t csum;
	uint16_t part2[6];
	uint16_t zero;
	uint16_t part3[2];
} __attribute__((packed));


struct fat_dir {
	struct fat_dirent *ent;
	int cur_ent;
	int max_nent;
};

struct fat_file {
	struct fat_dirent ent;
	int32_t first_clust;
	int64_t cur_pos;
	int32_t cur_clust;	/* cluster number corresponding to cur_offs */

	char *clustbuf;
	int buf_valid;
};


static void destroy(struct filesys *fs);

static struct fs_node *lookup(struct filesys *fs, const char *path);

static struct fs_node *open(struct filesys *fs, const char *path);
static void close(struct fs_node *node);
static int seek(struct fs_node *node, int offs, int whence);
static int read(struct fs_node *node, void *buf, int sz);
static int write(struct fs_node *node, void *buf, int sz);
static int rewinddir(struct fs_node *node);
static struct fs_dirent *readdir(struct fs_node *node);

static struct fat_dir *load_dir(struct fatfs *fs, struct fat_dirent *dent);
static void free_dir(struct fat_dir *dir);

static struct fat_file *init_file(struct fatfs *fatfs, struct fat_dirent *dent);
static void free_file(struct fat_file *file);

static int read_sector(int dev, uint64_t sidx, void *sect);
static int read_cluster(struct fatfs *fatfs, uint32_t addr, void *clust);
static int dent_filename(struct fat_dirent *dent, struct fat_dirent *prev, char *buf);
static struct fat_dirent *find_entry(struct fat_dir *dir, const char *name);

static uint32_t read_fat(struct fatfs *fatfs, uint32_t addr);
static int32_t next_cluster(struct fatfs *fatfs, int32_t addr);
static int32_t find_cluster(struct fatfs *fatfs, int count, int32_t clust);

static void dbg_printdir(struct fat_dirent *dir, int max_entries);
static void clean_trailws(char *s);

static struct fs_operations fs_fat_ops = {
	destroy,
	open, close,

	seek, read, write,

	rewinddir, readdir
};

static unsigned char sectbuf[512];


struct filesys *fsfat_create(int dev, uint64_t start, uint64_t size)
{
	int i;
	char *endp, *ptr;
	struct filesys *fs;
	struct fatfs *fatfs;
	struct fat_dir *rootdir;
	struct bparam *bpb;
	struct bparam_ext16 *bpb16;
	struct bparam_ext32 *bpb32;

	if(read_sector(dev, start, sectbuf) == -1) {
		return 0;
	}
	bpb = (struct bparam*)sectbuf;
	bpb16 = (struct bparam_ext16*)(sectbuf + sizeof *bpb);
	bpb32 = (struct bparam_ext32*)(sectbuf + sizeof *bpb);

	assert(bpb->sect_bytes == 512);

	if(bpb->jmp[0] != 0xeb || bpb->jmp[2] != 0x90) {
		return 0;
	}

	if(!(fatfs = malloc(sizeof *fatfs))) {
		panic("FAT: create failed to allocate memory for the fat filesystem data\n");
	}
	memset(fatfs, 0, sizeof *fatfs);
	fatfs->dev = dev < 0 ? boot_drive_number : dev;
	fatfs->start_sect = start;
	fatfs->size = bpb->num_sectors ? bpb->num_sectors : bpb->num_sectors32;
	fatfs->cluster_size = bpb->cluster_size;
	fatfs->fat_size = bpb->fat_size ? bpb->fat_size : bpb32->fat_size;
	fatfs->fat_sect = bpb->reserved_sect;
	fatfs->root_sect = fatfs->fat_sect + fatfs->fat_size * bpb->num_fats;
	fatfs->root_size = (bpb->num_dirent * sizeof(struct fat_dirent) + 511) / 512;
	fatfs->first_data_sect = bpb->reserved_sect + bpb->num_fats * fatfs->fat_size + fatfs->root_size;
	fatfs->num_data_sect = fatfs->size - (bpb->reserved_sect + bpb->num_fats * fatfs->fat_size + fatfs->root_size);
	fatfs->num_clusters = fatfs->num_data_sect / fatfs->cluster_size;

	if(fatfs->num_clusters < 4085) {
		fatfs->type = FAT12;
	} else if(fatfs->num_clusters < 65525) {
		fatfs->type = FAT16;
	} else if(fatfs->num_clusters < 268435445) {
		fatfs->type = FAT32;
	} else {
		fatfs->type = EXFAT;
	}

	switch(fatfs->type) {
	case FAT16:
		memcpy(fatfs->label, bpb16->label, sizeof bpb16->label);
		break;

	case FAT32:
	case EXFAT:
		fatfs->root_sect = bpb32->root_clust / fatfs->cluster_size;
		fatfs->root_size = 0;
		memcpy(fatfs->label, bpb32->label, sizeof bpb32->label);
		break;

	default:
		break;
	}

	endp = fatfs->label + sizeof fatfs->label - 2;
	while(endp >= fatfs->label && isspace(*endp)) {
		*endp-- = 0;
	}

	/* read the FAT */
	if(!(fatfs->fat = malloc(fatfs->fat_size * 512))) {
		panic("FAT: failed to allocate memory for the FAT (%lu bytes)\n", (unsigned long)fatfs->fat_size * 512);
	}
	ptr = fatfs->fat;
	for(i=0; i<fatfs->fat_size; i++) {
		read_sector(dev, fatfs->start_sect + fatfs->fat_sect + i, ptr);
		ptr += 512;
	}

	/* open root directory */
	if(!(rootdir = malloc(sizeof *rootdir))) {
		panic("FAT: failed to allocate root directory structure\n");
	}
	if(fatfs->type == FAT32) {
		panic("FAT32 root dir not implemented yet\n");
	} else {
		rootdir->max_nent = fatfs->root_size * 512 / sizeof(struct fat_dirent);
		if(!(rootdir->ent = malloc(fatfs->root_size * 512))) {
			panic("FAT: failed to allocate memory for the root directory\n");
		}
		ptr = (char*)rootdir->ent;

		for(i=0; i<fatfs->root_size; i++) {
			read_sector(dev, fatfs->start_sect + fatfs->root_sect + i, ptr);
			ptr += 512;
		}
	}
	fatfs->rootdir = rootdir;

	/* assume cluster_size is a power of two */
	fatfs->clust_mask = fatfs->cluster_size - 1;
	fatfs->clust_shift = 0;
	while((1 << fatfs->clust_shift) < fatfs->cluster_size) {
		fatfs->clust_shift++;
	}

	/* fill generic fs structure */
	if(!(fs = malloc(sizeof *fs))) {
		panic("FAT: create failed to allocate memory for the filesystem structure\n");
	}
	fs->type = FSTYPE_FAT;
	fs->fsop = &fs_fat_ops;
	fs->data = fatfs;


	printf("opened %s filesystem dev: %x, start: %lld\n", typestr[fatfs->type], fatfs->dev, start);
	if(fatfs->label[0]) {
		printf("  volume label: %s\n", fatfs->label);
	}

	printf("root directory:\n");
	dbg_printdir(fatfs->rootdir->ent, fatfs->rootdir->max_nent);
	putchar('\n');

	/* test */
	{
		struct fs_node *node = lookup(fs, "/readme.md");
		if(!node) {
			printf("failed to find /readme.md\n");
		} else {
			if(node->type != FSNODE_FILE) {
				printf("/readme.md isn't a file\n");
			} else {
				struct fat_file *file = node->data;
				int32_t clustid = file->ent.first_cluster_low;
				uint32_t bytes_left = file->ent.size_bytes;
				char clustbuf[2048];

				printf("contents of /readme.md (%lu bytes)\n", (unsigned long)bytes_left);

				do {
					char *ptr = clustbuf;
					uint32_t sz = bytes_left > 2048 ? 2048 : bytes_left;
					bytes_left -= sz;

					read_cluster(fatfs, clustid, clustbuf);

					for(i=0; i<sz; i++) {
						putchar(*ptr++);
					}

				} while((clustid = next_cluster(fatfs, clustid)) >= 0);
			}
		}
	}

	return fs;
}

static void destroy(struct filesys *fs)
{
	struct fatfs *fatfs = fs->data;
	free(fatfs);
	free(fs);
}

static struct fs_node *lookup(struct filesys *fs, const char *path)
{
	int len;
	char name[MAX_NAME];
	const char *ptr;
	struct fatfs *fatfs = fs->data;
	struct fat_dir *dir, *newdir;
	struct fat_dirent *dent;
	struct fs_node *node;

	if(path[0] == '/') {
		dir = fatfs->rootdir;
		while(*path == '/') path++;
	} else {
		/* TODO */
		panic("FAT: not implemented current directories and relative paths yet\n");
	}

	while(*path) {
		if(!dir) {
			/* we have more path components, yet the last one wasn't a dir */
			return 0;
		}

		ptr = path;
		while(*ptr && *ptr != '/') ptr++;
		len = ptr - path;
		memcpy(name, path, len);
		name[len] = 0;

		while(*ptr == '/') ptr++;	/* skip separators */
		path = ptr;

		if(!(dent = find_entry(dir, name))) {
			return 0;
		}

		newdir = dent->attr == ATTR_DIR ? load_dir(fatfs, dent) : 0;
		if(dir != fatfs->rootdir) {
			free_dir(dir);
		}
		dir = newdir;
	}


	if(!(node = malloc(sizeof *node))) {
		panic("FAT: lookup failed to allocate fs_node structure\n");
	}
	node->fs = fs;
	if(dir) {
		node->type = FSNODE_DIR;
		node->data = dir;
	} else {
		node->type = FSNODE_FILE;
		if(!(node->data = init_file(fatfs, dent))) {
			panic("FAT: failed to allocate file entry structure\n");
		}
	}

	return node;
}

static struct fs_node *open(struct filesys *fs, const char *path)
{
	return lookup(fs, path);
}

static void close(struct fs_node *node)
{
	switch(node->type) {
	case FSNODE_FILE:
		free_file(node->data);
		break;

	case FSNODE_DIR:
		free_dir(node->data);
		break;

	default:
		panic("FAT: close node is not a file nor a dir\n");
	}

	free(node);
}

static int seek(struct fs_node *node, int offs, int whence)
{
	struct fatfs *fatfs;
	struct fat_file *file;
	int64_t new_pos;
	unsigned int cur_clust_idx, new_clust_idx;

	if(node->type != FSNODE_FILE) {
		return -1;
	}

	fatfs = node->fs->data;
	file = node->data;

	switch(whence) {
	case FSSEEK_SET:
		new_pos = offs;
		break;

	case FSSEEK_CUR:
		new_pos = file->cur_pos + offs;
		break;

	case FSSEEK_END:
		new_pos = file->ent.size_bytes + offs;
		break;

	default:
		return -1;
	}

	if(new_pos < 0) new_pos = 0;

	cur_clust_idx = file->cur_pos >> fatfs->clust_shift;
	new_clust_idx = new_pos >> fatfs->clust_shift;
	/* if the new position does not fall in the same cluster as the previous one
	 * re-calculate cur_clust
	 */
	if(new_clust_idx != cur_clust_idx) {
		if(new_clust_idx < cur_clust_idx) {
			file->cur_clust = find_cluster(fatfs, new_clust_idx, file->first_clust);
		} else {
			file->cur_clust = find_cluster(fatfs, new_clust_idx - cur_clust_idx, file->cur_clust);
		}
		file->buf_valid = 0;
	}
	file->cur_pos = new_pos;
	return 0;
}
/* TODO: initialize all the new fields I added to fat_file in init_file */

static int read(struct fs_node *node, void *buf, int sz)
{
}

static int write(struct fs_node *node, void *buf, int sz)
{
}

static int rewinddir(struct fs_node *node)
{
	struct fat_dir *dir;

	if(node->type != FSNODE_DIR) {
		return -1;
	}

	dir = node->data;
	dir->cur_ent = 0;
	return 0;
}

static struct fs_dirent *readdir(struct fs_node *node)
{
	struct fat_dir *dir;

	if(node->type != FSNODE_DIR) {
		return 0;
	}

	dir = node->data;
	if(dir->cur_ent >= dir->max_nent) {
		return 0;
	}

	return dir->ent + dir->cur_ent++;
}

static struct fat_dir *load_dir(struct fatfs *fs, struct fat_dirent *dent)
{
	int32_t addr;
	struct fat_dir *dir;
	char *buf = 0;
	int bufsz = 0;

	if(dent->attr != ATTR_DIR) return 0;

	addr = dent->first_cluster_low;
	if(fs->type >= FAT32) {
		addr |= (uint32_t)dent->first_cluster_high << 16;
	}

	do {
		int prevsz = bufsz;
		bufsz += fs->cluster_size * 512;
		if(!(buf = realloc(buf, bufsz))) {
			panic("FAT: failed to allocate cluster buffer (%d bytes)\n", bufsz);
		}

		if(read_cluster(fs, addr, buf + prevsz) == -1) {
			printf("load_dir: failed to read cluster: %lu\n", (unsigned long)addr);
			free(buf);
			return 0;
		}
	} while((addr = next_cluster(fs, addr)) >= 0);

	if(!(dir = malloc(sizeof *dir))) {
		panic("FAT: failed to allocate directory structure\n");
	}
	dir->ent = (struct fat_dirent*)buf;
	dir->max_nent = bufsz / sizeof *dir->ent;
	dir->cur_ent = 0;
	return dir;
}

static void free_dir(struct fat_dir *dir)
{
	if(dir) {
		free(dir->ent);
		free(dir);
	}
}

static struct fat_file *init_file(struct fatfs *fatfs, struct fat_dirent *dent)
{
	struct fat_file *file;

	if(!(file = calloc(1, sizeof *file))) {
		panic("FAT: failed to allocate file structure\n");
	}
	file->ent = *dent;
	file->cur_offs = 0;
	file->cur_clust = 0;
	return file;
}

static void free_file(struct fat_file *file)
{
	if(file) {
		free(file);
	}
}

static int read_sector(int dev, uint64_t sidx, void *sect)
{
	if(dev == -1 || dev == boot_drive_number) {
		if(bdev_read_sect(sidx, sect) == -1) {
			return -1;
		}
		return 0;
	}

	printf("BUG: reading sectors from drives other than the boot drive not implemented yet\n");
	return -1;
}

static int read_cluster(struct fatfs *fatfs, uint32_t addr, void *clust)
{
	char *ptr = clust;
	int i;
	uint64_t saddr = (uint64_t)(addr - 2) * fatfs->cluster_size + fatfs->first_data_sect + fatfs->start_sect;

	for(i=0; i<fatfs->cluster_size; i++) {
		if(read_sector(fatfs->dev, saddr + i, ptr) == -1) {
			return -1;
		}
		ptr += 512;
	}
	return 0;
}

static int dent_filename(struct fat_dirent *dent, struct fat_dirent *prev, char *buf)
{
	int len = 0;
	char *ptr = buf;
	struct fat_lfnent *lfn = (struct fat_lfnent*)(dent - 1);

	if(lfn > (struct fat_lfnent*)prev && lfn->attr == ATTR_LFN) {
		/* found a long filename entry, use that */
		do {
			uint16_t ustr[14], *uptr = ustr;
			memcpy(uptr, lfn->part1, sizeof lfn->part1);
			uptr += sizeof lfn->part1 / sizeof *lfn->part1;
			memcpy(uptr, lfn->part2, sizeof lfn->part2);
			uptr += sizeof lfn->part2 / sizeof *lfn->part2;
			memcpy(uptr, lfn->part3, sizeof lfn->part3);
			ustr[13] = 0;

			uptr = ustr;
			while(*uptr) {
				*ptr++ = *(char*)uptr++;
				len++;
			}
			*ptr = 0;

			if(uptr < ustr + 13 || (lfn->seq & 0xf0) == 0x40) break;
			lfn -= 1;
		} while(lfn > (struct fat_lfnent*)prev && lfn->attr == ATTR_LFN);

	} else {
		/* regular 8.3 filename */
		memcpy(buf, dent->name, 8);
		buf[8] = 0;
		clean_trailws(buf);
		if(!buf[0]) return 0;

		if(dent->name[8] && dent->name[8] != ' ') {
			ptr = buf + strlen(buf);
			*ptr++ = '.';
			memcpy(ptr, dent->name + 8, 3);
			ptr[3] = 0;
			clean_trailws(ptr);
		}

		len = strlen(buf);
	}
	return len;
}

static struct fat_dirent *find_entry(struct fat_dir *dir, const char *name)
{
	int i;
	struct fat_dirent *dent = dir->ent;
	struct fat_dirent *prev = dent - 1;
	char entname[MAX_NAME];

	for(i=0; i<dir->max_nent; i++) {
		if(DENT_IS_NULL(dent)) break;

		if(!DENT_IS_UNUSED(dent) && dent->attr != ATTR_VOLID && dent->attr != ATTR_LFN) {
			if(dent_filename(dent, prev, entname) > 0) {
				if(strcasecmp(entname, name) == 0) {
					return dent;
				}
			}
		}
		if(dent->attr != ATTR_LFN) {
			prev = dent;
		}
		dent++;
	}

	return 0;
}

static uint32_t read_fat(struct fatfs *fatfs, uint32_t addr)
{
	uint32_t res = 0xffffffff;

	switch(fatfs->type) {
	case FAT12:
		{
			uint32_t idx = addr + addr / 2;
			res = ((uint16_t*)fatfs->fat)[idx];

			if(idx & 1) {
				res >>= 4;		/* odd entries end up on the high 12 bits */
			} else {
				res &= 0xfff;	/* even entries end up on the low 12 bits */
			}
		}
		break;

	case FAT16:
		res = ((uint16_t*)fatfs->fat)[addr];
		break;

	case FAT32:
	case EXFAT:
		res = ((uint32_t*)fatfs->fat)[addr];
		break;

	default:
		break;
	}

	return res;
}

static int32_t next_cluster(struct fatfs *fatfs, int32_t addr)
{
	uint32_t fatval = read_fat(fatfs, addr);

	if(fatval == 0) return -1;

	switch(fatfs->type) {
	case FAT12:
		if(fatval >= 0xff8) return -1;
		break;

	case FAT16:
		if(fatval >= 0xfff8) return -1;
		break;

	case FAT32:
	case EXFAT:	/* XXX ? */
		if(fatval >= 0xffffff8) return -1;
		break;

	default:
		break;
	}

	return fatval;
}

static int32_t find_cluster(struct fatfs *fatfs, int count, int32_t clust)
{
	while(count-- > 0 && (clust = next_cluster(fatfs, clust)) >= 0);
	return clust;
}

static void dbg_printdir(struct fat_dirent *dir, int max_entries)
{
	char name[MAX_NAME];
	struct fat_dirent *prev = dir - 1;
	struct fat_dirent *end = max_entries > 0 ? dir + max_entries : 0;

	while(!DENT_IS_NULL(dir) && (!end || dir < end)) {
		if(!DENT_IS_UNUSED(dir) && dir->attr != ATTR_VOLID && dir->attr != ATTR_LFN) {
			if(dent_filename(dir, prev, name) > 0) {
				printf("%s%c\n", name, dir->attr == ATTR_DIR ? '/' : ' ');
			}
		}
		if(dir->attr != ATTR_LFN) {
			prev = dir;
		}
		dir++;
	}
}

static void clean_trailws(char *s)
{
	char *p;

	if(!s || !*s) return;

	p = s + strlen(s) - 1;
	while(p >= s && isspace(*p)) p--;
	p[1] = 0;
}
