ifeq ($(APP_PARAM), )
	APP_PARAM:=../../Makefile.param
	include $(APP_PARAM)
endif

export LC_ALL=C
SHELL:=/bin/bash

PKG_NAME := yoloworld_demo
PKG_BIN ?= out

CURRENT_DIR := $(shell pwd)

RK_APP_CFLAGS = -I $(RK_APP_MEDIA_INCLUDE_PATH) \
				-I $(RK_APP_MEDIA_INCLUDE_PATH)/libdrm \
				-I $(RK_APP_MEDIA_INCLUDE_PATH)/rkaiq \
				-I $(RK_APP_MEDIA_INCLUDE_PATH)/rkaiq/uAPI \
				-I $(RK_APP_MEDIA_INCLUDE_PATH)/rkaiq/uAPI2 \
				-I $(RK_APP_MEDIA_INCLUDE_PATH)/rkaiq/algos \
				-I $(RK_APP_MEDIA_INCLUDE_PATH)/rkaiq/common \
				-I $(RK_APP_MEDIA_INCLUDE_PATH)/rkaiq/xcore \
				-I $(RK_APP_MEDIA_INCLUDE_PATH)/rkaiq/iq_parser \
				-I $(RK_APP_MEDIA_INCLUDE_PATH)/rkaiq/iq_parser_v2 \
				-I $(RK_APP_MEDIA_INCLUDE_PATH)/freetype2 \
				-I $(RK_APP_OUTPUT)/include \
				-I $(RK_APP_OUTPUT)/include/lvgl \
				-I $(CURRENT_DIR)/include \
				-I $(CURRENT_DIR)/3rdparty/dma \
				-I $(CURRENT_DIR)/3rdparty/eigen-3.4.0 \
				-I $(CURRENT_DIR)/3rdparty/rknpu2/include \
				-I $(CURRENT_DIR)/3rdparty/cJSON \
				-I $(CURRENT_DIR)/include/yoloworld \
				-I $(CURRENT_DIR)/include/media \
				-I $(CURRENT_DIR)/include/ui \
				-I $(CURRENT_DIR)/src/yoloworld \
				-I $(CURRENT_DIR)/src/media \
				-I $(CURRENT_DIR)/src/ui/page \
				-I $(CURRENT_DIR)/src/ui/core \
				-I $(CURRENT_DIR)/src/ui/lvgl/common \
				-I $(CURRENT_DIR)/src/ui/lvgl/drivers \
				-I $(CURRENT_DIR)/src/ui/lvgl/porting \
				-I $(CURRENT_DIR)/src/ui/utils

# print-cflags-formatted:
# 	@echo "RK_APP_CFLAGS 展开后的路径:"
# 	@echo $(RK_APP_CFLAGS) | sed 's/-I/\n-I/g'

YOLOWORLD_APP_LD_FLAGS += $(RK_APP_CFLAGS) \
	-Wl,-rpath-link,$(RK_APP_MEDIA_LIBS_PATH):$(RK_APP_PATH_LIB_INCLUDE)/root/usr/lib:$(RK_APP_OUTPUT)/lib \
	-pthread -g -lstdc++ -lm \
	-L$(RK_APP_PATH_LIB_INCLUDE)/root/usr/lib -lasound \
	-L$(RK_APP_MEDIA_LIBS_PATH) -lrksysutils -lrockchip_mpp -lrockit -lz -lpng -lrtsp -lfreetype -ldrm -lrkaiq \
	-L$(RK_APP_OUTPUT)/lib -llvgl \
	-L$(CURRENT_DIR)/3rdparty/rknpu2/Linux/armhf -lrknnrt \
	-L$(CURRENT_DIR)/lib -ldeepsort -lyoloworld_detector \
	-L$(RK_APP_MEDIA_LIBS_PATH) -lrga \
	-Wl,-Bdynamic

YOLOWORLD_APP_SRC := $(CURRENT_DIR)/src/main.cpp
YOLOWORLD_APP_SRC += $(CURRENT_DIR)/3rdparty/cJSON/cJSON.c
YOLOWORLD_APP_SRC += $(CURRENT_DIR)/src/yoloworld/yoloworld_detect.cpp
YOLOWORLD_APP_SRC += $(CURRENT_DIR)/src/media/*.cpp
YOLOWORLD_APP_SRC += $(CURRENT_DIR)/src/media/*.c
YOLOWORLD_APP_SRC += $(CURRENT_DIR)/src/ui/page/*.c
YOLOWORLD_APP_SRC += $(CURRENT_DIR)/src/ui/core/*.c
YOLOWORLD_APP_SRC += $(CURRENT_DIR)/src/ui/lvgl/common/*.c
YOLOWORLD_APP_SRC += $(CURRENT_DIR)/src/ui/lvgl/drivers/*.c
YOLOWORLD_APP_SRC += $(CURRENT_DIR)/src/ui/lvgl/porting/*.c
YOLOWORLD_APP_SRC += $(CURRENT_DIR)/src/ui/utils/*.c

PKG_TARGET := yoloworld_app-build

ifeq ($(PKG_BIN),)
$(error ### $(CURRENT_DIR): PKG_BIN is NULL, Please Check !!!)
endif

all: $(PKG_TARGET)
	@echo "build $(PKG_NAME) done"

yoloworld_app-build:
	rm -rf $(PKG_BIN); \
	mkdir -p $(PKG_BIN)/bin;
	mkdir -p $(PKG_BIN)/lib;
	cp -rf $(CURRENT_DIR)/lib/* $(PKG_BIN)/lib/
	cp -rf $(CURRENT_DIR)/RkLunch.sh $(PKG_BIN)/bin/
	$(RK_APP_CROSS)-g++ -g -std=c++17 -DEIGEN_MAX_ALIGN_BYTES=16 -DEIGEN_DONT_VECTORIZE $(YOLOWORLD_APP_SRC) -o $(PKG_BIN)/bin/$(PKG_NAME) $(YOLOWORLD_APP_LD_FLAGS)
	$(call MAROC_COPY_PKG_TO_APP_OUTPUT, $(RK_APP_OUTPUT), $(PKG_BIN))
	mkdir -p $(RK_APP_OUTPUT)/share/models
	mkdir -p $(RK_APP_OUTPUT)/share/fonts
	mkdir -p $(RK_APP_OUTPUT)/share/ui
	cp -rf $(CURRENT_DIR)/resources/models/* $(RK_APP_OUTPUT)/share/models/
	cp -rf $(CURRENT_DIR)/resources/fonts/* $(RK_APP_OUTPUT)/share/fonts/
	cp -rf $(CURRENT_DIR)/resources/ui/* $(RK_APP_OUTPUT)/share/ui/

clean:
	@rm -rf $(PKG_BIN)
	@rm -rf $(RK_APP_OUTPUT)/share/models
	@rm -rf $(RK_APP_OUTPUT)/share/fonts
	@rm -rf $(RK_APP_OUTPUT)/share/ui

distclean: clean
