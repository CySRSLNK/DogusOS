#ifndef PMM_H
#define PMM_H

#include <stdint.h>

void pmm_init(uint64_t mmap_addr, uint32_t mmap_size);
uint64_t pmm_alloc_frame(void);
void pmm_free_frame(uint64_t addr);
uint32_t pmm_total_frames(void);
uint32_t pmm_free_frames(void);

#endif