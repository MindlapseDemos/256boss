#ifndef PART_H_
#define PART_H_

#include <inttypes.h>

#define PART_ACT_BIT	(1 << 9)
#define PART_PRIM_BIT	(1 << 10)

#define PART_TYPE(attr)		((attr) & 0xff)
#define PART_IS_ACT(attr)	((attr) & PART_ACT_BIT)
#define PART_IS_PRIM(attr)	((attr) & PART_PRIM_BIT)

struct partition {
	uint32_t start_sect;
	size_t size_sect;

	unsigned int attr;
};

int read_partitions(int dev, struct partition *ptab, int ptabsz);
void print_partition_table(struct partition *ptab, int npart);

#endif	/* PART_H_ */
