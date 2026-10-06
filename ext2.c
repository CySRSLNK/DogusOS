// ext2 文件系统，支持读超级块、读写 inode、列目录、创建、删除、写文件
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

// 由 i_mode 高四位返回类型字符
static char ext2_type_char(uint16_t mode) {
	switch (mode & 0xF000) {
	case 0x8000:
		return '-';
	case 0x4000:
		return 'd';
	case 0xA000:
		return 'l';
	case 0x2000:
		return 'c';
	case 0x6000:
		return 'b';
	case 0x1000:
		return 'p';
	case 0xC000:
		return 's';
	default:
		return '?';
	}
}

// 读一个块
static int ext2_read_block(uint32_t block, void *buf) {
	uint32_t lba = EXT2_PARTITION_LBA + block * (block_size / 512);
	uint8_t count = block_size / 512;
	return disk_read_sectors(lba, count, buf);
}

// 写一个块
static int ext2_write_block(uint32_t block, const void *buf) {
	uint32_t lba = EXT2_PARTITION_LBA + block * (block_size / 512);
	uint8_t count = block_size / 512;
	return disk_write_sectors(lba, count, buf);
}

// 写回超级块
static void ext2_write_super(void) {
	uint8_t buf[512];
	disk_read_sectors(EXT2_SUPER_LBA, 1, buf);

	uint8_t *p = buf + (EXT2_SUPER_OFFSET % 512);

	for (uint32_t i = 0; i < sizeof(struct ext2_superblock); i++) {
		p[i] = ((uint8_t *)&sb)[i];
	}

	disk_write_sectors(EXT2_SUPER_LBA, 1, buf);
}

// 读块组描述符
static int ext2_read_group_desc(uint32_t group, struct ext2_group_desc *out) {
	uint32_t gdt_block = (1024 / block_size) + 1;
	uint32_t gdt_offset = group * sizeof(struct ext2_group_desc);
	uint32_t gdt_lba = EXT2_PARTITION_LBA + gdt_block * (block_size / 512) + gdt_offset / 512;

	uint8_t tmp[512];
	disk_read_sectors(gdt_lba, 1, tmp);

	uint8_t *p = tmp + (gdt_offset % 512);

	for (uint32_t i = 0; i < sizeof(struct ext2_group_desc); i++) {
		((uint8_t *)out)[i] = p[i];
	}

	return 0;
}

// 写块组描述符
static int ext2_write_group_desc(uint32_t group, const struct ext2_group_desc *in) {
	uint32_t gdt_block = (1024 / block_size) + 1;
	uint32_t gdt_offset = group * sizeof(struct ext2_group_desc);
	uint32_t gdt_lba = EXT2_PARTITION_LBA + gdt_block * (block_size / 512) + gdt_offset / 512;

	uint8_t tmp[512];
	disk_read_sectors(gdt_lba, 1, tmp);

	uint8_t *p = tmp + (gdt_offset % 512);

	for (uint32_t i = 0; i < sizeof(struct ext2_group_desc); i++) {
		p[i] = ((const uint8_t *)in)[i];
	}

	disk_write_sectors(gdt_lba, 1, tmp);

	return 0;
}

// 读 inode
int ext2_read_inode(uint32_t ino, struct ext2_inode *out) {
	uint32_t index = ino - 1;
	uint32_t group = index / sb.inodes_per_group;
	uint32_t offset = index % sb.inodes_per_group;

	struct ext2_group_desc gd;
	ext2_read_group_desc(group, &gd);

	uint32_t inode_size = sb.inode_size;
	if (inode_size == 0) {
		inode_size = 128;
	}

	uint32_t byte_off = offset * inode_size;
	uint32_t lba = EXT2_PARTITION_LBA + gd.bg_inode_table * (block_size / 512) + byte_off / 512;

	uint8_t tmp[512];
	disk_read_sectors(lba, 1, tmp);

	uint8_t *p = tmp + (byte_off % 512);
	struct ext2_inode *ip = (struct ext2_inode *)p;

	for (uint32_t i = 0; i < sizeof(struct ext2_inode); i++) {
		((uint8_t *)out)[i] = ((uint8_t *)ip)[i];
	}

	return 0;
}

