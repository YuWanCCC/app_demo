// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(PLATFORM_RV1126)

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

#define SUSPEND_TIME_REG 0xff3e0048

#define MAX_NL_BUF_SIZE (1024 * 16)
#define MAX_SELECT_TIMEOUT (2 * 1000 * 1000)


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
	int result = SAMPLE_COMM_WriteReg(SUSPEND_TIME_REG, wakeup_suspend_time * 91);
	// sprintf(wakeup_cmd, "io -4 0xff3e0048 910000");
	if (result != 0) {
		printf("Failed to set suspend time!\n");
		return -1;
	}
	printf("[%s()] wakeup suspend time = %d\n", __func__, wakeup_suspend_time);
	return 0;
}

#define ETHERNET_DEVICE "ffa80000.ethernet"
#define ETHERNET_DRIVER "/sys/bus/platform/drivers/rk_gmac-dwmac/"
// #define NIC_DEVICE "stmmac-0:02"
// #define NIC_DRIVER "/sys/bus/mdio_bus/drivers/RK630 PHY/"
#define ETHERNET_BIND_DONE "/sys/bus/platform/drivers/rk_gmac-dwmac/"
#define ETHERNET_UNBNID_DONE "/sys/bus/platform/drivers/rk_gmac-dwmac/"

int SAMPLE_COMM_AOV_BindEthernet() {
	char buf[MAX_NL_BUF_SIZE] = {'\0'};
	char name[256] = {'\0'};
	int ret = 0;
	int fd = -1;
	int len = MAX_NL_BUF_SIZE;
	fd_set read_set;
	struct ifreq ifr;
	struct timeval timeout;
	struct sockaddr_nl addr;
	struct nlmsghdr *nh = NULL;
	struct ifinfomsg *ifinfo = NULL;

	printf("[%s()] Enter\n", __func__);
	if (device_driver_is_bound(ETHERNET_DEVICE, ETHERNET_DRIVER)) {
		printf("[%s()] ethernet device already bind!\n", __func__);
		goto __FAILED;
	}

	fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &len, sizeof(len));
	memset(&addr, 0, sizeof(addr));
	addr.nl_family = AF_NETLINK;
	addr.nl_groups = RTNLGRP_LINK;
	addr.nl_pid = getpid();
	if (fd < 0) {
		printf("[%s()] Failed to open network netlink because %s\n", __func__,
		       strerror(errno));
		goto __FAILED;
	} else if (bind(fd, (struct sockaddr *)(&addr), sizeof(addr)) != 0) {
		printf("[%s()] bind network netlink addr failed because %s\n", __func__,
		       strerror(errno));
		goto __FAILED;
	}

	FD_ZERO(&read_set);
	FD_SET(fd, &read_set);
	timeout.tv_sec = 0;
	timeout.tv_usec = MAX_SELECT_TIMEOUT;
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (void *)&timeout,
	           (socklen_t)sizeof(struct timeval));

	// bind ethernet driver
	if (device_attach_driver(ETHERNET_DEVICE, ETHERNET_DRIVER) != 0)
		goto __FAILED;

#if 1
	// open etherner device
	system("ifconfig eth0 up");
	// reset local ip, gateway and dns resolvation
	system("killall -9 udhcpc");                // restart udhcpc
	system("route del default gw 0.0.0.0");     // delete default gateway
	system("cat /dev/null > /etc/resolv.conf"); // reset DNS resolvation
	system("udhcpc -i eth0 -T 1 -A 0 -b -q");   // find a new ip address for eth0
