// ext2 文件系统，只读，当前支持读超级块、读 inode、列根目录
#include <stdint.h>
#include "kernel.h"
#include "ext2.h"
#include "strings.h"

#define EXT2_PARTITION_LBA	2048
#define EXT2_SUPER_OFFSET	1024
#define EXT2_SUPER_LBA		(EXT2_PARTITION_LBA + EXT2_SUPER_OFFSET / 512)

#define EXT2_MAGIC		0xEF53

#define EXT2_ROOT_INO		2

// 超级块
struct ext2_superblock {
	uint32_t inodes_count;
	uint32_t blocks_count;
	uint32_t reserved_blocks_count;
	uint32_t free_blocks_count;
	uint32_t free_inodes_count;
	uint32_t first_data_block;
	uint32_t log_block_size;
	uint32_t log_frag_size;
	uint32_t blocks_per_group;
	uint32_t frags_per_group;
	uint32_t inodes_per_group;
	uint32_t mtime;
	uint32_t wtime;
	uint16_t mnt_count;
	uint16_t max_mnt_count;
	uint16_t magic;
	uint16_t state;
	uint16_t errors;
	uint16_t minor_rev_level;
	uint32_t lastcheck;
	uint32_t checkinterval;
	uint32_t creator_os;
	uint32_t rev_level;
	uint16_t def_resuid;
	uint16_t def_resgid;
	uint32_t first_ino;
	uint16_t inode_size;
	uint16_t block_group_nr;
	uint32_t feature_compat;
	uint32_t feature_incompat;
	uint32_t feature_ro_compat;
};

// 块组描述符
struct ext2_group_desc {
	uint32_t bg_block_bitmap;
	uint32_t bg_inode_bitmap;
	uint32_t bg_inode_table;
	uint16_t bg_free_blocks_count;
	uint16_t bg_free_inodes_count;
	uint16_t bg_used_dirs_count;
	uint16_t bg_pad;
	uint32_t bg_reserved[3];
};

// 目录项
struct ext2_dir_entry {
	uint32_t inode;
	uint16_t rec_len;
	uint8_t name_len;
	uint8_t file_type;
	char name[];
};

static struct ext2_superblock sb;
static struct ext2_inode root_ino;
static uint32_t block_size;
static int ext2_ok = 0;

static uint8_t block_buf[4096];

static int ext2_read_block(uint32_t block, void *buf) {
	uint32_t lba = EXT2_PARTITION_LBA + block * (block_size / 512);
	uint8_t count = block_size / 512;
	return disk_read_sectors(lba, count, buf);
}

int ext2_read_inode(uint32_t ino, struct ext2_inode *out) {
	uint32_t index = ino - 1;
	uint32_t group = index / sb.inodes_per_group;
	uint32_t offset = index % sb.inodes_per_group;

	uint32_t gdt_block = (1024 / block_size) + 1;
	uint32_t gdt_offset = group * sizeof(struct ext2_group_desc);
	uint32_t gdt_lba = EXT2_PARTITION_LBA + gdt_block * (block_size / 512) + gdt_offset / 512;

	uint8_t tmp[512];
	disk_read_sectors(gdt_lba, 1, tmp);

	uint8_t *p = tmp + (gdt_offset % 512);
	struct ext2_group_desc *g = (struct ext2_group_desc *)p;

	uint32_t inode_table = g->bg_inode_table;

	uint32_t inode_size = sb.inode_size;
	if (inode_size == 0) {
		inode_size = 128;
	}

	uint32_t byte_off = offset * inode_size;
	uint32_t lba2 = EXT2_PARTITION_LBA + inode_table * (block_size / 512) + byte_off / 512;

	uint8_t tmp2[512];
	disk_read_sectors(lba2, 1, tmp2);

	uint8_t *p2 = tmp2 + (byte_off % 512);
	struct ext2_inode *ip = (struct ext2_inode *)p2;

	for (uint32_t i = 0; i < sizeof(struct ext2_inode); i++) {
		((uint8_t *)out)[i] = ((uint8_t *)ip)[i];
	}

	return 0;
}

