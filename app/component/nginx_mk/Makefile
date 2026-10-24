ifeq ($(APP_PARAM), )
    APP_PARAM:=../Makefile.param
    include $(APP_PARAM)
endif

export LC_ALL=C
SHELL:=/bin/bash

CURRENT_DIR := $(shell pwd)

# standalone mode: prebuilt toolkits (openssl/zlib/pcre2) location, overridable via env
RK_APP_TOOLKITS_DIR ?= $(CURRENT_DIR)/../../../sysdrv/tools/board/toolkits
RK_APP_PCRE2_TAR_DIR ?= $(CURRENT_DIR)/../../../sysdrv/tools/board/pcre2

PKG_NAME := nginx
ifeq ($(wildcard $(CURRENT_DIR)/$(PKG_NAME)),)
PKG_NAME := nginx-1.24.0
endif
PKG_BIN ?= out
PKG_BUILD ?= build
PKG_CFG := rockchip-conf
PKG_VER := nginx-1.24.0.tar.gz
PKG_PATCHES := nginx-patches/
PKG_TAR := https://nginx.org/download/$(PKG_VER)
NGINX_HTTP_FLV_LIVE_PKG_NAME:= nginx-http-flv-module-1.2.10
NGINX_HTTP_FLV_LIVE_PKG_VER := v1.2.10.tar.gz
NGINX_HTTP_FLV_LIVE_PKG_TAR := https://github.com/winshining/nginx-http-flv-module/archive/$(NGINX_HTTP_FLV_LIVE_PKG_VER)
NGINX_HTTP_FLV_LIVE_PKG_PATCHES := nginx-http-flv-module-patches/

PCRE2_PKG_NAME := pcre2-10.45
PCRE2_PKG_TAR := $(RK_APP_PCRE2_TAR_DIR)/$(PCRE2_PKG_NAME).tar.bz2

PKG_CFLAGS := $(RK_APP_OPTS) -Os -I $(RK_APP_OUTPUT)/include -I $(CURRENT_DIR)/$(PKG_BIN)/include

ifeq ($(CONFIG_NGINX),y)
PKG_TARGET := nginx-build
endif
ifeq ($(RK_ENABLE_NGINX),y)
PKG_TARGET := nginx-build
endif

