ifeq ($(APP_PARAM), )
	APP_PARAM:=../../Makefile.param
	include $(APP_PARAM)
endif

export LC_ALL=C
SHELL:=/bin/bash

PKG_NAME := projector_demo
PKG_BIN ?= out

CURRENT_DIR := $(shell pwd)

RK_APP_CFLAGS = -I $(RK_APP_MEDIA_INCLUDE_PATH) \
				-I $(RK_APP_MEDIA_INCLUDE_PATH)/libdrm \
				-I $(CURRENT_DIR)/tools \
				-I $(CURRENT_DIR)/libs/include \
				-I $(CURRENT_DIR)/include

PROJECTOR_DEMO_LD_FLAGS += -Wl,-rpath-link,$(RK_APP_MEDIA_LIBS_PATH):$(RK_APP_PATH_LIB_INCLUDE)/root/usr/lib \
						-lpthread -L$(CURRENT_DIR)/libs -lrk_rtp \
						-L$(RK_APP_MEDIA_LIBS_PATH) -lrga -ldrm -L$(RK_APP_PATH_LIB_INCLUDE)/root/usr/lib -lasound -lrockchip_mpp -lrockit \
						-Wl,-Bdynamic

PROJECTOR_DEMO_SRC := $(CURRENT_DIR)/main_tx.cpp
PROJECTOR_DEMO_SRC += $(CURRENT_DIR)/src/process_tx.cpp
PROJECTOR_DEMO_SRC += $(CURRENT_DIR)/src/AdjustClkApi.cpp
PROJECTOR_DEMO_SRC += $(CURRENT_DIR)/tools/Timers.cpp
PROJECTOR_DEMO_SRC += $(CURRENT_DIR)/tools/rt_trace.cpp
PROJECTOR_DEMO_SRC += $(CURRENT_DIR)/tools/trace.cpp
PROJECTOR_DEMO_SRC += $(CURRENT_DIR)/tools/vbltest.cpp
PROJECTOR_DEMO_SRC += $(CURRENT_DIR)/src/mpi_venc.cpp
PROJECTOR_DEMO_SRC += $(CURRENT_DIR)/src/mpi_vo.cpp
PROJECTOR_DEMO_SRC += $(CURRENT_DIR)/src/mpi_vi.cpp
PROJECTOR_DEMO_SRC += $(CURRENT_DIR)/src/drm_api.cpp
PROJECTOR_DEMO_SRC += $(CURRENT_DIR)/netserver/tcp_client.cpp
PROJECTOR_DEMO_SRC += $(CURRENT_DIR)/netserver/tcp_server.cpp
PROJECTOR_DEMO_SRC += $(CURRENT_DIR)/logger/log.cpp

PKG_TARGET := projector_demo-build

ifeq ($(PKG_BIN),)
$(error ### $(CURRENT_DIR): PKG_BIN is NULL, Please Check !!!)
endif

all: $(PKG_TARGET)
	@echo "build $(PKG_NAME) done"


projector_demo-build:
	rm -rf $(PKG_BIN); \
	mkdir -p $(PKG_BIN)/bin;
	mkdir -p $(PKG_BIN)/lib;
	cp $(CURRENT_DIR)/src/RkLunch.sh $(PKG_BIN)/bin/
	cp $(CURRENT_DIR)/src/projector_daemon.sh  $(PKG_BIN)/bin/
	cp -rf $(CURRENT_DIR)/libs/lib* $(PKG_BIN)/lib/
	$(RK_APP_CROSS)-g++ -g $(PROJECTOR_DEMO_SRC) -o $(PKG_BIN)/bin/$(PKG_NAME) $(PROJECTOR_DEMO_LD_FLAGS) $(RK_APP_CFLAGS)
	$(call MAROC_COPY_PKG_TO_APP_OUTPUT, $(RK_APP_OUTPUT), $(PKG_BIN))

clean:
	@rm -rf $(PKG_BIN)

distclean: clean