void ext2_init(void) {
	uint8_t buf[512];

	disk_read_sectors(EXT2_SUPER_LBA, 1, buf);

	uint8_t *p = buf + (EXT2_SUPER_OFFSET % 512);
	struct ext2_superblock *s = (struct ext2_superblock *)p;

	if (s->magic != EXT2_MAGIC) {
		ext2_ok = 0;
		return;
	}

	for (uint32_t i = 0; i < sizeof(struct ext2_superblock); i++) {
		((uint8_t *)&sb)[i] = ((uint8_t *)s)[i];
	}

	block_size = 1024u << sb.log_block_size;

	ext2_read_inode(EXT2_ROOT_INO, &root_ino);

	ext2_ok = 1;
}

void ext2_print_info(void) {
	if (!ext2_ok) {
		print("ext2: not found\n");
		return;
	}

	print("ext2: inodes=");
	print_uint(sb.inodes_count);
	print(", blocks=");
	print_uint(sb.blocks_count);
	print(", block_size=");
	print_uint(block_size);
	print("\n");
}

void ext2_ls_root(void) {
	if (!ext2_ok) {
		print("ext2: not mounted\n");
		return;
	}

	ext2_read_block(root_ino.i_block[0], block_buf);

	uint32_t offset = 0;

	while (offset < block_size) {
		struct ext2_dir_entry *de = (struct ext2_dir_entry *)(block_buf + offset);

		if (de->inode == 0) {
			break;
		}

		if (de->name_len > 0) {
			for (int i = 0; i < de->name_len; i++) {
				print_char(de->name[i]);
			}
			print_char('\n');
		}

		offset += de->rec_len;
	}
}

// 按路径查找 inode，成功返回 0 并写入 ino，失败返回 -1
int ext2_lookup(const char *path, uint32_t *ino) {
	uint32_t cur = EXT2_ROOT_INO;

	const char *p = path;

	while (*p == '/') {
		p++;
	}

	if (*p == '\0') {
		*ino = cur;
		return 0;
	}

	struct ext2_inode inode;
	ext2_read_inode(cur, &inode);

	while (*p != '\0') {
		char name[256];
		int n = 0;

		while (*p != '\0' && *p != '/') {
			name[n++] = *p++;
		}

		name[n] = '\0';

		while (*p == '/') {
			p++;
		}

		int found = 0;

		for (int b = 0; b < 12; b++) {
			if (inode.i_block[b] == 0) {
				break;
			}

			ext2_read_block(inode.i_block[b], block_buf);

			uint32_t offset = 0;

			while (offset < block_size) {
				struct ext2_dir_entry *de = (struct ext2_dir_entry *)(block_buf + offset);

				if (de->inode == 0) {
					break;
				}

				if (de->rec_len == 0) {
					break;
				}

				if (de->name_len == n) {
					int match = 1;

					for (int i = 0; i < n; i++) {
						if (de->name[i] != name[i]) {
							match = 0;
							break;
						}
					}

					if (match) {
						cur = de->inode;
						found = 1;
						break;
					}
				}

				offset += de->rec_len;

				if (offset + de->rec_len > block_size) {
					break;
				}
			}

			if (found) {
				break;
			}
		}

		if (!found) {
			return -1;
		}

		ext2_read_inode(cur, &inode);
	}

	*ino = cur;
	return 0;
}

void ext2_ls(const char *path) {
	if (!ext2_ok) {
		print("ext2: not mounted\n");
		return;
	}

	uint32_t ino;

	if (ext2_lookup(path, &ino) != 0) {
		return;
	}

	struct ext2_inode inode;
	ext2_read_inode(ino, &inode);

	if ((inode.i_mode & 0xF000) != 0x4000) {
		return;
	}

	for (int b = 0; b < 12; b++) {
		if (inode.i_block[b] == 0) {
			break;
		}

		ext2_read_block(inode.i_block[b], block_buf);

		uint32_t offset = 0;

		while (offset < block_size) {
			struct ext2_dir_entry *de = (struct ext2_dir_entry *)(block_buf + offset);

			if (de->inode == 0) {
				break;
			}

			if (de->rec_len == 0) {
				break;
			}

			int skip = 0;

			if (de->name_len == 1 && de->name[0] == '.') {
				skip = 1;
			}

			if (de->name_len == 2 && de->name[0] == '.' && de->name[1] == '.') {
				skip = 1;
			}

			if (!skip && de->name_len > 0) {
				for (int i = 0; i < de->name_len; i++) {
					print_char(de->name[i]);
				}

				print_char('\n');
			}

			offset += de->rec_len;

			if (offset + de->rec_len > block_size) {
				break;
			}
		}
	}
}