// 写 inode
int ext2_write_inode(uint32_t ino, const struct ext2_inode *in) {
	uint32_t index = ino - 1;
	uint32_t group = index / sb.inodes_per_group;
	uint32_t offset = index % sb.inodes_per_group;

	struct ext2_group_desc gd;
	ext2_read_group_desc(group, &gd);

	uint32_t inode_size = sb.inode_size;
	if (inode_size == 0) {
		inode_size = 128;
	}

	uint32_t byte_off = offset * inode_size;
	uint32_t lba = EXT2_PARTITION_LBA + gd.bg_inode_table * (block_size / 512) + byte_off / 512;

	uint8_t tmp[512];
	disk_read_sectors(lba, 1, tmp);

	uint8_t *p = tmp + (byte_off % 512);

	for (uint32_t i = 0; i < sizeof(struct ext2_inode); i++) {
		p[i] = ((const uint8_t *)in)[i];
	}

	disk_write_sectors(lba, 1, tmp);

	return 0;
}

// 分配 inode，成功返回 inode 号，失败返回 0
int ext2_alloc_inode(void) {
	uint32_t group_count = (sb.blocks_count + sb.blocks_per_group - 1) / sb.blocks_per_group;

	for (uint32_t g = 0; g < group_count; g++) {
		struct ext2_group_desc gd;
		ext2_read_group_desc(g, &gd);

		if (gd.bg_free_inodes_count == 0) {
			continue;
		}

		uint8_t buf[4096];
		ext2_read_block(gd.bg_inode_bitmap, buf);

		for (uint32_t i = 0; i < sb.inodes_per_group; i++) {
			uint32_t byte = i / 8;
			uint32_t bit = i % 8;

			if (!(buf[byte] & (1 << bit))) {
				buf[byte] |= (1 << bit);
				ext2_write_block(gd.bg_inode_bitmap, buf);

				gd.bg_free_inodes_count--;
				ext2_write_group_desc(g, &gd);

				sb.free_inodes_count--;
				ext2_write_super();

				return g * sb.inodes_per_group + i + 1;
			}
		}
	}

	return 0;
}

// 分配块，成功返回块号，失败返回 0
int ext2_alloc_block(void) {
	uint32_t group_count = (sb.blocks_count + sb.blocks_per_group - 1) / sb.blocks_per_group;

	for (uint32_t g = 0; g < group_count; g++) {
		struct ext2_group_desc gd;
		ext2_read_group_desc(g, &gd);

		if (gd.bg_free_blocks_count == 0) {
			continue;
		}

		uint8_t buf[4096];
		ext2_read_block(gd.bg_block_bitmap, buf);

		for (uint32_t i = 0; i < sb.blocks_per_group; i++) {
			uint32_t global = g * sb.blocks_per_group + i;

			if (global < sb.first_data_block) {
				continue;
			}

			if (global >= sb.blocks_count) {
				break;
			}

			uint32_t byte = i / 8;
			uint32_t bit = i % 8;

			if (!(buf[byte] & (1 << bit))) {
				buf[byte] |= (1 << bit);
				ext2_write_block(gd.bg_block_bitmap, buf);

				gd.bg_free_blocks_count--;
				ext2_write_group_desc(g, &gd);

				sb.free_blocks_count--;
				ext2_write_super();

				return global;
			}
		}
	}

	return 0;
}

// 释放一个块
static void ext2_free_block(uint32_t block) {
	if (block == 0) {
		return;
	}

	uint32_t group = (block - sb.first_data_block) / sb.blocks_per_group;
	uint32_t index = (block - sb.first_data_block) % sb.blocks_per_group;

	struct ext2_group_desc gd;
	ext2_read_group_desc(group, &gd);

	uint8_t buf[4096];
	ext2_read_block(gd.bg_block_bitmap, buf);

	uint32_t byte = index / 8;
	uint32_t bit = index % 8;

	if (buf[byte] & (1 << bit)) {
		buf[byte] &= ~(1 << bit);
		ext2_write_block(gd.bg_block_bitmap, buf);

		gd.bg_free_blocks_count++;
		ext2_write_group_desc(group, &gd);

		sb.free_blocks_count++;
		ext2_write_super();
	}
}

// 释放一个 inode
static void ext2_free_inode(uint32_t ino) {
	uint32_t index = ino - 1;
	uint32_t group = index / sb.inodes_per_group;
	uint32_t offset = index % sb.inodes_per_group;

	struct ext2_group_desc gd;
	ext2_read_group_desc(group, &gd);

	uint8_t buf[4096];
	ext2_read_block(gd.bg_inode_bitmap, buf);

	uint32_t byte = offset / 8;
	uint32_t bit = offset % 8;

	if (buf[byte] & (1 << bit)) {
		buf[byte] &= ~(1 << bit);
		ext2_write_block(gd.bg_inode_bitmap, buf);

		gd.bg_free_inodes_count++;
		ext2_write_group_desc(group, &gd);

		sb.free_inodes_count++;
		ext2_write_super();
	}
}

