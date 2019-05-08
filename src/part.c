#include <stdio.h>
#include "part.h"
#include "boot.h"
#include "bootdev.h"
#include "ptype.h"

struct part_record {
	uint8_t stat;
	uint8_t first_head, first_cyl, first_sect;
	uint8_t type;
	uint8_t last_head, last_cyl, last_sect;
	uint32_t first_lba;
	uint32_t nsect_lba;
} __attribute__((packed));

static int read_sector(int dev, uint64_t sidx);
static const char *printsz(unsigned int sz_sect);
static const char *ptype_name(int type);

static unsigned char sectdata[512];

#define BOOTSIG_OFFS	510
#define PTABLE_OFFS		0x1be

#define BOOTSIG			0xaa55

#define IS_MBR			(sidx == 0)
#define IS_FIRST_EBR	(!IS_MBR && (first_ebr_offs == 0))

int read_partitions(int dev, struct partition *ptab, int ptabsz)
{
	int i, num_rec, nparts = 0;
	int num_bootrec = 0;
	struct partition *part = ptab;
	struct part_record *prec;
	uint64_t sidx = 0;
	uint64_t first_ebr_offs = 0;

	if(ptabsz <= 0) {
		ptab = 0;
	}

	do {
		if(IS_FIRST_EBR) {
			first_ebr_offs = sidx;
		}

		if(read_sector(dev, sidx) == -1) {
			printf("failed to read sector %llu\n", (unsigned long long)sidx);
			return -1;
		}
		if(*(uint16_t*)(sectdata + BOOTSIG_OFFS) != BOOTSIG) {
			printf("invalid partitionm table, sector %llu has no magic\n", (unsigned long long)sidx);
			return -1;
		}
		prec = (struct part_record*)(sectdata + PTABLE_OFFS);

		/* MBR has 4 records, EBRs have 2 */
		num_rec = IS_MBR ? 4 : 2;

		for(i=0; i<num_rec; i++) {
			/* ignore empty partitions in the MBR, stop on empty partitions in an EBR */
			if(prec[i].type == 0) {
				if(num_bootrec > 0) {
					sidx = 0;
					break;
				}
				continue;
			}

			/* ignore extended partitions and setup sector index to read the
			 * next logical partition.
			 */
			if(prec[i].type == PTYPE_EXT || prec[i].type == PTYPE_EXT_LBA) {
				/* all EBR start fields are relative to the first EBR offset */
				sidx = first_ebr_offs + prec[i].first_lba;
				continue;
			}

			/* found a proper partition */
			nparts++;

			if(!ptab) {
				part->attr = prec[i].type;

				if(prec[i].stat & 0x80) {
					part->attr |= PART_ACT_BIT;
				}
				if(IS_MBR) {
					part->attr |= PART_PRIM_BIT;
				}
				part->start_sect = prec[i].first_lba + first_ebr_offs;
				part->size_sect = prec[i].nsect_lba;
			}
		}

		num_bootrec++;
	} while(sidx > 0 && (!ptab || nparts < ptabsz));

	return nparts;
}

void print_partition_table(struct partition *ptab, int npart)
{
	int i;
	struct partition *p = ptab;

	printf("Partition table\n");
	printf("---------------\n");
	for(i=0; i<npart; i++) {
		printf("%d%c ", i, PART_IS_ACT(p->attr) ? '*' : ' ');
		printf("(%s) %-20s ", PART_IS_PRIM(p->attr) ? "pri" : "log", ptype_name(PART_TYPE(p->attr)));
		printf("start: %-10llu ", (unsigned long long)p->start_sect);
		printf("size: %-10llu [%s]\n", (unsigned long long)p->size_sect, printsz(p->size_sect));
		p++;
	}
	printf("---------------\n");
}

static int read_sector(int dev, uint64_t sidx)
{
	if(dev == -1 || dev == boot_drive_number) {
		if(bdev_read_sect(sidx, sectdata) == -1) {
			return -1;
		}
		return 0;
	}

	printf("BUG: reading partitions of drives other than the boot drive not implemented yet\n");
	return -1;
}

static const char *printsz(unsigned int sz_sect)
{
	int i = 0;
	const char *suffix[] = { "kb", "mb", "gb", "tb", "pb", 0 };
	static char buf[128];

	float sz = (float)sz_sect / 2.0;

	while(sz > 1024.0 && suffix[i + 1]) {
		sz /= 1024.0;
		i++;
	}

	sprintf(buf, "%.1f %s", sz, suffix[i]);
	return buf;
}

static const char *ptype_name(int type)
{
	int i;

	for(i=0; i<PTYPES_SIZE; i++) {
		if(partypes[i].type == type) {
			return partypes[i].name;
		}
	}
	return "unknown";
}
