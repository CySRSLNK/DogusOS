// DogusOS 内核主文件，初始化硬件与底层绘制，shell 在主循环中运行
#include <stdint.h>
#include "kernel.h"
#include "shell.h"
#include "commands.h"
#include "font.h"

// 字符到 font8x16 下标的映射表，顺序为小写、大写、数字、标点
static const char char_map[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ,./;'[]\\`-=<>?:\"{}|~!@#$%^&*()_+";

// 查找字符在字模数组中的下标，未找到返回 -1
int font_index(char c) {
	for (int i = 0; char_map[i] != '\0'; i++) {
		if (char_map[i] == c) {
			return i;
		}
	}

	return -1;
}

// Multiboot 2 标签类型
#define MB2_TAG_TYPE_END	0
#define MB2_TAG_TYPE_FRAMEBUFFER	8

// LOCK
// 深度计数版，同一作用域可重复调用，跨函数嵌套也安全
// 只有最外层真正 cli 与 popfq，内层只累加计数
// 临界区内禁止调用 delay_ms 或 hlt，IF 清零后等不到中断会死锁
static uint64_t cursor_lock_flags;
volatile int cursor_lock_depth = 0;

#define CURSOR_LOCK()								\
	do {									\
		if (cursor_lock_depth++ == 0) {					\
			__asm__ volatile ("pushfq\n\tpop %0\n\tcli"		\
					  : "=r"(cursor_lock_flags));		\
		}								\
	} while (0)

#define CURSOR_UNLOCK()								\
	do {									\
		if (--cursor_lock_depth == 0) {					\
			__asm__ volatile ("push %0\n\tpopfq"			\
					  : : "r"(cursor_lock_flags));		\
		}								\
	} while (0)

// 帧缓冲信息全局变量
uint64_t fb_addr;
uint32_t fb_width;
uint32_t fb_height;
uint32_t fb_bpp;
uint32_t fb_pitch;

// 颜色分量全局变量，默认白色
unsigned char color_r = 255;
unsigned char color_g = 255;
unsigned char color_b = 255;

// 前景色与背景色，默认前景白、背景黑
unsigned char fg_r = 255;
unsigned char fg_g = 255;
unsigned char fg_b = 255;
unsigned char bg_r = 0;
unsigned char bg_g = 0;
unsigned char bg_b = 0;

// 字符格总数，由帧缓冲尺寸向下取整得到
uint32_t total_cols = 0;
uint32_t total_rows = 0;

int cursor_col = 0;
int cursor_row = 0;

// 光标开关
int cursor_on = 1;

// 光标可见状态，闪烁相位，timer_handler 翻转
volatile int cursor_visible = 1;

// 下次翻转闪烁的毫秒时刻
uint64_t cursor_blink_next = CURSOR_BLINK_MS;

// 光标条账本
static int cursor_painted = 0;
static int cursor_painted_col = 0;
static int cursor_painted_row = 0;

// 光标条原始像素保存，底部两行共 16 像素
static uint32_t cursor_save[16];

// 毫秒计数，中断里加一
volatile uint64_t ticks = 0;

// 按键队列
#define KEY_BUF_SIZE	256

volatile char key_buf[KEY_BUF_SIZE];
volatile int key_head = 0;
volatile int key_tail = 0;

// 扫描码到字符的映射表，美式 QWERTY，不支持 Shift
static const char scancode_map[256] = {
	0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0, 0,
	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 0, 0,
	'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
	'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, 0, 0, ' ',
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// 扫描码到字符的映射表，美式 QWERTY，Shift 上档
static const char scancode_map_shift[256] = {
	0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 0, 0,
	'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 0, 0,
	'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|',
	'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, 0, 0, ' ',
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// 键盘修饰键状态
static int shift_pressed = 0;
static int caps_lock_on = 0;

// Multiboot 2 标签通用头部
struct mb2_tag {
	uint32_t type;
	uint32_t size;
};

// Multiboot 2 帧缓冲标签
struct mb2_tag_framebuffer {
	uint32_t type;
	uint32_t size;
	uint64_t framebuffer_addr;
	uint32_t framebuffer_pitch;
	uint32_t framebuffer_width;
	uint32_t framebuffer_height;
	uint8_t framebuffer_bpp;
	uint8_t framebuffer_type;
	uint16_t reserved;
};

// IDT 项结构
struct idt_entry {
	uint16_t offset_low;
	uint16_t selector;
	uint8_t ist;
	uint8_t type_attr;
	uint16_t offset_mid;
	uint32_t offset_high;
	uint32_t zero;
} __attribute__((packed));

// IDT 指针
struct idt_ptr {
	uint16_t limit;
	uint64_t base;
} __attribute__((packed));

struct idt_entry idt[256];
struct idt_ptr idtp;

#define PIC1_COMMAND	0x20
#define PIC1_DATA	0x21
#define PIC2_COMMAND	0xA0
#define PIC2_DATA	0xA1

void change_color(unsigned char r, unsigned char g, unsigned char b) {
	color_r = r;
	color_g = g;
	color_b = b;
}

void change_fg(unsigned char r, unsigned char g, unsigned char b) {
	fg_r = r;
	fg_g = g;
	fg_b = b;
}

void change_bg(unsigned char r, unsigned char g, unsigned char b) {
	bg_r = r;
	bg_g = g;
	bg_b = b;
}

// 在指定坐标画一个像素，颜色取自全局颜色变量
void draw_pixel(int x, int y) {
	if (x < 0 || y < 0 || (uint32_t)x >= fb_width || (uint32_t)y >= fb_height) {
		return;
	}

	uint8_t *pixel = (uint8_t *)fb_addr + y * fb_pitch + x * (fb_bpp / 8);

	if (fb_bpp == 32) {
		pixel[0] = color_b;
		pixel[1] = color_g;
		pixel[2] = color_r;
		pixel[3] = 0;
	} else if (fb_bpp == 24) {
		pixel[0] = color_b;
		pixel[1] = color_g;
		pixel[2] = color_r;
	} else if (fb_bpp == 16) {
		uint16_t value = ((color_r >> 3) << 11) | ((color_g >> 2) << 5) | (color_b >> 3);
		*(uint16_t *)pixel = value;
	}
}

// 读指定坐标像素颜色
uint32_t read_pixel(int x, int y) {
	if (x < 0 || y < 0 || (uint32_t)x >= fb_width || (uint32_t)y >= fb_height) {
		return 0;
	}

	uint8_t *pixel = (uint8_t *)fb_addr + y * fb_pitch + x * (fb_bpp / 8);

	if (fb_bpp == 32) {
		return ((uint32_t)pixel[2] << 16) | ((uint32_t)pixel[1] << 8) | pixel[0];
	} else if (fb_bpp == 24) {
		return ((uint32_t)pixel[2] << 16) | ((uint32_t)pixel[1] << 8) | pixel[0];
	} else if (fb_bpp == 16) {
		return *(uint16_t *)pixel;
	}

	return 0;
}

// 写指定坐标像素颜色
void write_pixel(int x, int y, uint32_t c) {
	if (x < 0 || y < 0 || (uint32_t)x >= fb_width || (uint32_t)y >= fb_height) {
		return;
	}

	uint8_t *pixel = (uint8_t *)fb_addr + y * fb_pitch + x * (fb_bpp / 8);

	if (fb_bpp == 32) {
		pixel[0] = c & 0xFF;
		pixel[1] = (c >> 8) & 0xFF;
		pixel[2] = (c >> 16) & 0xFF;
		pixel[3] = 0;
	} else if (fb_bpp == 24) {
		pixel[0] = c & 0xFF;
		pixel[1] = (c >> 8) & 0xFF;
		pixel[2] = (c >> 16) & 0xFF;
	} else if (fb_bpp == 16) {
		*(uint16_t *)pixel = (uint16_t)c;
	}
}

// 在指定字符格画一个字符
void draw_char(int cx, int cy, char c) {
	if (cx < 0 || cy < 0 || (uint32_t)cx >= total_cols || (uint32_t)cy >= total_rows) {
		return;
	}

	int index = font_index(c);

	if (index < 0) {
		return;
	}

	int px = cx * 8;
	int py = cy * 16;

	unsigned char saved_r = color_r;
	unsigned char saved_g = color_g;
	unsigned char saved_b = color_b;

	for (int row = 0; row < 16; row++) {
		unsigned char bits = font8x16[index][row];

		change_color(fg_r, fg_g, fg_b);
		for (int col = 0; col < 8; col++) {
			if ((bits >> (7 - col)) & 1) {
				draw_pixel(px + col, py + row);
			}
		}

		change_color(bg_r, bg_g, bg_b);
		for (int col = 0; col < 8; col++) {
			if (!((bits >> (7 - col)) & 1)) {
				draw_pixel(px + col, py + row);
			}
		}
	}

	change_color(saved_r, saved_g, saved_b);
}

// 在指定字符格底部两行画光标条，逐像素取反
// 取反两次严格还原，不依赖该格颜色数量
void cursor_paint(int cx, int cy) {
	if (cx < 0 || cy < 0 || (uint32_t)cx >= total_cols || (uint32_t)cy >= total_rows) {
		return;
	}

	int px = cx * 8;
	int py = cy * 16;

	for (int row = 14; row < 16; row++) {
		for (int col = 0; col < 8; col++) {
			uint32_t c = read_pixel(px + col, py + row);
			write_pixel(px + col, py + row, (~c) & 0xFFFFFF);
		}
	}
}

// 加锁与解锁，供 shell 模块在整行重画期间防止中断插入
void cursor_lock(void) {
	CURSOR_LOCK();
}

void cursor_unlock(void) {
	CURSOR_UNLOCK();
}

// 擦掉屏上已有的光标条，把保存的原始像素写回
void cursor_hide(void) {
	if (!cursor_painted) {
		return;
	}

	int px = cursor_painted_col * 8;
	int py = cursor_painted_row * 16;

	int idx = 0;
	for (int row = 14; row < 16; row++) {
		for (int col = 0; col < 8; col++) {
			write_pixel(px + col, py + row, cursor_save[idx]);
			idx++;
		}
	}

	cursor_painted = 0;
}

// 按当前状态在新位置画光标条，画前保存原始像素
void cursor_show(void) {
	if (cursor_painted) {
		return;
	}

	if (!cursor_on || !cursor_visible) {
		return;
	}

	if (cursor_col < 0 || cursor_row < 0 || (uint32_t)cursor_col >= total_cols || (uint32_t)cursor_row >= total_rows) {
		return;
	}

	int px = cursor_col * 8;
	int py = cursor_row * 16;

	int idx = 0;
	for (int row = 14; row < 16; row++) {
		for (int col = 0; col < 8; col++) {
			cursor_save[idx] = read_pixel(px + col, py + row);
			idx++;
		}
	}

	idx = 0;
	for (int row = 14; row < 16; row++) {
		for (int col = 0; col < 8; col++) {
			write_pixel(px + col, py + row, (~cursor_save[idx]) & 0xFFFFFF);
			idx++;
		}
	}

	cursor_painted_col = cursor_col;
	cursor_painted_row = cursor_row;
	cursor_painted = 1;
}

// 画面被整块抹掉或搬走后调用
void cursor_forget(void) {
	cursor_painted = 0;
}

// 设置光标字符格坐标，带边界 clamp
static void cursor_setpos(int cx, int cy) {
	if (total_cols == 0 || total_rows == 0) {
		return;
	}

	if (cx < 0) {
		cx = 0;
	}

	if (cy < 0) {
		cy = 0;
	}

	if ((uint32_t)cx > total_cols - 1) {
		cx = (int)total_cols - 1;
	}

	if ((uint32_t)cy > total_rows - 1) {
		cy = (int)total_rows - 1;
	}

	cursor_col = cx;
	cursor_row = cy;
}

// 移动虚拟光标到指定字符格
void cursor_goto(int cx, int cy) {
	CURSOR_LOCK();
	cursor_hide();
	cursor_setpos(cx, cy);
	cursor_show();
	CURSOR_UNLOCK();
}

// 移动虚拟光标到相对字符格
void cursor_move(int dx, int dy) {
	CURSOR_LOCK();
	cursor_hide();
	cursor_setpos(cursor_col + dx, cursor_row + dy);
	cursor_show();
	CURSOR_UNLOCK();
}

void cursor_enable(void) {
	CURSOR_LOCK();
	cursor_on = 1;
	cursor_show();
	CURSOR_UNLOCK();
}

void cursor_disable(void) {
	CURSOR_LOCK();
	cursor_hide();
	cursor_on = 0;
	CURSOR_UNLOCK();
}

// 滚屏，lines 正数上滚
void scroll_screen(int lines) {
	if (lines == 0) {
		return;
	}

	cursor_hide();
	cursor_forget();

	uint32_t line_bytes = fb_width * (fb_bpp / 8);
	int char_pixels = lines * 16;
	uint8_t *base = (uint8_t *)fb_addr;

	if (char_pixels > 0) {
		for (uint32_t y = char_pixels; y < fb_height; y++) {
			uint8_t *src = base + y * fb_pitch;
			uint8_t *dst = base + (y - char_pixels) * fb_pitch;

			for (uint32_t i = 0; i < line_bytes; i++) {
				dst[i] = src[i];
			}
		}

		unsigned char saved_r = color_r;
		unsigned char saved_g = color_g;
		unsigned char saved_b = color_b;

		__asm__ volatile ("cli");
		change_color(bg_r, bg_g, bg_b);

		for (uint32_t y = fb_height - char_pixels; y < fb_height; y++) {
			for (uint32_t x = 0; x < fb_width; x++) {
				draw_pixel(x, y);
			}
		}

		change_color(saved_r, saved_g, saved_b);
		__asm__ volatile ("sti");
	}
}

// 打印单字符
void print_char(char c) {
	CURSOR_LOCK();
	cursor_hide();

	if (c == '\r') {
		cursor_setpos(0, cursor_row);
	} else if (c == '\n') {
		int nr = cursor_row + 1;

		if ((uint32_t)nr >= total_rows) {
			scroll_screen(1);
			nr = nr - 1;
		}

		cursor_setpos(0, nr);
	} else if (c == '\b') {
		if (cursor_col > 0) {
			cursor_setpos(cursor_col - 1, cursor_row);
			draw_char(cursor_col, cursor_row, ' ');
		}
	} else {
		if (font_index(c) < 0) {
			c = ' ';
		}

		draw_char(cursor_col, cursor_row, c);

		int nc = cursor_col + 1;
		int nr = cursor_row;

		if ((uint32_t)nc >= total_cols) {
			nc = 0;
			nr = cursor_row + 1;

			if ((uint32_t)nr >= total_rows) {
				scroll_screen(1);
				nr = nr - 1;
			}
		}

		cursor_setpos(nc, nr);
	}

	cursor_visible = 1;
	cursor_blink_next = ticks + CURSOR_BLINK_MS;

	cursor_show();
	CURSOR_UNLOCK();
}

// 打印字符串
void print(const char *s) {
	while (*s != '\0') {
		print_char(*s);
		s++;
	}
}

// 打印无符号整数
void print_uint(uint32_t n) {
	char buf[16];
	int i = 0;

	if (n == 0) {
		print_char('0');
		return;
	}

	while (n > 0) {
		buf[i++] = '0' + (n % 10);
		n /= 10;
	}

	while (i > 0) {
		print_char(buf[--i]);
	}
}

// 打印时间戳
void print_timestamp(void) {
	uint32_t sec = ticks / 1000;
	uint32_t ms = ticks % 1000;
	uint32_t us = ms * 1000;

	print_char('[');

	for (int i = 0; i < 4; i++) {
		print_char(' ');
	}

	print_uint(sec);
	print_char('.');

	if (us < 100000) {
		print_char('0');
	}
	if (us < 10000) {
		print_char('0');
	}
	if (us < 1000) {
		print_char('0');
	}
	if (us < 100) {
		print_char('0');
	}
	if (us < 10) {
		print_char('0');
	}

	print_uint(us);
	print_char(']');
	print_char(' ');
}

// 整屏清屏
void clear_screen(void) {
	CURSOR_LOCK();
	cursor_hide();

	change_color(0, 0, 0);

	for (uint32_t y = 0; y < fb_height; y++) {
		for (uint32_t x = 0; x < fb_width; x++) {
			draw_pixel(x, y);
		}
	}

	cursor_forget();
	CURSOR_UNLOCK();
}

// 字符串比较
int strcmp(const char *a, const char *b) {
	while (*a && *b && *a == *b) {
		a++;
		b++;
	}

	return *a - *b;
}

// 解析 Multiboot 2 信息
void parse_multiboot2(uint64_t mb2_info_addr) {
	uint8_t *ptr = (uint8_t *)mb2_info_addr;
	uint32_t total_size = *(uint32_t *)ptr;
	uint8_t *tag_ptr = ptr + 8;
	uint8_t *end_ptr = ptr + total_size;

	while (tag_ptr < end_ptr) {
		struct mb2_tag *tag = (struct mb2_tag *)tag_ptr;

		if (tag->type == MB2_TAG_TYPE_END) {
			break;
		}

		if (tag->type == MB2_TAG_TYPE_FRAMEBUFFER) {
			struct mb2_tag_framebuffer *fb = (struct mb2_tag_framebuffer *)tag_ptr;
			fb_addr = fb->framebuffer_addr;
			fb_width = fb->framebuffer_width;
			fb_height = fb->framebuffer_height;
			fb_bpp = fb->framebuffer_bpp;
			fb_pitch = fb->framebuffer_pitch;
		}

		tag_ptr += (tag->size + 7) & ~7;
	}
}

// 设置一个 IDT 项
void idt_set_gate(int n, uint64_t handler) {
	idt[n].offset_low = handler & 0xFFFF;
	idt[n].selector = 0x08;
	idt[n].ist = 0;
	idt[n].type_attr = 0x8E;
	idt[n].offset_mid = (handler >> 16) & 0xFFFF;
	idt[n].offset_high = (handler >> 32) & 0xFFFFFFFF;
	idt[n].zero = 0;
}

// 初始化 IDT，加载到 CPU
void idt_init(void) {
	extern void isr32(void);
	extern void isr33(void);

	idtp.limit = sizeof(idt) - 1;
	idtp.base = (uint64_t)&idt;

	for (int i = 0; i < 256; i++) {
		idt_set_gate(i, (uint64_t)isr32);
	}

	idt_set_gate(33, (uint64_t)isr33);

	__asm__ volatile ("lidt %0" : : "m"(idtp));
}

// 初始化 8259 PIC，先屏蔽 IRQ1 键盘，等进入 shell 前再解除
void pic_init(void) {
	__asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)0x11), "d"(PIC1_COMMAND));
	__asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)0x00), "d"((uint16_t)0x80));

	__asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)0x11), "d"(PIC2_COMMAND));
	__asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)0x00), "d"((uint16_t)0x80));

	__asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)32), "d"(PIC1_DATA));
	__asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)0x00), "d"((uint16_t)0x80));

	__asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)40), "d"(PIC2_DATA));
	__asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)0x00), "d"((uint16_t)0x80));

	__asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)4), "d"(PIC1_DATA));
	__asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)0x00), "d"((uint16_t)0x80));

	__asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)2), "d"(PIC2_DATA));
	__asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)0x00), "d"((uint16_t)0x80));

	__asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)1), "d"(PIC1_DATA));
	__asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)0x00), "d"((uint16_t)0x80));

	__asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)1), "d"(PIC2_DATA));
	__asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)0x00), "d"((uint16_t)0x80));

	// 屏蔽 IRQ1 键盘，等进入 shell 前再解除
	__asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)0xFE), "d"(PIC1_DATA));
	__asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)0x00), "d"((uint16_t)0x80));

	__asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)0xFF), "d"(PIC2_DATA));
	__asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)0x00), "d"((uint16_t)0x80));
}

