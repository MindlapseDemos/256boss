#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <alloca.h>
#include "image.h"

struct rect {
	int x, y, w, h;
};

int csprite(struct image *img, int x, int y, int xsz, int ysz);
int proc_sheet(const char *fname);
void print_usage(const char *argv0);

int tile_xsz, tile_ysz;
struct rect rect;
int cmap_offs;
int ckey;
int fbpitch = 320;

int main(int argc, char **argv)
{
	int i;
	char *endp;

	for(i=1; i<argc; i++) {
		if(argv[i][0] == '-') {
			if(strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "-size") == 0) {
				if(sscanf(argv[++i], "%dx%d", &tile_xsz, &tile_ysz) != 2 ||
						tile_xsz <= 0 || tile_ysz <= 0) {
					fprintf(stderr, "%s must be followed by WIDTHxHEIGHT\n", argv[i - 1]);
					return 1;
				}

			} else if(strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "-rect") == 0) {
				rect.x = rect.y = 0;
				if(sscanf(argv[++i], "%dx%d+%d+%d", &rect.w, &rect.h, &rect.x, &rect.y) < 2 || rect.w <= 0 || rect.h <= 0) {
					fprintf(stderr, "%s must be followed by WIDTHxHEIGHT+X+Y\n", argv[i - 1]);
					return 1;
				}

			} else if(strcmp(argv[i], "-coffset") == 0) {
				cmap_offs = strtol(argv[++i], &endp, 10);
				if(endp == argv[i] || cmap_offs < 0 || cmap_offs >= 256) {
					fprintf(stderr, "-coffset must be followed by a valid colormap offset\n");
					return 1;
				}

			} else if(strcmp(argv[i], "-fbpitch") == 0) {
				fbpitch = atoi(argv[++i]);
				if(fbpitch <= 0) {
					fprintf(stderr, "-fbpitch must be followed by a positive number\n");
					return 1;
				}

			} else if(strcmp(argv[i], "-k") == 0 || strcmp(argv[i], "-key") == 0) {
				ckey = strtol(argv[++i], &endp, 10);
				if(endp == argv[i] || ckey < 0 || ckey >= 256) {
					fprintf(stderr, "%s must be followed by a valid color key\n", argv[i - 1]);
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
	int i, j, num_xtiles, num_ytiles, xsz, ysz, tidx;
	struct image img;

	if(load_image(&img, fname) == -1) {
		fprintf(stderr, "failed to load image: %s\n", fname);
		return -1;
	}
	if(rect.w <= 0) {
		rect.w = img.width;
		rect.h = img.height;
	}

	if(tile_xsz <= 0) {
		num_xtiles = num_ytiles = 1;
		xsz = rect.w;
		ysz = rect.h;
	} else {
		num_xtiles = rect.w / tile_xsz;
		num_ytiles = rect.h / tile_ysz;
		xsz = tile_xsz;
		ysz = tile_ysz;
	}

	tidx = 0;
	for(i=0; i<num_ytiles; i++) {
		for(j=0; j<num_xtiles; j++) {
			printf("tile%d:\n", tidx++);
			csprite(&img, rect.x + j * xsz, rect.y + i * ysz, xsz, ysz);
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
	int i, j, numops, mode, new_mode, start, skip_acc;
	unsigned char *pptr = img->pixels + y * img->scansz + x;
	struct csop *ops, *optr;

	ops = optr = alloca((xsz + 1) * ysz * sizeof *ops);

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
	skip_acc = 0;
	/* edi points to dest, FBPITCH is framebuffer width in bytes */
	for(i=0; i<numops; i++) {
		switch(optr->op) {
		case CSOP_SKIP:
			skip_acc += optr->len;
			pptr += optr->len;
			break;

		case CSOP_ENDL:
			skip_acc += fbpitch - xsz;
			pptr += img->scansz - xsz;
			break;

		case CSOP_COPY:
			if(skip_acc) {
				printf("\tadd $%d, %%edi\n", skip_acc);
				skip_acc = 0;
			}

			for(j=0; j<optr->len / 4; j++) {
				printf("\tmovl $0x%x, %d(%%edi)\n", *(uint32_t*)pptr, j * 4);
				pptr += 4;
			}
			j *= 4;
			switch(optr->len % 4) {
			case 3:
				printf("\tmovb $0x%x, %d(%%edi)\n", (unsigned int)*pptr++, j++);
			case 2:
				printf("\tmovw $0x%x, %d(%%edi)\n", (unsigned int)*(uint16_t*)pptr, j);
				pptr += 2;
				j += 2;
				break;
			case 1:
				printf("\tmovb $0x%x, %d(%%edi)\n", (unsigned int)*pptr++, j++);
				break;
			}

			skip_acc = optr->len;
			break;

		default:
			printf("\t# invalid op: %d\n", optr->op);
		}
		optr++;
	}

	return 0;
}

void print_usage(const char *argv0)
{
	printf("Usage: %s [options] <spritesheet>\n", argv0);
	printf("Options:\n");
	printf(" -s,-size <WxH>: tile size (default: whole image)\n");
	printf(" -r,-rect <WxH+X+Y>: use rectangle of the input image (default: whole image)\n");
	printf(" -coffset <offs>: colormap offset [0, 255] (default: 0)\n");
	printf(" -fbpitch <pitch>: target framebuffer pitch (scanline size in bytes)\n");
	printf(" -k,-key <color>: color-key for transparency (default: 0)\n");
	printf(" -h: print usage and exit\n");
}
