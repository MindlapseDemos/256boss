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

struct fs_operations fs_fat_ops;

static struct fs_operations *fsops[] = {
	&fs_fat_ops,
	0
};

int fs_mount(int dev, uint64_t start, uint64_t size, struct fs_node *parent)
{
	int i;
	void *fsdata;

	if(!parent && fs_root) {
		printf("fs_mount error: root filesystem already mounted!\n");
		return -1;
	}

	for(i=0; fsops[i]; i++) {
		/* TODO: create should return a struct filesystem, which should contain the
		 * per-filesystem data pointer in it
		 */
		if((fsdata = fsops[i]->create(dev, start, size))) {
			return 0;
		}
	}

	printf("failed to mount filesystem dev: %d, start %llu\n", dev, (unsigned long long)start);
	return -1;
}