#endif
	// wait for bind success
	// FIXME:
	// there has some bug for select()
	// ret = select(fd + 1, &read_set, NULL, NULL, &timeout);
	memset(&buf, 0, sizeof(buf));
	while ((ret = read(fd, buf, sizeof(buf))) > 0) {
		for (nh = (struct nlmsghdr *)buf; NLMSG_OK(nh, ret); nh = NLMSG_NEXT(nh, ret)) {
			if (nh->nlmsg_type == NLMSG_DONE) {
				printf("[%s()] NLMSG_DONE\n", __func__);
				goto __FAILED;
			}
			if (nh->nlmsg_type == NLMSG_ERROR) {
				printf("[%s()] NLMSG_ERROR\n", __func__);
				goto __FAILED;
			}
			if (nh->nlmsg_type != RTM_NEWLINK) {
				printf("[%s()] not RTM_NEWLINK, ignore\n", __func__);
				continue;
			}
			ifinfo = NLMSG_DATA(nh);
			if_indextoname(ifinfo->ifi_index, name);

			// printf("[%s()] %s: up %s, lower up %s, running %s, value 0X%X\n", __func__
			// 	, name
			// 	, (ifinfo->ifi_flags & IFF_UP) ? "true" : "false"
			// 	, (ifinfo->ifi_flags & IFF_LOWER_UP) ? "true" : "false"
			// 	, (ifinfo->ifi_flags & IFF_RUNNING) ? "true" : "false"
			// 	, ifinfo->ifi_flags
			// );

			if ((ifinfo->ifi_flags & IFF_RUNNING) && (ifinfo->ifi_flags & IFF_LOWER_UP) &&
			    (ifinfo->ifi_flags & IFF_UP)) {
				printf("[%s()] ethernet bind success!\n", __func__);
				goto __SUCCESS;
			}
		}
	}
	if (ret < 0)
		goto __FAILED;

__SUCCESS:
	if (fd >= 0)
		close(fd);
	printf("[%s()] Exit\n", __func__);
	return 0;
__FAILED:
	if (fd >= 0)
		close(fd);
	printf("[%s()] Exit Error\n", __func__);
	return -1;
}

int SAMPLE_COMM_AOV_UnbindEthernet() {
	char buf[MAX_NL_BUF_SIZE] = {'\0'};
	char name[256] = {'\0'};
	int ret = 0;
	int fd = -1;
	int len = MAX_NL_BUF_SIZE;
	fd_set read_set;
	struct sockaddr_nl addr;
	struct timeval timeout;
	struct nlmsghdr *nh = NULL;
	struct ifinfomsg *ifinfo = NULL;

	printf("[%s()] Enter\n", __func__);
	if (!device_driver_is_bound(ETHERNET_DEVICE, ETHERNET_DRIVER)) {
		printf("[%s()] ethernet device already unbind!\n", __func__);
		goto __FAILED;
	}

	fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &len, sizeof(len));
	memset(&addr, 0, sizeof(addr));
	addr.nl_family = AF_NETLINK;
	addr.nl_groups = RTNLGRP_LINK;
	addr.nl_pid = getpid();
	if (fd < 0) {
		printf("[%s()] Failed to open network netlink because %s\n", __func__,
		       strerror(errno));
		return -1;
	} else if (bind(fd, (struct sockaddr *)(&addr), sizeof(addr)) != 0) {
		printf("[%s()] bind network netlink addr failed because %s\n", __func__,
		       strerror(errno));
		goto __FAILED;
	}

	FD_ZERO(&read_set);
	FD_SET(fd, &read_set);
	timeout.tv_sec = 0;
	timeout.tv_usec = MAX_SELECT_TIMEOUT;

	// close ethernet device
	system("ifconfig eth0 0.0.0.0");
	system("ifconfig eth0 down");
	// unbind ethernet driver
	if (device_detach_driver(ETHERNET_DEVICE, ETHERNET_DRIVER) != 0)
		goto __FAILED;
	// wait for unbind done
