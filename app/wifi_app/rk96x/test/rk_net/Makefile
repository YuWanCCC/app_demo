ARCH := arm
CC := $(RK_APP_CROSS)-gcc
LD := $(RK_APP_CROSS)-gcc

PKG_TARGET := rknet

all: rknet_bin
	@echo "build rk net done"

$(warning "wifi chip" $(RK_ENABLE_WIFI_CHIP))

#rknet
OBJS += rknet.o rknet_common.o rknet_tcp_server.o rknet_tcp_client.o rknet_udp_server.o rknet_udp_client.o


CFLAGS := -Wall -g
CFLAGS += -lpthread -L ../out/lib/ -I include/ -I ./
#CFLAGS += -funsigned-char -freg-struct-return -fno-strict-aliasing -Wtrampolines -Wformat=2 -Wshadow -Wall -fpic -fpie -fno-common -Wfloat-equal -Wextra -fvisibility=hidden
CFLAGS += -funsigned-char -freg-struct-return -fno-strict-aliasing -Wtrampolines -Wformat=2 -Wshadow -Wall -fpic -fno-common -Wfloat-equal -Wextra
CFLAGS += -D_FORTIFY_SOURCE=2 -O2 -pipe
#规范建议：在Debug版本中实施，Release版本不实施
#CFLAGS += -ftrapv
#CFLAGS += -Werror

rknet_bin: $(OBJS)
	$(LD) -o $(PKG_TARGET) $(OBJS) $(CFLAGS)
	$(RK_APP_CROSS)-strip -s $(PKG_TARGET);
	chmod +x $(PKG_TARGET);
#	@cp -fa $(PKG_TARGET) ../$(PKG_BIN_INSTALL)/usr/bin;

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(PKG_TARGET)
