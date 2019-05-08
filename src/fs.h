#ifndef FS_H_
#define FS_H_

enum {
	FSTYPE_FAT,

	NUM_FSTYPES
};

enum {
	FSNODE_DIR,
	FSNODE_FILE
};

struct filesystem;

struct fs_node {
	struct filesystem *fs;
	int type;
	int nref;
	void *ndata;
};

struct fs_operations {
	void *(*init)(int dev, uint64_t start, uint64_t size);
	void (*destroy)(void *fsdata);

	struct fs_node *(*lookup)(void *fsdata, const char *path);
};

struct filesystem {
	int type;
	struct fs_node *parent;
	struct fs_operations *fsop;
	void *fsdata;
};

struct fs_node *root;

int fs_mount(int dev, uint64_t start, uint64_t size, struct fs_node *parent);

#endif	/* FS_H_ */
