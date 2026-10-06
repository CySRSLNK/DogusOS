#ifndef EXT2_H
#define EXT2_H

#include <stdint.h>

struct ext2_inode {
	uint16_t i_mode;
	uint16_t i_uid;
	uint32_t i_size;
	uint32_t i_atime;
	uint32_t i_ctime;
	uint32_t i_mtime;
	uint32_t i_dtime;
	uint16_t i_gid;
	uint16_t i_links_count;
	uint32_t i_blocks;
	uint32_t i_flags;
	uint32_t i_osd1;
	uint32_t i_block[15];
	uint32_t i_generation;
	uint32_t i_file_acl;
	uint32_t i_dir_acl;
	uint32_t i_faddr;
	uint8_t i_osd2[12];
};

void ext2_init(void);
void ext2_print_info(void);
void ext2_ls_root(void);
int ext2_lookup(const char *path, uint32_t *ino);
void ext2_ls(const char *path, int show_hidden, int show_inode, int show_long);
void ext2_tree(const char *path, int show_hidden);
int ext2_read_inode(uint32_t ino, struct ext2_inode *out);
int ext2_write_inode(uint32_t ino, const struct ext2_inode *in);
int ext2_alloc_inode(void);
int ext2_alloc_block(void);
int ext2_add_dir_entry(uint32_t dir_ino, uint32_t child_ino, const char *name, uint8_t type);
int ext2_mkdir(const char *path);
int ext2_remove(const char *path, int recursive);
int ext2_rmdir(const char *path);
int ext2_cat(const char *path);
int ext2_list_names(uint32_t ino, char names[][256], int max);
int ext2_read_file(const char *path, char *buffer, uint32_t max);
int ext2_write_file(const char *path, const char *data, uint32_t len, int append);

#endif