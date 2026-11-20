// 虚拟内存管理，每任务独立 PML4/PDPT/PD，复制内核低 4GB 恒等映射
#include <stdint.h>
#include "kernel.h"
#include "vmm.h"
#include "pmm.h"

#define PAGE_SIZE	4096
#define ENTRIES		512

extern uint64_t pml4_table[];
extern uint64_t pdpt_table[];
extern uint64_t pd_table[];

static uint64_t kernel_pd_phys;

void vmm_init(void) {
	kernel_pd_phys = (uint64_t)(uintptr_t)pd_table;
}

uint64_t vmm_create_task_pml4(void) {
	uint64_t new_pml4 = pmm_alloc_frame();

	if (new_pml4 == 0) {
		return 0;
	}

	uint64_t new_pdpt = pmm_alloc_frame();

	if (new_pdpt == 0) {
		pmm_free_frame(new_pml4);
		return 0;
	}

	uint64_t new_pd[4];

	for (int i = 0; i < 4; i++) {
		new_pd[i] = pmm_alloc_frame();

		if (new_pd[i] == 0) {
			for (int j = 0; j < i; j++) {
				pmm_free_frame(new_pd[j]);
			}

			pmm_free_frame(new_pdpt);
			pmm_free_frame(new_pml4);
			return 0;
		}
	}

	uint64_t *pml4 = (uint64_t *)new_pml4;
	uint64_t *pdpt = (uint64_t *)new_pdpt;

	for (int i = 0; i < ENTRIES; i++) {
		pml4[i] = 0;
		pdpt[i] = 0;
	}

	pml4[0] = new_pdpt | 0x03;

	for (int i = 0; i < 4; i++) {
		pdpt[i] = new_pd[i] | 0x03;
	}

	uint64_t *kernel_pd = (uint64_t *)kernel_pd_phys;

	for (int i = 0; i < 4; i++) {
		uint64_t *pd = (uint64_t *)new_pd[i];

		for (int j = 0; j < ENTRIES; j++) {
			pd[j] = kernel_pd[i * ENTRIES + j];
		}
	}

	return new_pml4;
}

// 把指定地址所在的 2MB 大页标记为用户可访问
void vmm_mark_user_page(uint64_t cr3, uint64_t vaddr) {
	uint64_t pml4_idx = (vaddr >> 39) & 0x1FF;
	uint64_t pdpt_idx = (vaddr >> 30) & 0x1FF;
	uint64_t pd_idx = (vaddr >> 21) & 0x1FF;

	uint64_t pd_base = (cr3 & 0xFFFFFFFFFF000ULL);
	uint64_t *pml4 = (uint64_t *)pd_base;

	if (pml4[pml4_idx] == 0) {
		return;
	}

	pml4[pml4_idx] |= 0x04;
	uint64_t pdpt_phys = pml4[pml4_idx] & 0xFFFFFFFFFF000ULL;
	uint64_t *pdpt = (uint64_t *)pdpt_phys;

	if (pdpt[pdpt_idx] == 0) {
		return;
	}

	pdpt[pdpt_idx] |= 0x04;
	uint64_t pd_phys = pdpt[pdpt_idx] & 0xFFFFFFFFFF000ULL;
	uint64_t *pd = (uint64_t *)pd_phys;

	if (pd[pd_idx] == 0) {
		return;
	}

	pd[pd_idx] |= 0x04;

	__asm__ volatile ("mov %%cr3, %%rax\n\tmov %%rax, %%cr3" : : : "rax");
}