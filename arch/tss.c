// 64 位 TSS，提供 ring3 到 ring0 切换时用的 RSP0
#include <stdint.h>
#include "tss.h"

// x86-64 TSS，共 104 字节
struct tss_entry {
	uint32_t reserved0;
	uint64_t rsp0;
	uint64_t rsp1;
	uint64_t rsp2;
	uint64_t reserved1;
	uint64_t ist[7];
	uint64_t reserved2;
	uint16_t reserved3;
	uint16_t iomap_base;
} __attribute__((packed));

static struct tss_entry tss;

extern uint64_t gdt64[];

// 把 TSS 描述符写入 GDT 的第 5、6 项，并加载 TR
void tss_init(void) {
	uint8_t *p = (uint8_t *)&tss;

	for (uint32_t i = 0; i < sizeof(struct tss_entry); i++) {
		p[i] = 0;
	}

	tss.iomap_base = sizeof(struct tss_entry);

	uint64_t base = (uint64_t)(uintptr_t)&tss;
	uint32_t limit = sizeof(struct tss_entry) - 1;

	uint64_t low = 0;

	low |= (uint64_t)(limit & 0xFFFF);
	low |= (uint64_t)(base & 0xFFFFFF) << 16;
	low |= (uint64_t)0x89 << 40;
	low |= (uint64_t)((limit >> 16) & 0xF) << 48;
	low |= (uint64_t)((base >> 24) & 0xFF) << 56;

	gdt64[5] = low;
	gdt64[6] = base >> 32;

	__asm__ volatile ("ltr %%ax" : : "a"((uint16_t)0x28));
}

void tss_set_rsp0(uint64_t rsp0) {
	tss.rsp0 = rsp0;
}