// 在目录里添加目录项，成功返回 0
int ext2_add_dir_entry(uint32_t dir_ino, uint32_t child_ino, const char *name, uint8_t type) {
	struct ext2_inode dir;
	ext2_read_inode(dir_ino, &dir);

	int name_len = 0;
	while (name[name_len] != '\0') {
		name_len++;
	}

	uint32_t needed = 8 + name_len;
	needed = (needed + 3) & ~3u;

	for (int b = 0; b < 12; b++) {
		if (dir.i_block[b] == 0) {
			break;
		}

		ext2_read_block(dir.i_block[b], block_buf);

		uint32_t offset = 0;

		while (offset < block_size) {
			struct ext2_dir_entry *de = (struct ext2_dir_entry *)(block_buf + offset);

			if (de->rec_len == 0) {
				break;
			}

			uint32_t actual = 8 + de->name_len;
			actual = (actual + 3) & ~3u;

			if (de->rec_len >= actual + needed) {
				uint16_t old_rec_len = de->rec_len;
				de->rec_len = actual;

				struct ext2_dir_entry *nd = (struct ext2_dir_entry *)(block_buf + offset + actual);
				nd->inode = child_ino;
				nd->rec_len = old_rec_len - actual;
				nd->name_len = name_len;
				nd->file_type = type;

				for (int i = 0; i < name_len; i++) {
					nd->name[i] = name[i];
				}

				ext2_write_block(dir.i_block[b], block_buf);
				return 0;
			}

			offset += de->rec_len;

			if (offset >= block_size) {
				break;
			}
		}
	}

	return -1;
}

// 在目录里删除名字为 name 的项，成功返回 0
static int ext2_remove_dir_entry(uint32_t dir_ino, const char *name) {
	int name_len = 0;
	while (name[name_len] != '\0') {
		name_len++;
	}

	struct ext2_inode dir;
	ext2_read_inode(dir_ino, &dir);

	for (int b = 0; b < 12; b++) {
		if (dir.i_block[b] == 0) {
			break;
		}

		ext2_read_block(dir.i_block[b], block_buf);

		uint32_t offset = 0;
		int prev = -1;

		while (offset < block_size) {
			struct ext2_dir_entry *de = (struct ext2_dir_entry *)(block_buf + offset);

			if (de->rec_len == 0) {
				break;
			}

			if (de->inode != 0 && de->name_len == name_len) {
				int match = 1;

				for (int i = 0; i < name_len; i++) {
					if (de->name[i] != name[i]) {
						match = 0;
						break;
					}
				}

				if (match) {
					if (prev < 0) {
						de->inode = 0;
					} else {
						struct ext2_dir_entry *pd = (struct ext2_dir_entry *)(block_buf + prev);
						pd->rec_len += de->rec_len;
					}

					ext2_write_block(dir.i_block[b], block_buf);
					return 0;
				}
			}

			prev = offset;
			offset += de->rec_len;

			if (offset >= block_size) {
				break;
			}
		}
	}

	return -1;
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

// 列目录
void ext2_ls(const char *path, int show_hidden, int show_inode, int show_long) {
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

	static char names[256][256];
	static uint32_t inodes[256];
	int count = 0;

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

			if (de->name_len > 0) {
				if (show_hidden || de->name[0] != '.') {
					if (count < 256) {
						int n = de->name_len;
						if (n > 255) {
							n = 255;
						}

						for (int i = 0; i < n; i++) {
							names[count][i] = de->name[i];
						}
						names[count][n] = '\0';
						inodes[count] = de->inode;
						count++;
					}
				}
			}

			offset += de->rec_len;

			if (offset + de->rec_len > block_size) {
				break;
			}
		}
	}

	for (int i = 0; i < count - 1; i++) {
		for (int j = 0; j < count - 1 - i; j++) {
			if (strcmp(names[j], names[j + 1]) > 0) {
				char tmp[256];

				for (int k = 0; k < 256; k++) {
					tmp[k] = names[j][k];
				}
				for (int k = 0; k < 256; k++) {
					names[j][k] = names[j + 1][k];
				}
				for (int k = 0; k < 256; k++) {
					names[j + 1][k] = tmp[k];
				}

				uint32_t ti = inodes[j];
				inodes[j] = inodes[j + 1];
				inodes[j + 1] = ti;
			}
		}
	}

	if (show_long) {
		for (int i = 0; i < count; i++) {
			if (show_inode) {
				print_uint(inodes[i]);
				print_char(' ');
			}

			struct ext2_inode child;
			ext2_read_inode(inodes[i], &child);
			print_char(ext2_type_char(child.i_mode));
			print_char(' ');
			print_uint(child.i_size);
			print_char(' ');
			print(names[i]);
			print_char('\n');
		}

		return;
	}

	int widths[256];
	for (int i = 0; i < count; i++) {
		int n = 0;
		while (names[i][n] != '\0') {
			n++;
		}

		if (show_inode) {
			uint32_t v = inodes[i];
			int d = 1;
			while (v >= 10) {
				v /= 10;
				d++;
			}
			n += d + 1;
		}

		widths[i] = n;
	}

	if (count == 0) {
		return;
	}

	int cols = count;
	if (cols > 64) {
		cols = 64;
	}

	while (cols > 1) {
		int rows = (count + cols - 1) / cols;
		int total = 0;

		for (int c = 0; c < cols; c++) {
			int mw = 0;

			for (int r = 0; r < rows; r++) {
				int idx = c * rows + r;

				if (idx >= count) {
					continue;
				}

				if (widths[idx] > mw) {
					mw = widths[idx];
				}
			}

			if (mw > 0) {
				total += mw + 2;
			}
		}

		if (total <= (int)total_cols) {
			break;
		}

		cols--;
	}

	int rows = (count + cols - 1) / cols;

	int col_widths[64];
	for (int c = 0; c < cols && c < 64; c++) {
		int mw = 0;

		for (int r = 0; r < rows; r++) {
			int idx = c * rows + r;

			if (idx >= count) {
				continue;
			}

			if (widths[idx] > mw) {
				mw = widths[idx];
			}
		}

		col_widths[c] = (mw > 0) ? (mw + 2) : 0;
	}

	for (int r = 0; r < rows; r++) {
		for (int c = 0; c < cols && c < 64; c++) {
			int idx = c * rows + r;

			if (idx >= count) {
				continue;
			}

			int printed = 0;

			if (show_inode) {
				print_uint(inodes[idx]);
				print_char(' ');

				uint32_t v = inodes[idx];
				int d = 1;
				while (v >= 10) {
					v /= 10;
					d++;
				}
				printed += d + 1;
			}

			print(names[idx]);

			int n = 0;
			while (names[idx][n] != '\0') {
				n++;
			}
			printed += n;

			int has_next = 0;

			for (int c2 = c + 1; c2 < cols; c2++) {
				if (c2 * rows + r < count) {
					has_next = 1;
					break;
				}
			}

			if (has_next) {
				for (int k = printed; k < col_widths[c]; k++) {
					print_char(' ');
				}
			}
		}

		print_char('\n');
	}
}

