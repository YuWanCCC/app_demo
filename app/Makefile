
MAKEFILE_DIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
export LC_ALL=C
SHELL:=/bin/bash
MAKE=make
AT?=@

################################################################################
##   Platform Specified  Configuraton
################################################################################
include $(MAKEFILE_DIR)/Makefile.param

################################################################################
##   Public Configuraton
################################################################################

app_src := $(wildcard ./*/Makefile ./component/fastboot_server/Makefile ./component/ao_record_service/Makefile)
app_src := $(dir $(app_src))
app_src_clean := $(app_src) ./component/rkfsmk ./component/third_libs ./component/rkadk ./component/lvgl

all: $(pkg-build)
	$(foreach target,$(app_src),make -C $(target)||exit -1;)
	$(call MAROC_COPY_PKG_TO_APP_OUTPUT, $(RK_PROJECT_PATH_APP), $(RK_APP_OUTPUT))

distclean: clean

clean:
	for target in $(app_src_clean); do \
		echo "make -C $${target}"; \
		test -f $${target}/Makefile || continue; \
		make -C $${target} distclean || \
		( echo "########################################"; \
		echo "Error: $${target} distclean failed"; \
		echo "########################################" && exit -1) || exit -1; \
	done
	@rm -rf $(RK_APP_OUTPUT)

info:
	@$(foreach target,$(app_src),make -C $(target) info||exit 0;)
