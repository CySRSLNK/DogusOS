# DogusOS 构建系统，读取 version.h 生成 grub.cfg 并制作磁盘镜像

# 从 version.h 提取宏值，唯一数据源
OS_NAME := $(shell gcc -dM -E version.h | awk '/#define OS_NAME / {print $$3}' | tr -d '"')
OS_VERSION := $(shell gcc -dM -E version.h | awk '/#define OS_VERSION / {print $$3}' | tr -d '"')
OS_ARCH := $(shell gcc -dM -E version.h | awk '/#define OS_ARCH / {print $$3}' | tr -d '"')
OS_BITS := $(shell gcc -dM -E version.h | awk '/#define OS_BITS / {print $$3}' | tr -d '"')

# 镜像文件名：name-version-arch_bits.img
IMG_NAME := $(shell echo $(OS_NAME) | sed 's/OS$$//' | tr 'A-Z' 'a-z')-$(OS_VERSION)-$(shell echo $(OS_ARCH) | tr 'A-Z' 'a-z')_$(OS_BITS).img

# 工具与参数
CC := gcc
AS := nasm
LD := ld
CFLAGS := -std=gnu17 -Wall -Wextra -O0 -g -ffreestanding -fno-pie -mno-red-zone -mcmodel=kernel
ASFLAGS := -f elf64
LDFLAGS := -T linker.ld -nostdlib -z noexecstack

# QEMU 参数
QEMU := qemu-system-x86_64
QEMU_FLAGS := -m 512M -display gtk -device VGA,edid=on,xres=640,yres=400 -drive file=$(IMG_NAME),format=raw

.PHONY: all run clean debug

# 默认目标：每次强制重新构建并生成镜像
all:
	$(AS) $(ASFLAGS) boot.asm -o boot.o
	$(CC) $(CFLAGS) -c kernel.c -o kernel.o
	$(CC) $(CFLAGS) -c shell.c -o shell.o
	$(CC) $(CFLAGS) -c commands.c -o commands.o
	$(LD) $(LDFLAGS) boot.o kernel.o shell.o commands.o -o kernel.elf
	@echo 'set gfxmode=640x400' > grub.cfg
	@echo 'set timeout=0' >> grub.cfg
	@echo 'set timeout_style=hidden' >> grub.cfg
	@echo 'set default=0' >> grub.cfg
	@echo '' >> grub.cfg
	@echo 'menuentry "$(OS_NAME) $(OS_VERSION)" {' >> grub.cfg
	@echo '	multiboot2 /boot/kernel.elf' >> grub.cfg
	@echo '}' >> grub.cfg
	@echo "创建镜像 $(IMG_NAME)"
	@dd if=/dev/zero of=$(IMG_NAME) bs=1M count=1024 status=none
	@LOOP=$$(sudo losetup -f); \
	sudo losetup $$LOOP $(IMG_NAME); \
	sudo parted -s $$LOOP mklabel msdos; \
	sudo parted -s $$LOOP mkpart primary ext2 1MiB 100%; \
	sudo partprobe $$LOOP; \
	sleep 1; \
	PART=$${LOOP}p1; \
	sudo mkfs.ext2 -q $$PART; \
	MNT=$$(mktemp -d); \
	sudo mount $$PART $$MNT; \
	sudo mkdir -p $$MNT/boot/grub; \
	sudo cp kernel.elf $$MNT/boot/kernel.elf; \
	sudo cp grub.cfg $$MNT/boot/grub/grub.cfg; \
	sudo grub-install --target=i386-pc --boot-directory=$$MNT/boot --modules="multiboot2 normal" $$LOOP; \
	sudo umount $$MNT; \
	rmdir $$MNT; \
	sudo losetup -d $$LOOP; \
	echo "镜像生成完成"

# 启动 QEMU 测试，不依赖 all，需先 make all
run:
	$(QEMU) $(QEMU_FLAGS)

# 启动 QEMU 并等待 GDB 连接，不依赖 all，需先 make all
debug:
	$(QEMU) $(QEMU_FLAGS) -s -S

# 清理编译产物，保留源文件与 img 文件
clean:
	@echo "清理编译产物"
	@for mnt in $$(mount | awk '/DogusOS/ {print $$3}'); do \
		sudo umount $$mnt 2>/dev/null || true; \
	done
	@for loop in $$(losetup -a | awk -F: '/$(IMG_NAME)/ {print $$1}'); do \
		sudo losetup -d $$loop 2>/dev/null || true; \
	done
	@rm -f boot.o kernel.o shell.o commands.o kernel.elf grub.cfg
	@rm -rf tmp_mnt
	@echo "清理完成，保留源文件与 $(IMG_NAME)"