// tree 递归帧池
static uint8_t tree_frame[8][4096];

static void ext2_tree_recursive(uint32_t ino, const char *prefix, int depth, int show_hidden) {
	if (depth >= 8) {
		return;
	}

	struct ext2_inode inode;
	ext2_read_inode(ino, &inode);

	if ((inode.i_mode & 0xF000) != 0x4000) {
		return;
	}

	uint8_t *buf = tree_frame[depth];

	for (int b = 0; b < 12; b++) {
		if (inode.i_block[b] == 0) {
			break;
		}

		uint32_t lba = EXT2_PARTITION_LBA + inode.i_block[b] * (block_size / 512);
		disk_read_sectors(lba, block_size / 512, buf);

		uint32_t offset = 0;

		while (offset < block_size) {
			struct ext2_dir_entry *de = (struct ext2_dir_entry *)(buf + offset);

			if (de->inode == 0 || de->rec_len == 0) {
				break;
			}

			if (de->name_len > 0 && (show_hidden || de->name[0] != '.')) {
				int more = 0;
				uint32_t scan = offset + de->rec_len;

				while (scan < block_size) {
					struct ext2_dir_entry *nxt = (struct ext2_dir_entry *)(buf + scan);

					if (nxt->inode == 0 || nxt->rec_len == 0) {
						break;
					}

					if (nxt->name_len > 0 && (show_hidden || nxt->name[0] != '.')) {
						more = 1;
						break;
					}

					scan += nxt->rec_len;
				}

				if (b + 1 < 12 && inode.i_block[b + 1] != 0) {
					more = 1;
				}

				int is_last = !more;

				print(prefix);
				print(is_last ? "`-- " : "|-- ");

				for (int i = 0; i < de->name_len; i++) {
					print_char(de->name[i]);
				}

				print_char('\n');

				struct ext2_inode child;
				ext2_read_inode(de->inode, &child);

				if ((child.i_mode & 0xF000) == 0x4000) {
					char new_prefix[256];
					int w = 0;

					for (int j = 0; prefix[j] != '\0' && w < 250; j++) {
						new_prefix[w++] = prefix[j];
					}

					const char *add = is_last ? "    " : "|   ";

					for (int j = 0; add[j] != '\0' && w < 255; j++) {
						new_prefix[w++] = add[j];
					}

					new_prefix[w] = '\0';

					ext2_tree_recursive(de->inode, new_prefix, depth + 1, show_hidden);
				}
			}

			offset += de->rec_len;

			if (offset + de->rec_len > block_size) {
				break;
			}
		}
	}
}