__RETRY:
	ret = select(fd + 1, &read_set, NULL, NULL, &timeout);
	if (ret > 0) {
		memset(&buf, 0, sizeof(buf));
		ret = read(fd, buf, sizeof(buf));
		for (nh = (struct nlmsghdr *)buf; NLMSG_OK(nh, ret); nh = NLMSG_NEXT(nh, ret)) {
			if (nh->nlmsg_type == NLMSG_DONE) {
				printf("[%s()] NLMSG_DONE\n", __func__);
				goto __FAILED;
			}
			if (nh->nlmsg_type == NLMSG_ERROR) {
				printf("[%s()] NLMSG_ERROR\n", __func__);
				goto __FAILED;
			}
			ifinfo = NLMSG_DATA(nh);
			if_indextoname(ifinfo->ifi_index, name);

			// printf("[%s()] %s: up %s, lower up %s, running %s, value 0X%X\n", __func__
			// 	, name
			// 	, (ifinfo->ifi_flags & IFF_UP) ? "true" : "false"
			// 	, (ifinfo->ifi_flags & IFF_LOWER_UP) ? "true" : "false"
			// 	, (ifinfo->ifi_flags & IFF_RUNNING) ? "true" : "false"
			// 	, ifinfo->ifi_flags
			// );

			if (!(ifinfo->ifi_flags & IFF_RUNNING) &&
			    !(ifinfo->ifi_flags & IFF_LOWER_UP) && !(ifinfo->ifi_flags & IFF_UP)) {
				printf("[%s()] ethernet unbind success!\n", __func__);
				goto __SUCCESS;
			}
		}
		// printf("[%s()] Bind msg: %s\n", __func__, buf);
		goto __RETRY; // drop all message
	} else {
		printf("[%s()] select error: %s\n", __func__, strerror(errno));
		goto __FAILED;
	}
__SUCCESS:
	close(fd);
	// open etherner device
	printf("[%s()] Exit\n", __func__);
	return 0;
__FAILED:
	close(fd);
	printf("[%s()] Exit Error\n", __func__);
	return -1;
}

#define SDCARD_DEVICE "ffc60000.dwmmc"
#define SDCARD_BIND_DONE "bind@/devices/platform/ffc60000.dwmmc/mmc_host/mmc2/mmc2:b368"
#define SDCARD_UNBIND_DONE "unbind@/devices/platform/ffc60000.dwmmc"
#define SDCARD_DRIVER "/sys/bus/platform/drivers/dwmmc_rockchip"

int SAMPLE_COMM_AOV_BindSdcard() {
	int ret = 0;
	int fd = -1;
	char buf[MAX_NL_BUF_SIZE] = {'\0'};
	fd_set read_set;
	struct timeval timeout;
	struct sockaddr_nl addr;

	if (device_driver_is_bound(SDCARD_DEVICE, SDCARD_DRIVER)) {
		printf("[%s()] sdcard device already bind!\n", __func__);
		return 0;
	}

	memset(&addr, 0, sizeof(addr));
	addr.nl_family = AF_NETLINK;
	addr.nl_groups = NETLINK_KOBJECT_UEVENT;
	addr.nl_pid = 0;

	fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_KOBJECT_UEVENT);
	if (fd < 0) {
		printf("[%s()] Failed to open sdcard netlink because %s\n", __func__,
		       strerror(errno));
		return -1;
	} else if (bind(fd, (struct sockaddr *)(&addr), sizeof(addr)) != 0) {
		printf("[%s()] bind sdcard netlink addr failed because %s\n", __func__,
		       strerror(errno));
		goto __FAILED;
	}

	FD_ZERO(&read_set);
	FD_SET(fd, &read_set);
	timeout.tv_sec = 0;
	timeout.tv_usec = MAX_SELECT_TIMEOUT;

	// bind sdcard
	if (device_attach_driver(SDCARD_DEVICE, SDCARD_DRIVER) != 0)
		goto __FAILED;
	// wait for bind success
