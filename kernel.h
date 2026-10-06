#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>

extern uint64_t fb_addr;
extern uint32_t fb_width;
extern uint32_t fb_height;
extern uint32_t fb_bpp;
extern uint32_t fb_pitch;

extern uint32_t total_cols;
extern uint32_t total_rows;

extern int cursor_col;
extern int cursor_row;

extern volatile int cursor_visible;
extern uint64_t cursor_blink_next;

#define CURSOR_BLINK_MS	200

extern volatile uint64_t ticks;

void change_color(unsigned char r, unsigned char g, unsigned char b);
void change_fg(unsigned char r, unsigned char g, unsigned char b);
void change_bg(unsigned char r, unsigned char g, unsigned char b);

void draw_pixel(int x, int y);
uint32_t read_pixel(int x, int y);
void write_pixel(int x, int y, uint32_t c);
void draw_char(int cx, int cy, char c);

void cursor_lock(void);
void cursor_unlock(void);
void cursor_hide(void);
void cursor_show(void);
void cursor_goto(int cx, int cy);
void cursor_move(int dx, int dy);
void cursor_enable(void);
void cursor_disable(void);

void scroll_screen(int lines);
void clear_screen(void);

void print_char(char c);
void print(const char *s);
void print_uint(uint32_t n);
void print_timestamp(void);

void delay_ms(uint32_t ms);

char get_key(void);

void disk_init(void);
int disk_read_sectors(uint32_t lba, uint8_t count, void *buf);
int disk_write_sectors(uint32_t lba, uint8_t count, const void *buf);

void ext2_init(void);
void ext2_print_info(void);
void ext2_ls_root(void);

int redir_is_active(void);
void redir_begin(void);
const char *redir_end(int *len);

int strcmp(const char *a, const char *b);

#endif