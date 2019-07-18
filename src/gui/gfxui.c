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
#include "vbe.h"
#include "ui/fsview.h"

int gfxui(void)
{
	struct vbe_edid edid;
	struct video_mode vinf;
	int i, xres, yres, mode = -1;

	if(vbe_get_edid(&edid) == 0 && edid_preferred_resolution(&edid, &xres, &yres) == 0) {
		printf("EDID: preferred resolution: %dx%d\n", xres, yres);
		mode = find_video_mode(xres, yres, 32);
	}

	if(mode == -1) {

	}

	return 0;
}