__RETRY:
	ret = select(fd + 1, &read_set, NULL, NULL, &timeout);
	if (ret > 0) {
		memset(&buf, 0, sizeof(buf));
		read(fd, buf, sizeof(buf));
		buf[MAX_NL_BUF_SIZE - 1] = '\0';
		// printf("[%s()] bind msg: %s\n", __func__, buf);
		if (strncmp(buf, SDCARD_BIND_DONE, strlen(SDCARD_BIND_DONE)) == 0) {
			printf("[%s()] Bind success: %s\n", __func__, buf);
			goto __SUCCESS;
		}
		goto __RETRY; // drop all message
	} else {
		printf("[%s()] select error %s\n", __func__, strerror(errno));
		goto __FAILED;
	}
__SUCCESS:
	close(fd);
	return 0;
__FAILED:
	close(fd);
	return -1;
}

int SAMPLE_COMM_AOV_UnbindSdcard() {
	int ret = 0;
	int fd = -1;
	char buf[MAX_NL_BUF_SIZE] = {'\0'};
	fd_set read_set;
	struct timeval timeout;
	struct sockaddr_nl addr;

	if (!device_driver_is_bound(SDCARD_DEVICE, SDCARD_DRIVER)) {
		printf("[%s()] sdcard device already unbind!\n", __func__);
		return 0;
	}

	memset(&addr, 0, sizeof(addr));
	addr.nl_family = AF_NETLINK;
	addr.nl_groups = NETLINK_KOBJECT_UEVENT;
	addr.nl_pid = 0;

	fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_KOBJECT_UEVENT);
	if (fd < 0) {
		printf("[%s()] Failed to open sdcard netlink because %s\n", __func__,
		       strerror(errno));
		return -1;
	} else if (bind(fd, (struct sockaddr *)(&addr), sizeof(addr)) != 0) {
		printf("[%s()] bind sdcard netlink addr failed because %s\n", __func__,
		       strerror(errno));
		goto __FAILED;
	}

	memset(&buf, 0, sizeof(buf));
	FD_ZERO(&read_set);
	FD_SET(fd, &read_set);
	timeout.tv_sec = 0;
	timeout.tv_usec = MAX_SELECT_TIMEOUT;

	// unbind sdcard
	if (device_detach_driver(SDCARD_DEVICE, SDCARD_DRIVER) != 0)
		goto __FAILED;
	// wait for unbind success
__RETRY:
	ret = select(fd + 1, &read_set, NULL, NULL, &timeout);
	if (ret > 0) {
		memset(&buf, 0, sizeof(buf));
		read(fd, buf, sizeof(buf));
		buf[MAX_NL_BUF_SIZE - 1] = '\0';
		// printf("[%s()] unbind msg: %s\n", __func__, buf);
		if (strcmp(buf, SDCARD_UNBIND_DONE) == 0) {
			printf("[%s()] Unbind success: %s\n", __func__, buf);
			goto __SUCCESS;
		}
		goto __RETRY; // drop all message
	} else {
		printf("[%s()] select error %s\n", __func__, strerror(errno));
		goto __FAILED;
	}
__SUCCESS:
	close(fd);
	return 0;
__FAILED:
	close(fd);
	return -1;
}

#define I2S_DRIVER "/sys/bus/platform/drivers/rockchip-i2s-tdm/"
#define ASOC_DRIVER "/sys/bus/platform/drivers/asoc-simple-card/"
#define I2S_DEVICE "ff800000.i2s"
#define ASOC_DEVICE "rk809-sound"
#define SOUND_BIND_DONE "bind@/devices/platform/rk809-sound"
#define SOUND_UNBIND_DONE "unbind@/devices/platform/rk809-sound"