// 树形列出目录
void ext2_tree(const char *path, int show_hidden) {
	if (!ext2_ok) {
		return;
	}

	uint32_t ino;

	if (ext2_lookup(path, &ino) != 0) {
		return;
	}

	ext2_tree_recursive(ino, "", 0, show_hidden);
}

// 读文件内容并打印，成功返回 0，失败返回负值
int ext2_cat(const char *path) {
	if (!ext2_ok) {
		return -1;
	}

	uint32_t ino;

	if (ext2_lookup(path, &ino) != 0) {
		return -1;
	}

	struct ext2_inode inode;
	ext2_read_inode(ino, &inode);

	if ((inode.i_mode & 0xF000) == 0x4000) {
		return -2;
	}

	uint32_t size = inode.i_size;
	uint32_t printed = 0;

	for (int b = 0; b < 12; b++) {
		if (inode.i_block[b] == 0) {
			break;
		}

		if (printed >= size) {
			break;
		}

		ext2_read_block(inode.i_block[b], block_buf);

		for (uint32_t i = 0; i < block_size && printed < size; i++) {
			print_char(block_buf[i]);
			printed++;
		}
	}

	return 0;
}

// 列出目录里的名字，跳过 . 与 ..，最多 max 个，返回实际数量
int ext2_list_names(uint32_t ino, char names[][256], int max) {
	if (!ext2_ok) {
		return 0;
	}

	struct ext2_inode inode;
	ext2_read_inode(ino, &inode);

	if ((inode.i_mode & 0xF000) != 0x4000) {
		return 0;
	}

	int count = 0;

	for (int b = 0; b < 12; b++) {
		if (inode.i_block[b] == 0) {
			break;
		}

		ext2_read_block(inode.i_block[b], block_buf);

		uint32_t offset = 0;

		while (offset < block_size) {
			struct ext2_dir_entry *de = (struct ext2_dir_entry *)(block_buf + offset);

			if (de->inode == 0 || de->rec_len == 0) {
				break;
			}

			if (de->name_len > 0) {
				int skip = 0;
				if (de->name_len == 1 && de->name[0] == '.') {
					skip = 1;
				}
				if (de->name_len == 2 && de->name[0] == '.' && de->name[1] == '.') {
					skip = 1;
				}

				if (!skip && count < max) {
					int n = de->name_len;
					if (n > 255) {
						n = 255;
					}

					for (int i = 0; i < n; i++) {
						names[count][i] = de->name[i];
					}

					names[count][n] = '\0';
					count++;
				}
			}

			offset += de->rec_len;

			if (offset >= block_size) {
				break;
			}
		}
	}

	return count;
}

