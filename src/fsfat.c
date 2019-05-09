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
#include <inttypes.h>
#include "fs.h"
#include "bootdev.h"
#include "boot.h"
#include "panic.h"

enum { FAT12, FAT16, FAT32, EXFAT };
static const char *typestr[] = { "fat12", "fat16", "fat32", "exfat" };

struct fatfs {
	int type;
	int dev;
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
	uint32_t num_root_clusters;
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

static void *create(int dev, uint64_t start, uint64_t size);
static void destroy(void *fsdata);
static struct fs_node *lookup(void *fsdata, const char *path);

static int read_sector(int dev, uint64_t sidx, void *sect);

struct fs_operations fs_fat_ops = {
	create,
	destroy,
	lookup
};

static unsigned char sectbuf[512];


static void *create(int dev, uint64_t start, uint64_t size)
{
	struct fatfs *fs;
	struct bparam *bpb;
	/*struct bparam_ext16 *bpb16;*/
	struct bparam_ext32 *bpb32;

	if(read_sector(dev, start, sectbuf) == -1) {
		return 0;
	}
	bpb = (struct bparam*)sectbuf;
	/*bpb16 = (struct bparam_ext16*)sectbuf + sizeof *bpb;*/
	bpb32 = (struct bparam_ext32*)sectbuf + sizeof *bpb;

	if(bpb->jmp[0] != 0xeb || bpb->jmp[2] != 0x90) {
		return 0;
	}

	if(!(fs = malloc(sizeof *fs))) {
		panic("FAT: create failed to allocate memory for the filesystem structure\n");
	}
	fs->dev = dev < 0 ? boot_drive_number : dev;
	fs->size = bpb->num_sectors ? bpb->num_sectors : bpb->num_sectors32;
	fs->cluster_size = bpb->cluster_size;
	fs->fat_size = bpb->fat_size ? bpb->fat_size : bpb32->fat_size;
	fs->fat_sect = bpb->reserved_sect;
	fs->root_sect = 0;	/* TODO */
	fs->root_size = (bpb->num_dirent * sizeof(struct fat_dirent) + bpb->sect_bytes - 1) / bpb->sect_bytes;
	fs->first_data_sect = bpb->reserved_sect + bpb->num_fats * fs->fat_size + fs->root_size;
	fs->num_data_sect = fs->size - (bpb->reserved_sect + bpb->num_fats * fs->fat_size + fs->root_size);
	fs->num_clusters = fs->num_data_sect / fs->cluster_size;

	if(fs->num_clusters < 4085) {
		fs->type = FAT12;
	} else if(fs->num_clusters < 65525) {
		fs->type = FAT16;
	} else if(fs->num_clusters < 268435445) {
		fs->type = FAT32;
	} else {
		fs->type = EXFAT;
	}

	printf("opened %s filesystem dev: %x, start: %lld\n", typestr[fs->type], fs->dev, start);
	printf("  size: %lu sectors (%llu bytes)\n", (unsigned long)fs->size,
			(unsigned long long)fs->size * bpb->sect_bytes);
	printf("  sector size: %d bytes\n", bpb->sect_bytes);
	printf("  %lu clusters (%d sectors each)\n", (unsigned long)fs->num_clusters,
			fs->cluster_size);
	printf("  FAT starts at: %lu\n", (unsigned long)fs->fat_sect);
	printf("  root dir at: %lu (%d sectors)\n", (unsigned long)fs->root_sect, fs->root_size);
	printf("  data sectors start at: %lu (%lu sectors, %lu clusters)\n\n",
			(unsigned long)fs->first_data_sect, (unsigned long)fs->num_data_sect,
			(unsigned long)fs->num_clusters);

	return fs;
}

static void destroy(void *fsdata)
{
	struct fatfs *fs = fsdata;

	free(fs);
}

static struct fs_node *lookup(void *fsdata, const char *path)
{
	return 0;	/* TODO */
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
