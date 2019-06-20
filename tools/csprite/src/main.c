#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <alloca.h>
#include "image.h"

int csprite(struct image *img, int x, int y, int xsz, int ysz);
int proc_sheet(const char *fname);
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
			if(proc_sheet(argv[i]) == -1) {
				return 1;
			}
		}
	}

	return 0;
}

int proc_sheet(const char *fname)
{
	int i, j, num_xtiles, num_ytiles, xsz, ysz;
	struct image img;

	if(load_image(&img, fname) == -1) {
		fprintf(stderr, "failed to load image: %s\n", fname);
		return -1;
	}

	if(tile_xsz <= 0) {
		num_xtiles = num_ytiles = 1;
		xsz = img.width;
		ysz = img.height;
	} else {
		num_xtiles = img.width / tile_xsz;
		num_ytiles = img.height / tile_ysz;
		xsz = tile_xsz;
		ysz = tile_ysz;
	}

	for(i=0; i<num_ytiles; i++) {
		for(j=0; j<num_xtiles; j++) {
			csprite(&img, j * xsz, i * ysz, xsz, ysz);
		}
	}

	return 0;
}

enum {
	CSOP_SKIP,
	CSOP_FILL,
	CSOP_COPY,
	CSOP_ENDL
};

struct csop {
	unsigned char op, val;
	int len;
};

int csprite(struct image *img, int x, int y, int xsz, int ysz)
{
	int i, j, numops, mode = -1, new_mode, start = -1;
	unsigned char *pptr = img->pixels + y * img->scansz + x;
	struct csop *ops, *optr;

	ops = optr = alloca(xsz * ysz * sizeof *ops);

	for(i=0; i<ysz; i++) {
		mode = -1;
		start = -1;

		if(i > 0) {
			optr++->op = CSOP_ENDL;
		}

		for(j=0; j<xsz; j++) {
			if(*pptr == ckey) {
				new_mode = CSOP_SKIP;
			} else {
				new_mode = CSOP_COPY;
			}

			if(new_mode != mode) {
				if(mode != -1) {
					assert(start >= 0);
					optr->op = mode;
					optr->len = j - start;
					optr++;
				}
				mode = new_mode;
				start = j;
			}
			pptr++;
		}
		pptr += img->scansz - xsz;

		if(mode != -1) {
			assert(start >= 0);
			optr->op = mode;
			optr->len = xsz - start;
			optr++;
		}
	}
	numops = optr - ops;

	pptr = img->pixels + y * img->scansz + x;
	optr = ops;
	/* edi points to dest, FBPITCH is framebuffer width in bytes */
	for(i=0; i<numops; i++) {
		switch(optr->op) {
		case CSOP_SKIP:
			printf("\tadd $%d, %%edi\n", optr->len);
			pptr += optr->len;
			break;

		case CSOP_ENDL:
			printf("\tadd $FBPITCH-%d, %%edi\n", xsz);
			pptr += img->scansz - xsz;
			break;

		case CSOP_COPY:
			for(j=0; j<optr->len; j++) {
				printf("\tmov $0x%x, %d(%%edi)\n", (unsigned int)*pptr++, j);
			}
			printf("\tadd $%d, %%edi\n", optr->len);
			break;

		default:
			printf("\t# invalid op: %d\n", optr->op);
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
