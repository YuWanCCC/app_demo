ARCH := arm
CC := $(RK_APP_CROSS)-gcc
LD := $(RK_APP_CROSS)-gcc

TARGETS := librk96x_sdk.so rkwifi_server rkwifi_kpalive btmcu_daemon

SDK_OBJS := \
	lib/rk_wifi.o \
	lib/rk_wifi_hal.o \
	lib/rk_wifi_kpalive.o \
	utility/utility.o \
	utility/encode/gbk_to_utf8.o \
	utility/encode/utf8_to_gbk.o \
	utility/encode/rk_encode.o

RKWIFI_SERVER_OBJS := \
	src/rk_wifi_server.o \
	src/rk_wifi_softap.o

RKWIFI_KPALIVE_OBJS := \
	lib/rk_wifi.o \
	lib/rk_wifi_kpalive.o \
	src/rk_wifi_kpmain.o \
	utility/utility.o

BTMCU_DAEMON_OBJS := \
	src/rk_btmcu_daemon.o \
	utility/uart/uart.o \
	utility/utility.o

CFLAGS := -Os -pipe -fpic -fno-common -ffunction-sections -fdata-sections
CFLAGS += -D_GNU_SOURCE -D_FORTIFY_SOURCE=2 -DWPA_WIFI
#CFLAGS +=  -D_DEBUG_STACK
CFLAGS += -pthread -Wall -Wextra -Wshadow
CFLAGS += -I ../$(PKG_NAME_WPA_TOOLS)/src/common -I include/

LDFLAGS :=  -lpthread -Wl,--gc-sections
LIB_WPA_CLIENT := -L ../out/lib -lwpa_client
LIB_RK96X_SDK := -L ./ -lrk96x_sdk

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

all: $(TARGETS) test

librk96x_sdk.so: $(SDK_OBJS)
	$(LD) -o $@ $^ -shared $(LDFLAGS) $(LIB_WPA_CLIENT)

rkwifi_server: $(RKWIFI_SERVER_OBJS)
	$(LD) -o $@ $^ $(LDFLAGS) $(LIB_RK96X_SDK) $(LIB_WPA_CLIENT)

rkwifi_kpalive: $(RKWIFI_KPALIVE_OBJS)
	$(LD) -o $@ $^ $(LDFLAGS)

btmcu_daemon: $(BTMCU_DAEMON_OBJS)
	$(LD) -o $@ $^ $(LDFLAGS)

.PHONY: test
test: $(TARGETS)
	make -C test

clean:
	@rm -rf $(TARGETS) $(SDK_OBJS) $(RKWIFI_SERVER_OBJS) $(RKWIFI_KPALIVE_OBJS) $(BTMCU_DAEMON_OBJS)
	make -C test clean