// 创建目录，成功返回 0，失败返回负值
// -1 父目录不存在，-2 目标已存在，-3 父不是目录，-4 无空间
int ext2_mkdir(const char *path) {
	int len = 0;
	while (path[len] != '\0') {
		len++;
	}

	if (len < 2 || path[0] != '/') {
		return -1;
	}

	int slash = -1;
	for (int i = len - 1; i >= 1; i--) {
		if (path[i] == '/') {
			slash = i;
			break;
		}
	}

	char parent[256];
	char name[256];

	if (slash == -1) {
		parent[0] = '/';
		parent[1] = '\0';

		int k = 0;
		for (int i = 1; i < len; i++) {
			name[k++] = path[i];
		}
		name[k] = '\0';
	} else {
		for (int i = 0; i < slash; i++) {
			parent[i] = path[i];
		}
		parent[slash] = '\0';

		int k = 0;
		for (int i = slash + 1; i < len; i++) {
			name[k++] = path[i];
		}
		name[k] = '\0';
	}

	if (name[0] == '\0') {
		return -1;
	}

	uint32_t parent_ino;

	if (ext2_lookup(parent, &parent_ino) != 0) {
		return -1;
	}

	struct ext2_inode pino;
	ext2_read_inode(parent_ino, &pino);

	if ((pino.i_mode & 0xF000) != 0x4000) {
		return -3;
	}

	uint32_t existing;
	if (ext2_lookup(path, &existing) == 0) {
		return -2;
	}

	uint32_t new_ino = ext2_alloc_inode();

	if (new_ino == 0) {
		return -4;
	}

	uint32_t new_block = ext2_alloc_block();

	if (new_block == 0) {
		return -4;
	}

	for (uint32_t i = 0; i < block_size; i++) {
		block_buf[i] = 0;
	}

	struct ext2_dir_entry *dot = (struct ext2_dir_entry *)block_buf;
	dot->inode = new_ino;
	dot->rec_len = 12;
	dot->name_len = 1;
	dot->file_type = 2;
	dot->name[0] = '.';

	struct ext2_dir_entry *dotdot = (struct ext2_dir_entry *)(block_buf + 12);
	dotdot->inode = parent_ino;
	dotdot->rec_len = block_size - 12;
	dotdot->name_len = 2;
	dotdot->file_type = 2;
	dotdot->name[0] = '.';
	dotdot->name[1] = '.';

	ext2_write_block(new_block, block_buf);

	struct ext2_inode new_inode;
	for (uint32_t i = 0; i < sizeof(struct ext2_inode); i++) {
		((uint8_t *)&new_inode)[i] = 0;
	}

	new_inode.i_mode = 0x41ED;
	new_inode.i_size = block_size;
	new_inode.i_links_count = 2;
	new_inode.i_blocks = block_size / 512;
	new_inode.i_block[0] = new_block;

	ext2_write_inode(new_ino, &new_inode);

	if (ext2_add_dir_entry(parent_ino, new_ino, name, 2) != 0) {
		return -4;
	}

	pino.i_links_count++;
	ext2_write_inode(parent_ino, &pino);

	return 0;
}

// 递归删除 inode 指向的文件或目录内容
static void ext2_remove_recursive(uint32_t ino, int depth) {
	if (depth > 8) {
		return;
	}

	struct ext2_inode inode;
	ext2_read_inode(ino, &inode);

	if ((inode.i_mode & 0xF000) == 0x4000) {
		for (int b = 0; b < 12; b++) {
			if (inode.i_block[b] == 0) {
				break;
			}

			ext2_read_block(inode.i_block[b], block_buf);

			uint32_t offset = 0;

			while (offset < block_size) {
				struct ext2_dir_entry *de = (struct ext2_dir_entry *)(block_buf + offset);

				if (de->inode == 0 || de->rec_len == 0) {
					break;
				}

				int skip = 0;
				if (de->name_len == 1 && de->name[0] == '.') {
					skip = 1;
				}
				if (de->name_len == 2 && de->name[0] == '.' && de->name[1] == '.') {
					skip = 1;
				}

				if (!skip) {
					ext2_remove_recursive(de->inode, depth + 1);
				}

				offset += de->rec_len;

				if (offset >= block_size) {
					break;
				}
			}
		}
	}

	for (int b = 0; b < 12; b++) {
		if (inode.i_block[b] == 0) {
			break;
		}

		ext2_free_block(inode.i_block[b]);
	}

	ext2_free_inode(ino);
}

// 删除文件或目录，成功返回 0，失败返回负值
int ext2_remove(const char *path, int recursive) {
	int len = 0;
	while (path[len] != '\0') {
		len++;
	}

	if (len < 2 || path[0] != '/') {
		return -2;
	}

	int slash = -1;
	for (int i = len - 1; i >= 1; i--) {
		if (path[i] == '/') {
			slash = i;
			break;
		}
	}

	char parent[256];
	char name[256];

	if (slash == -1) {
		parent[0] = '/';
		parent[1] = '\0';

		int k = 0;
		for (int i = 1; i < len; i++) {
			name[k++] = path[i];
		}
		name[k] = '\0';
	} else {
		for (int i = 0; i < slash; i++) {
			parent[i] = path[i];
		}
		parent[slash] = '\0';

		int k = 0;
		for (int i = slash + 1; i < len; i++) {
			name[k++] = path[i];
		}
		name[k] = '\0';
	}

	if (name[0] == '\0') {
		return -2;
	}

	uint32_t parent_ino;

	if (ext2_lookup(parent, &parent_ino) != 0) {
		return -1;
	}

	struct ext2_inode pino;
	ext2_read_inode(parent_ino, &pino);

	if ((pino.i_mode & 0xF000) != 0x4000) {
		return -3;
	}

	uint32_t target_ino;

	if (ext2_lookup(path, &target_ino) != 0) {
		return -2;
	}

	struct ext2_inode tino;
	ext2_read_inode(target_ino, &tino);

	if ((tino.i_mode & 0xF000) == 0x4000) {
		if (!recursive) {
			return -4;
		}

		ext2_remove_recursive(target_ino, 0);

		if (ext2_remove_dir_entry(parent_ino, name) != 0) {
			return -2;
		}

		pino.i_links_count--;
		ext2_write_inode(parent_ino, &pino);

		return 0;
	}

	ext2_remove_recursive(target_ino, 0);

	if (ext2_remove_dir_entry(parent_ino, name) != 0) {
		return -2;
	}

	return 0;
}

