#include <stdio.h>
#include "image.h"

struct sprite {
	int width, height;
	unsigned char *data;
	char *code;

	struct sprite *next;
};

int proc_file(const char *fname);
void print_usage(const char *argv0);

int tile_xsz, tile_ysz;
int cmap_offs;
int ckey;

int main(int argc, char **argv)
{
	int i;
	char *endp;

	for(i=1; i<argc; i++) {
		if(argv[i][0] == '-') {
			if(strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "-size") == 0) {
				if(sscanf(argv[++i], "%dx%d", &tile_xsz, &tile_ysz) != 2 ||
						tile_xsz <= 0 || tile_ysz <= 0) {
					fprintf(stderr, "-s must be followed by WIDTHxHEIGHT\n");
					return 1;
				}

			} else if(strcmp(argv[i], "-coffset") == 0) {
				cmap_offs = strtol(argv[++i], &endp, 10);
				if(endp == argv[i] || cmap_offs < 0 || cmap_offs >= 256) {
					fprintf(stderr, "-coffset must be followed by a valid colormap offset\n");
					return 1;
				}

			} else if(strcmp(argv[i], "-k") == 0 || strcmp(argv[i], "-key") == 0) {
				ckey = strtol(argv[++i], &endp, 10);
				if(endp == argv[i] || ckey < 0 || ckey >= 256) {
					fprintf(stderr, "%s must be followed by a valid color key\n", argv[-1]);
					return 1;
				}

			} else if(strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "-help") == 0) {
				print_usage(argv[0]);
				return 0;

			} else {
				fprintf(stderr, "invalid option: %s\n", argv[i]);
				print_usage(argv[0]);
				return 1;
			}

		} else {
			if(proc_file(argv[i]) == -1) {
				return 1;
			}
		}
	}

	return 0;
}

int proc_file(const char *fname)
{
	int i, j, num_xtiles, num_ytiles;
	struct image img;
	struct sprite spr;
	unsigned char *pptr;

	if(load_image(&img, fname) == -1) {
		fprintf(stderr, "failed to load image: %s\n", fname);
		return -1;
	}

	if(tile_xsz <= 0) {
		num_xtiles = num_ytiles = 1;
		spr.width = img.width;
		spr.height = img.height;
	} else {
		num_xtiles = img.width / tile_xsz;
		num_ytiles = img.help / tile_ysz;
		spr.width = tile_xsz;
		spr.height = tile_ysz;
	}

	pptr = img.pixels;
	for(i=0; i<num_ytiles; i++) {
		for(j=0; j<num_xtiles; j++) {
			csprite(&spr, pptr, img.width);
		}
	}

	return 0;
}

void print_usage(const char *argv0)
{
	printf("Usage: %s [options] <spritesheet>\n", argv0);
	printf("Options:\n");
	printf(" -s,-size <WxH>: tile size (default: whole image)\n");
	printf(" -coffset <offs>: colormap offset [0, 255] (default: 0)\n");
	printf(" -k,-key <color>: color-key for transparency (default: 0)\n");
	printf(" -h: print usage and exit\n");
}
