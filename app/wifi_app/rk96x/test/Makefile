ARCH := arm
CC := $(RK_APP_CROSS)-gcc
LD := $(RK_APP_CROSS)-gcc

TARGETS := kp_demo ble_netconf_demo

CFLAGS := -Os -pipe -fpic -fno-common -ffunction-sections -fdata-sections
CFLAGS += -D_GNU_SOURCE -D_FORTIFY_SOURCE=2
CFLAGS += -pthread -Wall -Wextra -Wshadow
LDFLAGS := -lpthread -Wl,--gc-sections

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

all: $(TARGETS) test_tools

kp_demo: keepalive_demo.o
	$(LD) -o $@ $^ $(LDFLAGS)

ble_netconf_demo: ble_netconf_demo.o
	$(LD) -o $@ $^ $(LDFLAGS)

.PHONY: test_tools
test_tools:
	ARCH=$(ARCH) CC=$(CC) LD=$(LD) make -C rk_kp
	ARCH=$(ARCH) CC=$(CC) LD=$(LD) make -C rk_net

clean:
	@rm -rf $(TARGETS) $(RKWIFI_SERVER_OBJS) $(BTMCU_DAEMON_OBJS)
	@rm -f *.o
	make -C rk_kp clean
	make -C rk_net clean
