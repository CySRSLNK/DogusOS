// 物理页框分配器，位图法，1 位对应 1 个 4KB 页框
#include <stdint.h>
#include "kernel.h"
#include "pmm.h"

#define PAGE_SIZE	4096
#define MAX_PHYS_ADDR	0x100000000ULL
#define MAX_FRAMES	(MAX_PHYS_ADDR / PAGE_SIZE)
#define BITMAP_SIZE	(MAX_FRAMES / 8)

// Multiboot2 内存映射条目，固定 24 字节
struct mb2_mmap_entry {
	uint64_t addr;
	uint64_t len;
	uint32_t type;
	uint32_t zero;
};

static uint8_t bitmap[BITMAP_SIZE];
static uint32_t total_frames = 0;
static uint32_t free_frames = 0;

extern char _kernel_end;

// 置位为已用
static void bitmap_set(uint32_t frame) {
	bitmap[frame / 8] |= (uint8_t)(1 << (frame % 8));
}

// 清位为可用
static void bitmap_clear(uint32_t frame) {
	bitmap[frame / 8] &= (uint8_t)~(1 << (frame % 8));
}

// 测试某位，1 表示已用
static int bitmap_test(uint32_t frame) {
	return (bitmap[frame / 8] >> (frame % 8)) & 1;
}

void pmm_init(uint64_t mmap_addr, uint32_t mmap_size) {
	for (uint32_t i = 0; i < BITMAP_SIZE; i++) {
		bitmap[i] = 0xFF;
	}

	free_frames = 0;

	uint8_t *p = (uint8_t *)mmap_addr;
	uint8_t *end = p + mmap_size;

	while (p < end) {
		struct mb2_mmap_entry *e = (struct mb2_mmap_entry *)p;

		if (e->type == 1) {
			uint64_t start = e->addr;
			uint64_t stop = e->addr + e->len;

			uint64_t first = (start + PAGE_SIZE - 1) / PAGE_SIZE;
			uint64_t last = stop / PAGE_SIZE;

			for (uint64_t f = first; f < last && f < MAX_FRAMES; f++) {
				if (bitmap_test((uint32_t)f)) {
					bitmap_clear((uint32_t)f);
					free_frames++;
				}
			}
		}

		p += sizeof(struct mb2_mmap_entry);
	}

	// 保留低 1MB，BIOS、GRUB、实模式残留都在这
	uint64_t low_reserved = 0x100000 / PAGE_SIZE;

	for (uint64_t f = 0; f < low_reserved && f < MAX_FRAMES; f++) {
		if (!bitmap_test((uint32_t)f)) {
			bitmap_set((uint32_t)f);
			free_frames--;
		}
	}

	// 保留内核自身占用的区间
	uint64_t kernel_end_phys = (uint64_t)(uintptr_t)&_kernel_end;
	uint64_t kernel_reserved = (kernel_end_phys + PAGE_SIZE - 1) / PAGE_SIZE;

	for (uint64_t f = low_reserved; f < kernel_reserved && f < MAX_FRAMES; f++) {
		if (!bitmap_test((uint32_t)f)) {
			bitmap_set((uint32_t)f);
			free_frames--;
		}
	}

	// 保留 .user_test 所在的整个 2MB 区间，避免用户页与分配页重叠
	extern char _user_test_start;
	uint64_t ut = (uint64_t)(uintptr_t)&_user_test_start;
	uint64_t ut_first = (ut & ~0x1FFFFFULL) / PAGE_SIZE;
	uint64_t ut_last = ut_first + (0x200000 / PAGE_SIZE);

	for (uint64_t f = ut_first; f < ut_last && f < MAX_FRAMES; f++) {
		if (!bitmap_test((uint32_t)f)) {
			bitmap_set((uint32_t)f);
			free_frames--;
		}
	}

	total_frames = free_frames;
}

// 从低地址往高地址找第一个空闲页框，成功返回物理地址，失败返回 0
uint64_t pmm_alloc_frame(void) {
	if (free_frames == 0) {
		return 0;
	}

	for (uint32_t i = 0; i < MAX_FRAMES; i++) {
		if (!bitmap_test(i)) {
			bitmap_set(i);
			free_frames--;
			return (uint64_t)i * PAGE_SIZE;
		}
	}

	return 0;
}

// 释放页框，地址须为 4KB 对齐
void pmm_free_frame(uint64_t addr) {
	uint32_t f = (uint32_t)(addr / PAGE_SIZE);

	if (f >= MAX_FRAMES) {
		return;
	}

	if (bitmap_test(f)) {
		bitmap_clear(f);
		free_frames++;
	}
}

uint32_t pmm_total_frames(void) {
	return total_frames;
}

uint32_t pmm_free_frames(void) {
	return free_frames;
}