int SAMPLE_COMM_AOV_BindSoundcard() {
	int ret = 0;
	int fd = -1;
	char buf[MAX_NL_BUF_SIZE] = {'\0'};
	fd_set read_set;
	struct timeval timeout;
	struct sockaddr_nl addr;

	memset(&addr, 0, sizeof(addr));
	addr.nl_family = AF_NETLINK;
	addr.nl_groups = NETLINK_KOBJECT_UEVENT;
	addr.nl_pid = 0;

	printf("[%s()] Enter\n", __func__);
	fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_KOBJECT_UEVENT);
	if (fd < 0) {
		printf("[%s()] Failed to open netlink because %s\n", __func__, strerror(errno));
		return -1;
	} else if (bind(fd, (struct sockaddr *)(&addr), sizeof(addr)) != 0) {
		printf("[%s()] bind netlink addr failed because %s\n", __func__, strerror(errno));
		goto __FAILED;
	}

	memset(&buf, 0, sizeof(buf));
	FD_ZERO(&read_set);
	FD_SET(fd, &read_set);
	timeout.tv_sec = 0;
	timeout.tv_usec = MAX_SELECT_TIMEOUT;

	device_attach_driver(I2S_DEVICE, I2S_DRIVER);
	device_attach_driver("rk817-codec", "/sys/bus/platform/drivers/rk817-codec");
	device_attach_driver("snd_aloop.0", "/sys/bus/platform/drivers/snd_aloop");
	device_attach_driver(ASOC_DEVICE, ASOC_DRIVER);

__RETRY:
	ret = select(fd + 1, &read_set, NULL, NULL, &timeout);
	if (ret > 0) {
		memset(&buf, 0, sizeof(buf));
		read(fd, buf, sizeof(buf));
		buf[MAX_NL_BUF_SIZE - 1] = '\0';
		// printf("[%s()] msg: %s\n", __func__, buf);
		if (strncmp(buf, SOUND_BIND_DONE, strlen(SOUND_BIND_DONE)) == 0) {
			printf("[%s()] Bind success: %s\n", __func__, buf);
			goto __SUCCESS;
		}
		goto __RETRY; // drop all message
	} else {
		printf("[%s()] select error %s\n", __func__, strerror(errno));
		goto __FAILED;
	}

__SUCCESS:
	close(fd);
	return 0;
__FAILED:
	close(fd);
	return 0;
}

int SAMPLE_COMM_AOV_UnbindSoundcard() {
	int ret = 0;
	int fd = -1;
	char buf[MAX_NL_BUF_SIZE] = {'\0'};
	fd_set read_set;
	struct timeval timeout;
	struct sockaddr_nl addr;

	printf("[%s()] Enter\n", __func__);
	memset(&addr, 0, sizeof(addr));
	addr.nl_family = AF_NETLINK;
	addr.nl_groups = NETLINK_KOBJECT_UEVENT;
	addr.nl_pid = 0;

	fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_KOBJECT_UEVENT);
	if (fd < 0) {
		printf("[%s()] Failed to open netlink because %s\n", __func__, strerror(errno));
		return -1;
	} else if (bind(fd, (struct sockaddr *)(&addr), sizeof(addr)) != 0) {
		printf("[%s()] bind netlink addr failed because %s\n", __func__, strerror(errno));
		goto __FAILED;
	}

	memset(&buf, 0, sizeof(buf));
	FD_ZERO(&read_set);
	FD_SET(fd, &read_set);
	timeout.tv_sec = 0;
	timeout.tv_usec = MAX_SELECT_TIMEOUT;

	device_detach_driver(ASOC_DEVICE, ASOC_DRIVER);
	device_detach_driver("snd_aloop.0", "/sys/bus/platform/drivers/snd_aloop");
	device_detach_driver("rk817-codec", "/sys/bus/platform/drivers/rk817-codec");
	device_detach_driver(I2S_DEVICE, I2S_DRIVER);

__RETRY:
	ret = select(fd + 1, &read_set, NULL, NULL, &timeout);
	if (ret > 0) {
		memset(&buf, 0, sizeof(buf));
		read(fd, buf, sizeof(buf));
		buf[MAX_NL_BUF_SIZE - 1] = '\0';
		// printf("[%s()] msg: %s\n", __func__, buf);
		if (strncmp(buf, SOUND_UNBIND_DONE, strlen(SOUND_UNBIND_DONE)) == 0) {
			printf("[%s()] Unbind success: %s\n", __func__, buf);
			goto __SUCCESS;
		}
		goto __RETRY; // drop all message
	} else {
		printf("[%s()] select error %s\n", __func__, strerror(errno));
		goto __FAILED;
	}

