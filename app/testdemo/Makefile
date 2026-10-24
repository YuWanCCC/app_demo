ifeq ($(APP_PARAM), )
	APP_PARAM:=../Makefile.param
	include $(APP_PARAM)
endif

SHELL:=/bin/bash

CURRENT_DIR := $(shell pwd)

ifneq (, $(filter YOLOWORLD_DEMO, $(RK_APP_TYPE)))
PKG_TARGET := lvgl-build yoloworld_demo-build
endif

ifneq (, $(filter PROJECTOR_DEMO, $(RK_APP_TYPE)))
PKG_TARGET := projector_demo-build
endif

all: $(PKG_TARGET)
	@echo "build $(PKG_NAME) done"

lvgl-build:
	@test ! -d $(RK_APP_COMPONENT)/lvgl || make -C $(RK_APP_COMPONENT)/lvgl CONFIG_LVGL=y
	@echo "build lvgl done"

yoloworld_demo-build: lvgl-build
	@test ! -d $(CURRENT_DIR)/yoloworld_demo || make -C $(CURRENT_DIR)/yoloworld_demo

projector_demo-build:
	@test ! -d $(CURRENT_DIR)/low_delay_net_display || make -C $(CURRENT_DIR)/low_delay_net_display

clean:
	@test -d $(RK_APP_COMPONENT)/lvgl && make -C $(RK_APP_COMPONENT)/lvgl clean
	@test -d $(CURRENT_DIR)/yoloworld_demo && make -C $(CURRENT_DIR)/yoloworld_demo clean
	@test ! -d $(CURRENT_DIR)/low_delay_net_display || make -C $(CURRENT_DIR)/low_delay_net_display clean

distclean: clean

info:
	@echo -e "$(C_YELLOW)-------------------------------------------------------------------------$(C_NORMAL)"
	@echo -e "RK_APP_TYPE=$(RK_APP_TYPE)"
	@echo -e "$(CURRENT_DIR) option support as follow:"
	@echo -e "	YOLOWORLD_DEMO"
	@echo -e "	PROJECTOR_DEMO"
	@echo -e "$(C_YELLOW)-------------------------------------------------------------------------$(C_NORMAL)"
