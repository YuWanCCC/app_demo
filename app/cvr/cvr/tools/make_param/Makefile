CC ?= gcc
CROSS_COMPILE ?=

TARGET = make_param
HOST_TARGET = make_param_host
OUT_PATH = out

CFLAGS = -O2 -Wall -I./include

ifeq ($(IS_NAND_FLASH), YES)
	CFLAGS += -DIS_NAND_FLASH
endif

target = make_param.c
target_bin = $(patsubst %.c,%,$(target))

ifneq ($(RK_TOOLCHAIN_CROSS),)
CROSS_COMPILE_CC ?= $(RK_TOOLCHAIN_CROSS)-gcc
else
CROSS_COMPILE_CC ?= arm-rockchip830-linux-gnueabihf-gcc
endif

all: $(TARGET) $(HOST_TARGET)

$(TARGET):
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -o $@ $(target)

$(HOST_TARGET):
	$(CC) $(CFLAGS) -o $@ $(target)

clean:
	@echo "Cleaning make_param directory..."
	@if [ -d "$(OUT_PATH)" ]; then \
		echo "Removing output directory: $(OUT_PATH)"; \
		rm -rf $(OUT_PATH); \
	fi
	@echo "Removing generated files..."
	@for file in $(TARGET) $(HOST_TARGET) param.img; do \
		if [ -f "$$file" ]; then \
			echo "  - Removing $$file"; \
			rm -f "$$file"; \
		fi \
	done
	@echo "Clean make_param completed"

.PHONY: all clean