#define PIT_CHANNEL0	0x40
#define PIT_COMMAND	0x43
#define PIT_FREQ	1193182
#define PIT_HZ		1000

// PIT 初始化，设 1000Hz
void pit_init(void) {
	uint32_t divisor = PIT_FREQ / PIT_HZ;

	__asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)0x36), "d"(PIT_COMMAND));
	__asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)(divisor & 0xFF)), "d"(PIT_CHANNEL0));
	__asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)((divisor >> 8) & 0xFF)), "d"(PIT_CHANNEL0));
}

// PIT 中断处理，每毫秒一次
void timer_handler(void) {
	ticks++;

	__asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)0x20), "d"(PIC1_COMMAND));
}

// 键盘中断处理
void keyboard_handler(void) {
	static int extended = 0;

	uint8_t scancode = 0;

	__asm__ volatile ("inb %%dx, %%al" : "=a"(scancode) : "d"((uint16_t)0x60));

	if (scancode == 0xE0) {
		extended = 1;

		__asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)0x20), "d"(PIC1_COMMAND));
		return;
	}

	if (scancode == 0x2A || scancode == 0x36) {
		shift_pressed = 1;

		__asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)0x20), "d"(PIC1_COMMAND));
		return;
	}

	if (scancode == 0xAA || scancode == 0xB6) {
		shift_pressed = 0;

		__asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)0x20), "d"(PIC1_COMMAND));
		return;
	}

	if (scancode == 0x3A) {
		caps_lock_on = !caps_lock_on;

		__asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)0x20), "d"(PIC1_COMMAND));
		return;
	}

	if (scancode == 0xBA) {
		__asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)0x20), "d"(PIC1_COMMAND));
		return;
	}

	char c = 0;

	if (extended) {
		extended = 0;

		if (scancode < 0x80) {
			if (scancode == 0x4B) {
				c = 0x01;
			} else if (scancode == 0x4D) {
				c = 0x02;
			} else if (scancode == 0x47) {
				c = 0x03;
			} else if (scancode == 0x4F) {
				c = 0x04;
			} else if (scancode == 0x53) {
				c = 0x05;
			} else if (scancode == 0x48) {
				c = 0x06;
			} else if (scancode == 0x50) {
				c = 0x07;
			}
		}
	} else if (scancode < 0x80) {
		c = shift_pressed ? scancode_map_shift[scancode] : scancode_map[scancode];

		if (c >= 'a' && c <= 'z' && caps_lock_on) {
			c = c - 'a' + 'A';
		} else if (c >= 'A' && c <= 'Z' && caps_lock_on) {
			c = c - 'A' + 'a';
		}

		if (scancode == 0x1C) {
			c = '\n';
		} else if (scancode == 0x0E) {
			c = '\b';
		}
	}

	if (c != 0) {
		int next = (key_head + 1) % KEY_BUF_SIZE;

		if (next != key_tail) {
			key_buf[key_head] = c;
			key_head = next;
		}
	}

	__asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)0x20), "d"(PIC1_COMMAND));
}

