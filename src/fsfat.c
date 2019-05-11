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
#include "fs.h"
#include "bootdev.h"
#include "boot.h"
#include "panic.h"

#define DIRENT_UNUSED	0xe5

enum { FAT12, FAT16, FAT32, EXFAT };
static const char *typestr[] = { "fat12", "fat16", "fat32", "exfat" };

struct fat_dirent;

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

	struct fat_dirent *rootdir;
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

static void destroy(struct filesys *fs);

static struct fs_dir *opendir(struct filesys *fs, const char *path);
static void closedir(struct filesys *fs, struct fs_dir *dir);

static int read_sector(int dev, uint64_t sidx, void *sect);
static void dbg_printdir(struct fat_dirent *dir, int max_entries);

static struct fs_operations fs_fat_ops = {
	destroy,
	opendir, closedir
};

static unsigned char sectbuf[512];


struct filesys *fsfat_create(int dev, uint64_t start, uint64_t size)
{
	int i;
	char *endp, *ptr;
	struct filesys *fs;
	struct fatfs *fatfs;
	struct fat_dirent *rootdir;
	struct bparam *bpb;
	struct bparam_ext16 *bpb16;
	struct bparam_ext32 *bpb32;

	if(read_sector(dev, start, sectbuf) == -1) {
		return 0;
	}
	bpb = (struct bparam*)sectbuf;
	bpb16 = (struct bparam_ext16*)(sectbuf + sizeof *bpb);
	bpb32 = (struct bparam_ext32*)(sectbuf + sizeof *bpb);

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
	fatfs->root_size = (bpb->num_dirent * sizeof(struct fat_dirent) + bpb->sect_bytes - 1) / bpb->sect_bytes;
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
		memcpy(fatfs->label, bpb32->label, sizeof bpb32->label);
		break;

	default:
		break;
	}

	endp = fatfs->label + sizeof fatfs->label - 2;
	while(endp >= fatfs->label && isspace(*endp)) {
		*endp-- = 0;
	}

	/* open root directory */
	if(!(rootdir = malloc(fatfs->root_size * 512))) {
		panic("FAT: failed to allocate memory for the root directory\n");
	}
	ptr = (char*)rootdir;

	for(i=0; i<fatfs->root_size; i++) {
		read_sector(dev, fatfs->start_sect + fatfs->root_sect + i, ptr);
		ptr += 512;
	}

	if(!(fs = malloc(sizeof *fs))) {
		panic("FAT: create failed to allocate memory for the filesystem structure\n");
	}
	fs->type = FSTYPE_FAT;
	fs->fsop = &fs_fat_ops;
	fs->data = fatfs;
	fs->root = 0;//fatroot;


	printf("opened %s filesystem dev: %x, start: %lld\n", typestr[fatfs->type], fatfs->dev, start);
	if(fatfs->label[0]) {
		printf("  volume label: %s\n", fatfs->label);
	}
	printf("  size: %lu sectors (%llu bytes)\n", (unsigned long)fatfs->size,
			(unsigned long long)fatfs->size * bpb->sect_bytes);
	printf("  sector size: %d bytes\n", bpb->sect_bytes);
	printf("  %lu clusters (%d sectors each)\n", (unsigned long)fatfs->num_clusters,
			fatfs->cluster_size);
	printf("  FAT starts at: %lu\n", (unsigned long)fatfs->fat_sect);
	printf("  root dir at: %lu (%d sectors)\n", (unsigned long)fatfs->root_sect, fatfs->root_size);
	printf("  data sectors start at: %lu (%lu sectors, %lu clusters)\n\n",
			(unsigned long)fatfs->first_data_sect, (unsigned long)fatfs->num_data_sect,
			(unsigned long)fatfs->num_clusters);

	dbg_printdir(rootdir, fatfs->root_size * 512 / sizeof(struct fat_dirent));

	return fs;
}

static void destroy(struct filesys *fs)
{
	struct fatfs *fatfs = fs->data;
	free(fatfs);
	free(fs);
}

static struct fs_dir *opendir(struct filesys *fs, const char *path)
{
	return 0;	/* TODO */
}

static void closedir(struct filesys *fs, struct fs_dir *dir)
{
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

#define DENT_IS_NULL(dent)	(((unsigned char*)(dent))[0] == 0)
#define DENT_IS_UNUSED(dent)	(((unsigned char*)(dent))[0] == DIRENT_UNUSED)
#define DENT_LFN_SEQ(dent)	(((unsigned char*)(dent))[0] & 0xf)
#define DENT_LFN_LAST(dent)	((((unsigned char*)(dent))[0] & 0xf0) == 0x40)

#define ATTR_RO		0x01
#define ATTR_HIDDEN	0x02
#define ATTR_SYSTEM	0x04
#define ATTR_VOLID	0x08
#define ATTR_DIR	0x10
#define ATTR_ARCHIVE	0x20
#define ATTR_LFN	0xf

static int dent_filename(struct fat_dirent *dent, struct fat_dirent *prev, char *buf)
{
	int len = 0;
	char *ptr = buf;
	struct fat_lfnent *lfn = (struct fat_lfnent*)(dent - 1);

	if(lfn > prev && lfn->attr == ATTR_LFN) {
		/* found a long filename entry, use that */
		do {
			uint16_t ustr[14], *uptr = ustr;
			memcpy(uptr, lfn->part1, sizeof lfn->part1), uptr += sizeof lfn->part1;
			memcpy(uptr, lfn->part2, sizeof lfn->part2), uptr += sizeof lfn->part2;
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
		} while(lfn > prev && lfn->attr == ATTR_LFN);

	} else {
		/* regular 8.3 filename */
		memcpy(dent->name, buf, 8);
		ptr = buf + 7;
		while(ptr >= buf && isspace(*ptr)) {
			*ptr-- = 0;
		}
		if(buf[0] == 0) return 0;

		ptr++;
		memcpy(ptr, dent->name + 8, 3);
		ptr += 2;
		while(ptr >= buf && isspace(*ptr)) {
			*ptr-- = 0;
		}
		*++ptr = 0;

		len = ptr - buf;
	}
	return len;
}

static void dbg_printdir(struct fat_dirent *dir, int max_entries)
{
	char name[256];
	struct fat_dirent *prev = dir - 1;
	struct fat_dirent *end = max_entries > 0 ? dir + max_entries : 0;

	printf("DBG directory listing\n");

	name[11] = 0;
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
