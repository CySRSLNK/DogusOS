// ATA PIO 驱动，读硬盘扇区
#include <stdint.h>
#include "disk.h"

#define ATA_DATA	0x1F0
#define ATA_ERROR	0x1F1
#define ATA_SECCOUNT	0x1F2
#define ATA_LBA_LOW	0x1F3
#define ATA_LBA_MID	0x1F4
#define ATA_LBA_HIGH	0x1F5
#define ATA_DRIVE	0x1F6
#define ATA_STATUS	0x1F7
#define ATA_COMMAND	0x1F7
#define ATA_CONTROL	0x3F6

static void ata_wait_bsy(void) {
	while (1) {
		uint8_t status = 0;
		__asm__ volatile ("inb %%dx, %%al" : "=a"(status) : "d"((uint16_t)ATA_STATUS));

		if (!(status & 0x80)) {
			break;
		}
	}
}

static void ata_wait_ready(void) {
	while (1) {
		uint8_t status = 0;
		__asm__ volatile ("inb %%dx, %%al" : "=a"(status) : "d"((uint16_t)ATA_STATUS));

		if (!(status & 0x80) && (status & 0x08)) {
			break;
		}
	}
}

void disk_init(void) {
	__asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)0xE0), "d"((uint16_t)ATA_DRIVE));
	ata_wait_bsy();
}

int disk_read_sectors(uint32_t lba, uint8_t count, void *buf) {
	if (count == 0) {
		return 0;
	}

	ata_wait_bsy();

	__asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)(0xE0 | ((lba >> 24) & 0x0F))), "d"((uint16_t)ATA_DRIVE));
	__asm__ volatile ("outb %%al, %%dx" : : "a"(count), "d"((uint16_t)ATA_SECCOUNT));
	__asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)(lba & 0xFF)), "d"((uint16_t)ATA_LBA_LOW));
	__asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)((lba >> 8) & 0xFF)), "d"((uint16_t)ATA_LBA_MID));
	__asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)((lba >> 16) & 0xFF)), "d"((uint16_t)ATA_LBA_HIGH));
	__asm__ volatile ("outb %%al, %%dx" : : "a"((uint8_t)0x20), "d"((uint16_t)ATA_COMMAND));

	uint16_t *ptr = (uint16_t *)buf;

	for (int s = 0; s < count; s++) {
		ata_wait_ready();

		for (int i = 0; i < 256; i++) {
			uint16_t value = 0;
			__asm__ volatile ("inw %%dx, %%ax" : "=a"(value) : "d"((uint16_t)ATA_DATA));
			ptr[s * 256 + i] = value;
		}
	}

	return 0;
}