// 删除空目录，成功返回 0，失败返回负值
int ext2_rmdir(const char *path) {
	int len = 0;
	while (path[len] != '\0') {
		len++;
	}

	if (len < 2 || path[0] != '/') {
		return -2;
	}

	int slash = -1;
	for (int i = len - 1; i >= 1; i--) {
		if (path[i] == '/') {
			slash = i;
			break;
		}
	}

	char parent[256];
	char name[256];

	if (slash == -1) {
		parent[0] = '/';
		parent[1] = '\0';

		int k = 0;
		for (int i = 1; i < len; i++) {
			name[k++] = path[i];
		}
		name[k] = '\0';
	} else {
		for (int i = 0; i < slash; i++) {
			parent[i] = path[i];
		}
		parent[slash] = '\0';

		int k = 0;
		for (int i = slash + 1; i < len; i++) {
			name[k++] = path[i];
		}
		name[k] = '\0';
	}

	if (name[0] == '\0') {
		return -2;
	}

	uint32_t parent_ino;

	if (ext2_lookup(parent, &parent_ino) != 0) {
		return -1;
	}

	struct ext2_inode pino;
	ext2_read_inode(parent_ino, &pino);

	if ((pino.i_mode & 0xF000) != 0x4000) {
		return -3;
	}

	uint32_t target_ino;

	if (ext2_lookup(path, &target_ino) != 0) {
		return -2;
	}

	struct ext2_inode tino;
	ext2_read_inode(target_ino, &tino);

	if ((tino.i_mode & 0xF000) != 0x4000) {
		return -4;
	}

	for (int b = 0; b < 12; b++) {
		if (tino.i_block[b] == 0) {
			break;
		}

		ext2_read_block(tino.i_block[b], block_buf);

		uint32_t offset = 0;

		while (offset < block_size) {
			struct ext2_dir_entry *de = (struct ext2_dir_entry *)(block_buf + offset);

			if (de->inode == 0 || de->rec_len == 0) {
				break;
			}

			if (de->name_len > 0) {
				int skip = 0;
				if (de->name_len == 1 && de->name[0] == '.') {
					skip = 1;
				}
				if (de->name_len == 2 && de->name[0] == '.' && de->name[1] == '.') {
					skip = 1;
				}

				if (!skip) {
					return -5;
				}
			}

			offset += de->rec_len;

			if (offset >= block_size) {
				break;
			}
		}
	}

	for (int b = 0; b < 12; b++) {
		if (tino.i_block[b] == 0) {
			break;
		}

		ext2_free_block(tino.i_block[b]);
	}

	ext2_free_inode(target_ino);

	if (ext2_remove_dir_entry(parent_ino, name) != 0) {
		return -2;
	}

	pino.i_links_count--;
	ext2_write_inode(parent_ino, &pino);

	return 0;
}

// 释放文件占用的所有直接块，并把 i_block 清零
static void ext2_free_file_blocks(struct ext2_inode *inode) {
	for (int b = 0; b < 12; b++) {
		if (inode->i_block[b] != 0) {
			ext2_free_block(inode->i_block[b]);
			inode->i_block[b] = 0;
		}
	}
}

// 读文件全部内容到 buffer，返回实际字节数
int ext2_read_file(const char *path, char *buffer, uint32_t max) {
	uint32_t ino;

	if (ext2_lookup(path, &ino) != 0) {
		return -1;
	}

	struct ext2_inode inode;
	ext2_read_inode(ino, &inode);

	if ((inode.i_mode & 0xF000) == 0x4000) {
		return -2;
	}

	uint32_t size = inode.i_size;
	if (size > max) {
		size = max;
	}

	uint32_t read = 0;

	for (int b = 0; b < 12; b++) {
		if (inode.i_block[b] == 0) {
			break;
		}

		if (read >= size) {
			break;
		}

		ext2_read_block(inode.i_block[b], block_buf);

		for (uint32_t i = 0; i < block_size && read < size; i++) {
			buffer[read++] = block_buf[i];
		}
	}

	return (int)read;
}

