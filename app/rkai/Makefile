ifeq ($(APP_PARAM), )
    APP_PARAM:=../Makefile.param
    include $(APP_PARAM)
endif

export LC_ALL=C
SHELL:=/bin/bash

CURRENT_DIR := $(shell pwd)

PKG_NAME := rkai
PKG_BIN ?= out
PKG_BUILD ?= build

# debug: build cmake with more message
ifneq ($(V),)
PKG_CONF_OPTS += -DCMAKE_VERBOSE_MAKEFILE=ON
PKG_DBG :=
else
PKG_DBG := @
endif
#
ifneq ($(RK_BUILD_VERSION_TYPE), DEBUG)
PKG_CONF_OPTS += -DCMAKE_BUILD_TYPE=Release
else
PKG_CONF_OPTS += -DCMAKE_BUILD_TYPE=Debug
endif

RK_APP_CFLAGS = -I $(RK_APP_MEDIA_INCLUDE_PATH)
RK_APP_LDFLAGS = -L $(RK_APP_MEDIA_LIBS_PATH) -L$(RK_APP_OUTPUT)/lib

RK_APP_OPTS += -Wl,-rpath-link,$(RK_APP_MEDIA_LIBS_PATH):$(RK_APP_PATH_LIB_INCLUDE)/root/usr/lib:$(RK_APP_OUTPUT)/lib
PKG_CONF_OPTS += -DCMAKE_C_FLAGS="$(RK_APP_CFLAGS) $(RK_APP_LDFLAGS) $(RK_APP_OPTS)" \
				-DCMAKE_CXX_FLAGS="$(RK_APP_CFLAGS) $(RK_APP_LDFLAGS) $(RK_APP_OPTS)"

PKG_TOOLCHAIN_SYSROOT := $(shell $(RK_APP_CROSS)-gcc -print-sysroot)
PKG_CONF_OPTS += -DCMAKE_FIND_ROOT_PATH="$(CURRENT_DIR)/$(PKG_BIN);$(RK_APP_OUTPUT);$(PKG_TOOLCHAIN_SYSROOT)"
PKG_CONF_OPTS += -DCMAKE_SYSROOT="$(PKG_TOOLCHAIN_SYSROOT)"

PKG_CONF_OPTS += -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER
PKG_CONF_OPTS += -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY
PKG_CONF_OPTS += -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY
PKG_CONF_OPTS += -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY
PKG_CONF_OPTS += -DCMAKE_NO_SYSTEM_FROM_IMPORTED=ON
PKG_CONF_OPTS += -DCMAKE_SYSTEM_NAME=Linux

PKG_CONF_OPTS += -DROCKCHIP_AI_CHIP=$(RK_APP_CHIP)

ifneq (, $(filter RK_APP_RKAI, $(RK_APP_TYPE)))
PKG_TARGET := rkai-build
endif

ifeq ($(PKG_BIN),)
$(error ### $(CURRENT_DIR): PKG_BIN is NULL, Please Check !!!)
endif

all: $(PKG_TARGET)
	$(call MAROC_COPY_PKG_TO_APP_OUTPUT, $(RK_APP_OUTPUT), $(PKG_BIN))
	@echo "build $(PKG_NAME) done"

rkai-build:
	$(PKG_DBG)test -f $(CURRENT_DIR)/$(PKG_BIN)/bin/RkLunch-RK_APP_RKAI.sh || ( \
	rm -rf $(PKG_BIN) $(PKG_BUILD); \
	mkdir -p $(PKG_BIN); \
	mkdir -p $(PKG_BUILD); \
	pushd $(PKG_BUILD); \
		rm -rf CMakeCache.txt; \
		cmake $(CURRENT_DIR)/$(PKG_NAME)/ \
		-DCMAKE_C_COMPILER=$(RK_APP_CROSS)-gcc \
		-DCMAKE_CXX_COMPILER=$(RK_APP_CROSS)-g++ \
		-DCMAKE_POSITION_INDEPENDENT_CODE=ON \
		-DCMAKE_INSTALL_PREFIX="$(CURRENT_DIR)/$(PKG_BIN)" \
		-DDEPENDMENT_ROCKIT=1 \
		$(PKG_CONF_OPTS) ;\
		make -j$(RK_APP_JOBS) || exit -1; \
		make install; \
	popd; \
	cp rkai/RkLunch-RK_APP_RKAI.sh $(CURRENT_DIR)/$(PKG_BIN)/bin -fv; \
	)

clean:
	@rm -rf $(PKG_BIN) $(PKG_BUILD);

distclean: clean

info:
	@echo -e "$(C_YELLOW)-------------------------------------------------------------------------$(C_NORMAL)"
	@echo -e "RK_APP_TYPE=$(RK_APP_TYPE)"
	@echo -e "$(C_YELLOW)-------------------------------------------------------------------------$(C_NORMAL)"
