// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(PLATFORM_RK3576)

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <net/if.h>   // must be included later than <net/if.h>
#include <linux/if.h> // must be included later than <net/if.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/input.h>

#define MAX_NL_BUF_SIZE (1024 * 16)
#define MAX_SELECT_TIMEOUT (5 * 1000 * 1000)

#define SUSPEND_TIME_REG 0x20834310


static int32_t SAMPLE_COMM_WriteReg(int32_t addr, int32_t value) {
	int mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (mem_fd < 0) {
		perror("Error opening /dev/mem");
		return -1;
	}

	// 获取页大小
	size_t page_size = getpagesize();

	// 计算页对齐的地址和偏移量
	off_t page_base = (addr & ~(page_size - 1));
	off_t page_offset = addr - page_base;

	// 映射内存
	void *mem_map =
	    mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, page_base);
	if (mem_map == MAP_FAILED) {
		perror("Error mapping memory");
		close(mem_fd);
		return -1;
	}

	// 计算寄存器地址
	int *target_reg = (int *)((char *)mem_map + page_offset);

	// 写入数据
	*target_reg = value;

	// 解除内存映射
	munmap(mem_map, page_size);
	close(mem_fd);

	return 0;
}

static int32_t SAMPLE_COMM_ReadReg(int32_t addr, uint8_t *buf, uint32_t len) {
	int mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (mem_fd < 0) {
		perror("Error opening /dev/mem");
		return -1;
	}

	// 获取页大小
	size_t page_size = getpagesize();
	if (len > page_size) {
		printf("Dump length is too long!\n");
		return -1;
	}

	// 计算页对齐的地址和偏移量
	off_t page_base = (addr & ~(page_size - 1));
	off_t page_offset = addr - page_base;

	// 映射内存
	void *mem_map =
	    mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, page_base);
	if (mem_map == MAP_FAILED) {
		perror("Error mapping memory");
		close(mem_fd);
		return -1;
	}

	// 计算寄存器地址
	const uint8_t *target_reg = (uint8_t *)((char *)mem_map + page_offset);

	// 读取数据
	memcpy(buf, target_reg, len);

	// 解除内存映射
	munmap(mem_map, page_size);
	close(mem_fd);

	return 0;
}

static bool device_driver_is_bound(const char *device, const char *driver) {
	char path[256] = {'\0'};
	snprintf(path, 256, "%s/%s", driver, device);
	return (access(path, F_OK) == 0);
}

static int device_attach_driver(const char *device, const char *driver) {
	char path[256] = {'\0'};
	int fd = -1;
	int ret = 0;
	// printf("[%s()] Enter\n", __func__);
	snprintf(path, sizeof(path), "%s/bind", driver);
	fd = open(path, O_WRONLY | O_NONBLOCK | O_CLOEXEC);
	if (fd < 0) {
		printf("[%s()] can't open %s because %s\n", __func__, driver, strerror(errno));
		return -1;
	}
	// printf("[%s()] start bind %s to %s\n", __func__, device, driver);
	ret = write(fd, device, strlen(device));
	if (ret < 0) {
		printf("[%s()] bind %s to %s failed because %s\n", __func__, device, driver,
		       strerror(errno));
		close(fd);
		return -1;
	}
	close(fd);
	// printf("[%s()] Exit\n", __func__);
	return 0;
}

static int device_detach_driver(const char *device, const char *driver) {
	char path[256] = {'\0'};
	int fd = -1;
	int ret = 0;
	// printf("[%s()] Enter\n", __func__);
	snprintf(path, sizeof(path), "%s/unbind", driver);
	fd = open(path, O_WRONLY | O_NONBLOCK | O_CLOEXEC);
	if (fd < 0) {
		printf("[%s()] can't open %s because %s\n", __func__, path, strerror(errno));
		return -1;
	}
	// printf("[%s()] start unbind %s from %s\n", __func__, device, driver);
	ret = write(fd, device, strlen(device));
	if (ret < 0) {
		printf("[%s()] unbind %s from %s failed because %s\n", __func__, device, driver,
		       strerror(errno));
		close(fd);
		return -1;
	}
	close(fd);
	// printf("[%s()] Exit\n", __func__);
	return 0;
}

int SAMPLE_COMM_AOV_SetSuspendTime(int wakeup_suspend_time) {
	int result = SAMPLE_COMM_WriteReg(SUSPEND_TIME_REG, wakeup_suspend_time * 32.768);
	if (result != 0) {
		printf("Failed to set suspend time!\n");
		return -1;
	}
	printf("[%s()] wakeup suspend time = %d\n", __func__, wakeup_suspend_time);
	return 0;
}

int SAMPLE_COMM_AOV_BindEthernet() {
	return 0;
}

int SAMPLE_COMM_AOV_UnbindEthernet() {
	return 0;
}

static bool detect_sdcard_is_enable() {
	return true;
}

int SAMPLE_COMM_AOV_BindSdcard() {
	return 0;
}

int SAMPLE_COMM_AOV_UnbindSdcard() {
	return 0;
}

int SAMPLE_COMM_AOV_BindSoundcard() {
	return 0;
}

int SAMPLE_COMM_AOV_UnbindSoundcard() {
	return 0;
}

int SAMPLE_COMM_AOV_BindEmmc() {
	return 0;
}

int SAMPLE_COMM_AOV_UnbindEmmc() {
	return 0;
}

int SAMPLE_COMM_AOV_DisableNonBootCPUs() {
	return 0;
}

int SAMPLE_COMM_AOV_EnableNonBootCPUs() {
}

int SAMPLE_COMM_AOV_BindUsb() {
	return 0;
}
int SAMPLE_COMM_AOV_UnbindUsb() {
	return 0;
}

int SAMPLE_COMM_AOV_BindSDIO() {
	return 0;
}

int SAMPLE_COMM_AOV_UnbindSDIO() {
	return 0;
}

#endif