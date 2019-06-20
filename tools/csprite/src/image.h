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
#ifndef IMAGE_H_
#define IMAGE_H_

struct cmapent {
	unsigned char r, g, b;
};

struct image {
	int width, height;
	int bpp;
	int nchan;
	int scansz, pitch;
	int cmap_ncolors;
	struct cmapent cmap[256];
	unsigned char *pixels;
};

int alloc_image(struct image *img, int x, int y, int bpp);
int load_image(struct image *img, const char *fname);
int save_image(struct image *img, const char *fname);

int cmp_image(struct image *a, struct image *b);

void blit_image(struct image *src, int sx, int sy, int w, int h, struct image *dst, int dx, int dy);

void image_color_offset(struct image *img, int offs);

#endif	/* IMAGE_H_ */