// 从按键队列取一个字符，队列空返回 0
char get_key(void) {
	if (key_head == key_tail) {
		return 0;
	}

	char c = key_buf[key_tail];
	key_tail = (key_tail + 1) % KEY_BUF_SIZE;

	return c;
}

// 毫秒级延时
void delay_ms(uint32_t ms) {
	uint64_t start = ticks;

	while (ticks - start < ms) {
		__asm__ volatile ("hlt");
	}
}

// 内核入口，由 boot.asm 调用
void kernel_main(uint64_t mb2_info_addr) {
	parse_multiboot2(mb2_info_addr);

	total_cols = fb_width / 8;
	total_rows = fb_height / 16;

	idt_init();
	pic_init();
	pit_init();

	__asm__ volatile ("sti");

	clear_screen();

	print_timestamp();
	print("DogusOS v0.1\n");
	delay_ms(100);

	print_timestamp();
	print("Arch: x86_64\n");
	delay_ms(100);

	print_timestamp();
	print("Multiboot2 info parsed\n");
	delay_ms(100);

	print_timestamp();
	print("Framebuffer: ");
	print_uint(fb_width);
	print_char('x');
	print_uint(fb_height);
	print(", ");
	print_uint(fb_bpp);
	print("bpp\n");
	delay_ms(100);

	print_timestamp();
	print("Text grid: ");
	print_uint(total_cols);
	print_char('x');
	print_uint(total_rows);
	print_char('\n');
	delay_ms(100);

	print_timestamp();
	print("IDT initialized\n");
	delay_ms(100);

	print_timestamp();
	print("PIC initialized\n");
	delay_ms(100);

	print_timestamp();
	print("PIT initialized at 1000Hz\n");
	delay_ms(100);

	print_timestamp();
	print("Kernel ready... ");

	for (int i = 0; i < 10; i++) {
		print("|");
		delay_ms(100);
		cursor_move(-1, 0);
		print("/");
		delay_ms(100);
		cursor_move(-1, 0);
		print("-");
		delay_ms(100);
		cursor_move(-1, 0);
		print("\\");
		delay_ms(100);
		cursor_move(-1, 0);
	}

	uint8_t status = 0;

	do {
		__asm__ volatile ("inb %%dx, %%al" : "=a"(status) : "d"((uint16_t)0x64));

		if (status & 0x01) {
			uint8_t discard = 0;
			__asm__ volatile ("inb %%dx, %%al" : "=a"(discard) : "d"((uint16_t)0x60));
		}
	} while (status & 0x01);

	uint8_t mask = 0;
	__asm__ volatile ("inb %%dx, %%al" : "=a"(mask) : "d"((uint16_t)PIC1_DATA));
	mask &= ~(1 << 1);
	__asm__ volatile ("outb %%al, %%dx" : : "a"(mask), "d"(PIC1_DATA));

	clear_screen();

	key_head = 0;
	key_tail = 0;

	shell_init();
	shell_run();
}