ARCH := arm
CC := $(RK_APP_CROSS)-gcc
LD := $(RK_APP_CROSS)-gcc

BLUEZ_DIR := $(CURDIR)/../external/bluez-5.77

CFLAGS := -Os -pipe -fno-common -ffunction-sections -fdata-sections
CFLAGS += -Wall -Wextra -Wshadow
CFLAGS += -I $(BLUEZ_DIR) -I $(BLUEZ_DIR)/lib

LDFLAGS := -s -Wl,--gc-sections
LDFLAGS += $(BLUEZ_DIR)/lib/.libs/libbluetooth-internal.a
LDFLAGS += $(BLUEZ_DIR)/src/.libs/libshared-mainloop.a
LDFLAGS += -lpthread

# Server source files
SERVER_SOURCES = btgatt-server-main.c \
                btgatt-server-transfer.c \
                btgatt-server-hci.c \
                btgatt-server-mgmt.c

# Server header files
SERVER_HEADERS = btgatt-server.h \
          btgatt-server-transfer.h \
          btgatt-server-hci.h \
          btgatt-server-mgmt.h

# Client source files
CLIENT_SOURCES = btgatt-client.c

# Object files
SERVER_OBJECTS = $(SERVER_SOURCES:.c=.o)
CLIENT_OBJECTS = $(CLIENT_SOURCES:.c=.o)

# Targets
all: rkbt_gatt_server rkbt_gatt_client

rkbt_gatt_server: $(SERVER_OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

rkbt_gatt_client: $(CLIENT_OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

# Header dependencies
$(SERVER_OBJECTS): $(HEADERS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f rkbt_gatt_server rkbt_gatt_client $(SERVER_OBJECTS) $(CLIENT_OBJECTS)

.PHONY: all clean