__SUCCESS:
	close(fd);
	return 0;
__FAILED:
	close(fd);
	return 0;
}

#define EMMC_DEVICE "ffc50000.dwmmc"
#define EMMC_BIND_DONE "bind@/devices/platform/ffc50000.dwmmc"
#define EMMC_UNBIND_DONE "unbind@/devices/platform/ffc50000.dwmmc"
#define EMMC_DRIVER "/sys/bus/platform/drivers/dwmmc_rockchip"

int SAMPLE_COMM_AOV_BindEmmc() {
	int ret = 0;
	int fd = -1;
	char buf[MAX_NL_BUF_SIZE] = {'\0'};
	fd_set read_set;
	struct timeval timeout;
	struct sockaddr_nl addr;

	if (device_driver_is_bound(EMMC_DEVICE, EMMC_DRIVER)) {
		printf("[%s()] emmc device already bind!\n", __func__);
		return 0;
	}

	memset(&addr, 0, sizeof(addr));
	addr.nl_family = AF_NETLINK;
	addr.nl_groups = NETLINK_KOBJECT_UEVENT;
	addr.nl_pid = 0;

	fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_KOBJECT_UEVENT);
	if (fd < 0) {
		printf("[%s()] Failed to open emmc netlink because %s\n", __func__,
		       strerror(errno));
		return -1;
	} else if (bind(fd, (struct sockaddr *)(&addr), sizeof(addr)) != 0) {
		printf("[%s()] bind emmc netlink addr failed because %s\n", __func__,
		       strerror(errno));
		goto __FAILED;
	}

	FD_ZERO(&read_set);
	FD_SET(fd, &read_set);
	timeout.tv_sec = 0;
	timeout.tv_usec = MAX_SELECT_TIMEOUT;

	// bind emmc
	if (device_attach_driver(EMMC_DEVICE, EMMC_DRIVER) != 0)
		goto __FAILED;
	// wait for bind success
__RETRY:
	ret = select(fd + 1, &read_set, NULL, NULL, &timeout);
	if (ret > 0) {
		memset(&buf, 0, sizeof(buf));
		read(fd, buf, sizeof(buf));
		buf[MAX_NL_BUF_SIZE - 1] = '\0';
		// printf("[%s()] bind msg: %s\n", __func__, buf);
		if (strncmp(buf, EMMC_BIND_DONE, strlen(EMMC_BIND_DONE)) == 0) {
			printf("[%s()] Bind success: %s\n", __func__, buf);
			goto __SUCCESS;
		}
		goto __RETRY; // drop all message
	} else {
		printf("[%s()] select error %s\n", __func__, strerror(errno));
		goto __FAILED;
	}
__SUCCESS:
	close(fd);
	return 0;
__FAILED:
	close(fd);
	return -1;
}

int SAMPLE_COMM_AOV_UnbindEmmc() {
	int ret = 0;
	int fd = -1;
	char buf[MAX_NL_BUF_SIZE] = {'\0'};
	fd_set read_set;
	struct timeval timeout;
	struct sockaddr_nl addr;

	if (!device_driver_is_bound(EMMC_DEVICE, EMMC_DRIVER)) {
		printf("[%s()] emmc device already unbind!\n", __func__);
		return 0;
	}

	memset(&addr, 0, sizeof(addr));
	addr.nl_family = AF_NETLINK;
	addr.nl_groups = NETLINK_KOBJECT_UEVENT;
	addr.nl_pid = 0;

	fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_KOBJECT_UEVENT);
	if (fd < 0) {
		printf("[%s()] Failed to open emmc netlink because %s\n", __func__,
		       strerror(errno));
		return -1;
	} else if (bind(fd, (struct sockaddr *)(&addr), sizeof(addr)) != 0) {
		printf("[%s()] bind emmc netlink addr failed because %s\n", __func__,
		       strerror(errno));
		goto __FAILED;
	}

	memset(&buf, 0, sizeof(buf));
	FD_ZERO(&read_set);
	FD_SET(fd, &read_set);
	timeout.tv_sec = 0;
	timeout.tv_usec = MAX_SELECT_TIMEOUT;

	// unbind emmc
	if (device_detach_driver(EMMC_DEVICE, EMMC_DRIVER) != 0)
		goto __FAILED;
	// wait for unbind success