ifeq ($(PKG_BIN),)
$(error ### $(CURRENT_DIR): PKG_BIN is NULL, Please Check !!!)
endif

all: $(PKG_TARGET)
	$(RKDBG)rm -f $(CURRENT_DIR)/$(PKG_BIN)/pkgconfig/libcrypto.pc
	$(RKDBG)rm -f $(CURRENT_DIR)/$(PKG_BIN)/lib/libcrypto.*
	$(RKDBG)rm -f $(CURRENT_DIR)/$(PKG_BIN)/lib/libz.*
	$(call MAROC_COPY_PKG_TO_APP_OUTPUT, $(RK_APP_OUTPUT), $(PKG_BIN))
	@echo "build $(PKG_NAME) done"

thirdlibs-build:
	$(RKDBG)mkdir -p $(CURRENT_DIR)/$(PKG_BIN)/
	$(RKDBG)test -f $(RK_APP_TOOLKITS_DIR)/openssl/out/lib/libcrypto.so || make -C $(RK_APP_TOOLKITS_DIR)/openssl
	$(RKDBG)test -f $(CURRENT_DIR)/$(PKG_BIN)/lib/libcrypto.so || ( \
		cp -rfa $(RK_APP_TOOLKITS_DIR)/openssl/out/* $(CURRENT_DIR)/$(PKG_BIN)/ )
	$(RKDBG)test -f $(CURRENT_DIR)/$(PKG_BIN)/lib/libz.so || ( \
		cp -rfa $(RK_APP_TOOLKITS_DIR)/zlib/out/* $(CURRENT_DIR)/$(PKG_BIN)/ )
	$(RKDBG)test -f $(CURRENT_DIR)/$(PCRE2_PKG_NAME).tar.bz2 || cp -fv $(PCRE2_PKG_TAR) $(CURRENT_DIR)/;
	$(RKDBG)rm -rf $(CURRENT_DIR)/$(PCRE2_PKG_NAME);
	$(RKDBG)tar xf $(CURRENT_DIR)/$(PCRE2_PKG_NAME).tar.bz2;
	$(RKDBG)test -f $(CURRENT_DIR)/$(PKG_BIN)/lib/libpcre2-8.a || ( \
		cd $(CURRENT_DIR)/$(PCRE2_PKG_NAME); \
		CC=$(RK_APP_CROSS)-gcc \
		./configure --prefix=$(CURRENT_DIR)/$(PKG_BIN) \
		--disable-shared --enable-static \
		--host $(RK_APP_CROSS) --target $(RK_APP_CROSS) || (echo "Configure $(PCRE2_PKG_NAME) failed" && exit 1); \
		make -j$(RK_APP_JOBS) || ( echo "Make $(PCRE2_PKG_NAME) failed" && exit 1); \
		make install || ( echo "Install $(PCRE2_PKG_NAME) failed" && exit 1); \
	)
	$(RKDBG)echo "build dependent libs done"

nginx-build:
	$(RKDBG)test -f $(CURRENT_DIR)/$(NGINX_HTTP_FLV_LIVE_PKG_VER) || wget $(NGINX_HTTP_FLV_LIVE_PKG_TAR)
	$(RKDBG)test -d $(CURRENT_DIR)/$(NGINX_HTTP_FLV_LIVE_PKG_NAME) || tar xf $(CURRENT_DIR)/$(NGINX_HTTP_FLV_LIVE_PKG_VER)
	$(RKDBG)test -f $(CURRENT_DIR)/$(PKG_VER) || wget $(PKG_TAR)
	$(RKDBG)test -d $(CURRENT_DIR)/$(PKG_NAME) || tar xf $(CURRENT_DIR)/$(PKG_VER)
	$(RKDBG)test ! -d $(CURRENT_DIR)/$(PKG_NAME) || \
		test -f $(CURRENT_DIR)/$(PKG_BIN)/usr/sbin/nginx || (\
	echo "Start to build $(PKG_NAME)"; \
	make thirdlibs-build -C $(CURRENT_DIR); \
	echo "RK_APP_CROSS is $(RK_APP_CROSS)"; \
	cp -af $(CURRENT_DIR)/$(PKG_PATCHES)/* $(CURRENT_DIR)/$(PKG_NAME)/; \
	cd $(CURRENT_DIR)/$(PKG_NAME) && bash patches.sh && cd -; \
	cp -af $(CURRENT_DIR)/$(NGINX_HTTP_FLV_LIVE_PKG_PATCHES)/* $(CURRENT_DIR)/$(NGINX_HTTP_FLV_LIVE_PKG_NAME)/; \
	cd $(CURRENT_DIR)/$(NGINX_HTTP_FLV_LIVE_PKG_NAME) && bash patches.sh && cd -; \
	mkdir -p $(CURRENT_DIR)/$(PKG_BIN)/etc/init.d; \
	pushd $(CURRENT_DIR)/$(PKG_NAME); \
		sed -i -e 's/-Werror//g' auto/cc/* ; \
		ngx_force_c_compiler=yes ngx_force_c99_have_variadic_macros=yes \
		ngx_force_gcc_have_variadic_macros=yes ngx_force_gcc_have_atomic=yes \
		ngx_force_have_epoll=yes ngx_force_have_sendfile=yes \
		ngx_force_have_sendfile64=yes ngx_force_have_pr_set_dumpable=yes \
		ngx_force_have_timer_event=yes ngx_force_have_map_anon=yes \
		ngx_force_have_map_devzero=yes ngx_force_have_sysvshm=yes \
		ngx_force_have_posix_sem=yes ngx_force_have_libatomic=no \
		ngx_feature_run=no \
		./configure --crossbuild=Linux::"arm" \
			--with-cc="$(RK_APP_CROSS)-gcc" \
			--with-cpp="$(RK_APP_CROSS)-g++" \
			--prefix=/usr/ \
			--conf-path=/etc/nginx/nginx.conf \
			--pid-path=/var/run/nginx.pid \
			--lock-path=/var/run/lock/nginx.lock --user=www-data --group=www-data \
			--error-log-path=/var/log/nginx/error.log \
			--http-log-path=/var/log/nginx/access.log \
			--http-client-body-temp-path=/var/tmp/nginx/client-body \
			--http-proxy-temp-path=/var/tmp/nginx/proxy \
			--http-fastcgi-temp-path=/var/tmp/nginx/fastcgi --with-pcre \
			--without-select_module --without-poll_module --without-http-cache \
			--without-http_ssi_module --without-http_referer_module \
			--without-http_upstream_least_conn_module \
			--without-http_upstream_keepalive_module \
			--without-http_upstream_ip_hash_module \
			--without-http_upstream_hash_module --without-http_split_clients_module \
			--without-http_memcached_module --without-http_map_module \
			--without-http_limit_req_module --without-http_limit_conn_module \
			--without-http_geo_module --without-http_empty_gif_module \
			--without-http_auth_basic_module --without-http_charset_module \
			--without-http_autoindex_module --without-http_access_module \
			--without-http_browser_module --without-http_uwsgi_module \
			--without-http_scgi_module --without-http_upstream_zone_module \
			--with-http_gzip_static_module  \
			--add-module=$(CURRENT_DIR)/$(NGINX_HTTP_FLV_LIVE_PKG_NAME) \
			--with-cc-opt="$(PKG_CFLAGS)" \
			--with-ld-opt="-L $(CURRENT_DIR)/$(PKG_BIN)/lib -lz"; \
		make -j$(RK_APP_JOBS) || (echo "Make $(PKG_NAME) failed" && exit 1); \
		make install DESTDIR=$(CURRENT_DIR)/$(PKG_BIN) || (echo "Install $(PKG_NAME) failed" && exit 2); \
		rm -rf $(CURRENT_DIR)/$(PKG_BIN)/{bin,share,ssl,lib/engines-1.1}; \
		cp $(CURRENT_DIR)/S50nginx $(CURRENT_DIR)/$(PKG_BIN)/etc/init.d/ -fav; \
		rm -rf $(CURRENT_DIR)/$(PKG_BIN)/usr/html; \
		rm -rf $(CURRENT_DIR)/$(PKG_BIN)/etc/nginx; \
	popd; \
	) || exit 1;
	$(call MAROC_PKG_CHECK_PREBUILT, $(CURRENT_DIR),$(PKG_NAME),$(PKG_BIN))

clean:
	$(RKDBG)rm -rf $(PKG_BIN) $(PKG_BUILD);
	$(RKDBG)rm -rf $(CURRENT_DIR)/$(PKG_NAME)
	$(RKDBG)rm -rf $(CURRENT_DIR)/$(PCRE2_PKG_NAME)
	$(RKDBG)rm -rf $(CURRENT_DIR)/$(NGINX_HTTP_FLV_LIVE_PKG_NAME)

distclean: clean

info:
	@echo ""
################################################################################
