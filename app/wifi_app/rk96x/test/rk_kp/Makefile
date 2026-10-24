ARCH := arm
CC := $(RK_APP_CROSS)-gcc
LD := $(RK_APP_CROSS)-gcc

PKG_TARGET := rkkp

all: kp_bin
	@echo "build rk kp done"

$(warning "wifi chip" $(RK_ENABLE_WIFI_CHIP))

#rkcp_client
OBJS += rkkp.o

CFLAGS := -Wall -g
CFLAGS += -L ../../ -L ../../../out/lib/ -lrk96x_sdk -lwpa_client -lpthread -I ../../include/ -I ./
CFLAGS += -funsigned-char -freg-struct-return -fno-strict-aliasing -Wtrampolines -Wformat=2 -Wshadow -Wall -fpic -fno-common -Wfloat-equal -Wextra
CFLAGS += -D_FORTIFY_SOURCE=2 -O2 -pipe
#规范建议：在Debug版本中实施，Release版本不实施
#CFLAGS += -ftrapv
#CFLAGS += -Werror

kp_bin: $(OBJS)
	$(LD) -o $(PKG_TARGET) $(OBJS) $(CFLAGS)
	$(RK_APP_CROSS)-strip -s $(PKG_TARGET);
	chmod +x $(PKG_TARGET);
#	@cp -fa $(PKG_TARGET) ../$(PKG_BIN_INSTALL)/usr/bin;

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(PKG_TARGET)