__RETRY:
	ret = select(fd + 1, &read_set, NULL, NULL, &timeout);
	if (ret > 0) {
		memset(&buf, 0, sizeof(buf));
		read(fd, buf, sizeof(buf));
		buf[MAX_NL_BUF_SIZE - 1] = '\0';
		// printf("[%s()] unbind msg: %s\n", __func__, buf);
		if (strcmp(buf, EMMC_UNBIND_DONE) == 0) {
			printf("[%s()] Unbind success: %s\n", __func__, buf);
			goto __SUCCESS;
		}
		goto __RETRY; // drop all message
	} else {
		printf("[%s()] select error %s\n", __func__, strerror(errno));
		goto __FAILED;
	}
__SUCCESS:
	close(fd);
	return 0;
__FAILED:
	close(fd);
	return -1;
}

int SAMPLE_COMM_AOV_DisableNonBootCPUs() {
	int ret;
	int fd;
	const char *off = "0";

	fd = open("/sys/devices/system/cpu/cpu1/online", O_WRONLY);
	if (fd >= 0) {
		ret = write(fd, off, strlen(off));
		if (ret >= 0)
			printf("disable cpu 1 success\n");
		else
			printf("disable cpu 1 failed because %s\n", strerror(errno));
		close(fd);
	}
	fd = open("/sys/devices/system/cpu/cpu2/online", O_WRONLY);
	if (fd >= 0) {
		ret = write(fd, off, strlen(off));
		if (ret >= 0)
			printf("disable cpu 2 success\n");
		else
			printf("disable cpu 2 failed because %s\n", strerror(errno));
		close(fd);
	}
	fd = open("/sys/devices/system/cpu/cpu3/online", O_WRONLY);
	if (fd >= 0) {
		ret = write(fd, off, strlen(off));
		if (ret >= 0)
			printf("disable cpu 3 success\n");
		else
			printf("disable cpu 3 failed because %s\n", strerror(errno));
		close(fd);
	}
	return 0;
}

int SAMPLE_COMM_AOV_EnableNonBootCPUs() {
	int fd;
	const char *on = "1";
	int ret;

	fd = open("/sys/devices/system/cpu/cpu1/online", O_WRONLY);
	if (fd >= 0) {
		ret = write(fd, on, strlen(on));
		if (ret > 0)
			printf("enable cpu 1 success\n");
		else
			printf("enable cpu 1 failed because %s\n", strerror(errno));
		close(fd);
	}
	fd = open("/sys/devices/system/cpu/cpu2/online", O_WRONLY);
	if (fd >= 0) {
		ret = write(fd, on, strlen(on));
		if (ret > 0)
			printf("enable cpu 2 success\n");
		else
			printf("enable cpu 2 failed because %s\n", strerror(errno));
		close(fd);
	}
	fd = open("/sys/devices/system/cpu/cpu3/online", O_WRONLY);
	if (fd >= 0) {
		ret = write(fd, on, strlen(on));
		if (ret > 0)
			printf("enable cpu 3 success\n");
		else
			printf("enable cpu 3 failed because %s\n", strerror(errno));
		close(fd);
	}
	return 0;
}

int SAMPLE_COMM_AOV_BindUsb() { return 0; }
int SAMPLE_COMM_AOV_UnbindUsb() { return 0; }

int SAMPLE_COMM_AOV_BindSDIO() { return 0; }
int SAMPLE_COMM_AOV_UnbindSDIO() { return 0; }

#endif