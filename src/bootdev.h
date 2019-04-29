#ifndef BOOTDEV_H_
#define BOOTDEV_H_

void bdev_init(void);

int bdev_read_sect(uint64_t lba, void *buf);
int bdev_write_sect(uint64_t lba, void *buf);

#endif	/* BOOTDEV_H_ */
