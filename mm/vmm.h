#ifndef VMM_H
#define VMM_H

#include <stdint.h>

void vmm_init(void);
uint64_t vmm_create_task_pml4(void);
void vmm_mark_user_page(uint64_t cr3, uint64_t vaddr);

#endif