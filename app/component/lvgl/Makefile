ifeq ($(APP_PARAM), )
    APP_PARAM:=../../Makefile.param
    include $(APP_PARAM)
endif

export LC_ALL=C
SHELL:=/bin/bash

CURRENT_DIR := $(shell pwd)

PKG_NAME := lvgl
ifeq ($(wildcard $(CURRENT_DIR)/$(PKG_NAME)),)
PKG_NAME := lvgl-8.4.0
endif
PKG_BIN ?= out
PKG_BUILD ?= build
PKG_CFG := rockchip-conf
PKG_VER := v8.4.0.tar.gz
PKG_PATCHES := patches.sh
PKG_PATCHES_DIR := lvgl-patches
PKG_TAR := https://github.com/lvgl/lvgl/archive/refs/tags/$(PKG_VER)

LVGL_DEF_CONF := $(CURRENT_DIR)/lv_conf_default.h
LVGL_DST_CONF := $(CURRENT_DIR)/$(PKG_NAME)/lv_conf.h

ifneq (, $(filter RKCVR_RV1106, $(RK_APP_TYPE)))
LVGL_SRC_CONF := $(CURRENT_DIR)/$(PKG_CFG)/lv_conf_rv1106_cvr.h
else ifneq (, $(filter RKCVR_RV1103B, $(RK_APP_TYPE)))
LVGL_SRC_CONF := $(CURRENT_DIR)/$(PKG_CFG)/lv_conf_rv1103b_cvr.h
else ifneq (, $(filter FACIAL_GATE_RV1106_NORMAL, $(RK_APP_TYPE)))
LVGL_SRC_CONF := $(CURRENT_DIR)/$(PKG_CFG)/lv_conf_facialgate.h
else ifneq (, $(filter RKCVR_RV1126, $(RK_APP_TYPE)))
LVGL_SRC_CONF := $(CURRENT_DIR)/$(PKG_CFG)/lv_conf_rv1126_cvr.h
else ifneq (, $(filter RKCVR_RV1126B, $(RK_APP_TYPE)))
LVGL_SRC_CONF := $(CURRENT_DIR)/$(PKG_CFG)/lv_conf_rv1126b_cvr.h
else ifneq (, $(filter RKIPC_RV1126B_DV, $(RK_APP_TYPE)))
LVGL_SRC_CONF := $(CURRENT_DIR)/$(PKG_CFG)/lv_conf_rv1126b_dv.h
else ifneq (, $(filter YOLOWORLD_DEMO, $(RK_APP_TYPE)))
LVGL_SRC_CONF := $(CURRENT_DIR)/$(PKG_CFG)/lv_conf_rv1126b_yoloworld.h
else
LVGL_SRC_CONF := $(LVGL_DEF_CONF)
endif

RK_APP_CFLAGS = -I $(RK_APP_MEDIA_INCLUDE_PATH)/freetype2

PKG_CONF_OPTS += -DCMAKE_C_FLAGS="$(RK_APP_CFLAGS)" \
				-DCMAKE_CXX_FLAGS="$(RK_APP_CFLAGS)"

ifeq ($(CONFIG_LVGL),y)
    PKG_TARGET := lvgl-build
else ifeq ($(RK_ENABLE_LVGL),y)
    PKG_TARGET := lvgl-build
endif

ifeq ($(PKG_BIN),)
$(error ### $(CURRENT_DIR): PKG_BIN is NULL, Please Check !!!)
endif

all: $(PKG_TARGET)
	$(call MAROC_COPY_PKG_TO_APP_OUTPUT, $(RK_APP_OUTPUT), $(PKG_BIN))
	@echo "build $(PKG_NAME) done"

lvgl-build:
	@test -f $(LVGL_DST_CONF) || (\
	if [ ! -f "$(CURRENT_DIR)/$(PKG_VER)" ]; then \
		wget $(PKG_TAR) -O $(CURRENT_DIR)/$(PKG_VER) || exit -1; \
	fi; \
	if [ ! -d "$(CURRENT_DIR)/$(PKG_NAME)" ]; then \
		tar xf $(CURRENT_DIR)/$(PKG_VER) -C $(CURRENT_DIR) || exit -1; \
	fi; \
	)
	@test -f $(CURRENT_DIR)/$(PKG_BIN)/lib/liblvgl.a || (\
	echo "Start to build $(PKG_NAME)"; \
	if [ ! -f "$(LVGL_DEF_CONF)" ]; then \
		cp -af $(CURRENT_DIR)/$(PKG_PATCHES_DIR)/* $(CURRENT_DIR)/$(PKG_NAME); \
		pushd $(CURRENT_DIR)/$(PKG_NAME); \
		bash $(PKG_PATCHES) .; \
		popd; \
	fi; \
	if [ -n $(LVGL_SRC_CONF) ] && [ -f $(LVGL_SRC_CONF) ] ; then \
		cp $(LVGL_SRC_CONF) $(LVGL_DST_CONF); \
	fi; \
	rm -rf $(PKG_BIN) $(PKG_BUILD); \
	mkdir -p $(CURRENT_DIR)/$(PKG_BUILD); \
	pushd $(CURRENT_DIR)/$(PKG_BUILD); \
		rm -f CMakeCache.txt; \
		cmake $(CURRENT_DIR)/$(PKG_NAME) \
			-DCMAKE_C_COMPILER=$(RK_APP_CROSS)-gcc \
			-DCMAKE_CXX_COMPILER=$(RK_APP_CROSS)-g++ \
			-DCMAKE_INSTALL_PREFIX="$(CURRENT_DIR)/$(PKG_BIN)" \
			-DCMAKE_BUILD_TYPE=Release \
			-DCMAKE_COLOR_MAKEFILE=OFF \
			-DBUILD_DOC=OFF \
			-DBUILD_DOCS=OFF \
			-DBUILD_EXAMPLE=OFF \
			-DBUILD_EXAMPLES=OFF \
			-DBUILD_TEST=OFF \
			-DBUILD_TESTS=OFF \
			-DBUILD_TESTING=OFF \
			-DBUILD_SHARED_LIBS=OFF \
			$(PKG_CONF_OPTS) \
			-DCMAKE_BUILD_TARGET=cmake_linux \
			-DARCH="$(RK_APP_ARCH_TYPE)" ;\
			make -j$(RK_JOBS) || exit -1; \
			make install; \
	popd; \
	) || exit 1;

	$(call MAROC_PKG_CHECK_PREBUILT, $(CURRENT_DIR),$(PKG_NAME),$(PKG_BIN))

clean:
	@rm -rf $(PKG_BIN) $(PKG_BUILD);

distclean: clean