// 写文件，append 为 1 时追加，为 0 时覆盖
// 返回 0 成功，负值失败
int ext2_write_file(const char *path, const char *data, uint32_t len, int append) {
	int plen = 0;
	while (path[plen] != '\0') {
		plen++;
	}

	if (plen < 2 || path[0] != '/') {
		return -1;
	}

	int slash = -1;
	for (int i = plen - 1; i >= 1; i--) {
		if (path[i] == '/') {
			slash = i;
			break;
		}
	}

	char parent[256];
	char name[256];

	if (slash == -1) {
		parent[0] = '/';
		parent[1] = '\0';

		int k = 0;
		for (int i = 1; i < plen; i++) {
			name[k++] = path[i];
		}
		name[k] = '\0';
	} else {
		for (int i = 0; i < slash; i++) {
			parent[i] = path[i];
		}
		parent[slash] = '\0';

		int k = 0;
		for (int i = slash + 1; i < plen; i++) {
			name[k++] = path[i];
		}
		name[k] = '\0';
	}

	if (name[0] == '\0') {
		return -1;
	}

	uint32_t parent_ino;

	if (ext2_lookup(parent, &parent_ino) != 0) {
		return -2;
	}

	struct ext2_inode pino;
	ext2_read_inode(parent_ino, &pino);

	if ((pino.i_mode & 0xF000) != 0x4000) {
		return -3;
	}

	uint32_t ino;
	int is_new = 0;

	if (ext2_lookup(path, &ino) != 0) {
		ino = ext2_alloc_inode();

		if (ino == 0) {
			return -5;
		}

		is_new = 1;
	} else {
		struct ext2_inode tmp;
		ext2_read_inode(ino, &tmp);

		if ((tmp.i_mode & 0xF000) == 0x4000) {
			return -4;
		}
	}

	static char existing[49152];
	uint32_t existing_len = 0;

	if (!is_new && append) {
		int r = ext2_read_file(path, existing, 49152);

		if (r < 0) {
			return r;
		}

		existing_len = (uint32_t)r;
	}

	if (!is_new && !append) {
		struct ext2_inode old;
		ext2_read_inode(ino, &old);
		ext2_free_file_blocks(&old);
		ext2_write_inode(ino, &old);
	}

	uint32_t total_len = len;

	if (!is_new && append) {
		total_len = existing_len + len;
	}

	if (total_len > 49152) {
		return -5;
	}

	uint32_t blocks_needed = (total_len + block_size - 1) / block_size;
	if (blocks_needed > 12) {
		return -5;
	}

	struct ext2_inode inode;

	if (!is_new) {
		ext2_read_inode(ino, &inode);
	} else {
		for (uint32_t i = 0; i < sizeof(struct ext2_inode); i++) {
			((uint8_t *)&inode)[i] = 0;
		}

		inode.i_mode = 0x81A4;
		inode.i_links_count = 1;
	}

	for (uint32_t b = 0; b < blocks_needed; b++) {
		if (inode.i_block[b] == 0) {
			uint32_t nb = ext2_alloc_block();

			if (nb == 0) {
				return -5;
			}

			inode.i_block[b] = nb;
		}
	}

	static char final_buf[49152];
	uint32_t final_len = 0;

	if (!is_new && append) {
		for (uint32_t i = 0; i < existing_len; i++) {
			final_buf[final_len++] = existing[i];
		}
	}

	for (uint32_t i = 0; i < len; i++) {
		final_buf[final_len++] = data[i];
	}

	for (uint32_t b = 0; b < blocks_needed; b++) {
		for (uint32_t i = 0; i < block_size; i++) {
			block_buf[i] = 0;
		}

		uint32_t start = b * block_size;

		for (uint32_t i = 0; i < block_size && start + i < final_len; i++) {
			block_buf[i] = final_buf[start + i];
		}

		ext2_write_block(inode.i_block[b], block_buf);
	}

	inode.i_size = final_len;
	inode.i_blocks = blocks_needed * (block_size / 512);

	ext2_write_inode(ino, &inode);

	if (is_new) {
		if (ext2_add_dir_entry(parent_ino, ino, name, 1) != 0) {
			return -5;
		}
	}

	return 0;
}