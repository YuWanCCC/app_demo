ifeq ($(APP_PARAM), )
    APP_PARAM := ../Makefile.param
    include $(APP_PARAM)
endif

export LC_ALL=C
SHELL:=/bin/bash
# default build adbd with static link.
# PKG_WPA_RTK_BUILD_STATIC ?= -static -pthread
# arch=arm default use thumb-2 compile
ifeq ($(RK_APP_ARCH_TYPE),arm)
PKG_USE_THUMB2 ?= YES
endif

CURRENT_DIR := $(shell pwd)
PKG_TARBALL_WPA_RTK_TOOLS := wpa_supplicant_hostapd-0.8_rtw-2-ga8ef7c824.20200911.tar.gz
PKG_NAME_WPA_RTK_TOOLS:= wpa_supplicant_hostapd-0.8_rtw-2-ga8ef7c824.20200911
ifeq ($(RK_ENABLE_WIFI_WPA3),y)
PKG_TARBALL_WPA_SUPPLICANT := wpa_supplicant-2.10.20241118.tar.bz2
PKG_TARBALL_HOSTAPD := hostapd-2.10.20241118.tar.bz2
PKG_TARBALL_OPENSSL := libopenssl-3.2.1.20241118.tar.bz2
PKG_NAME_WPA_TOOLS := wpa_supplicant-2.10
PKG_NAME_HOSTAPD_TOOLS := hostapd-2.10
PKG_NAME_OPENSSL := libopenssl-3.2.1
else
PKG_TARBALL_WPA_SUPPLICANT := wpa_supplicant-2.6.tar.bz2
PKG_TARBALL_HOSTAPD := hostapd-2.6.tar.bz2
PKG_NAME_WPA_TOOLS := wpa_supplicant-2.6
PKG_NAME_HOSTAPD_TOOLS := hostapd-2.6
endif

export PKG_CONFIG_PATH := $(CURRENT_DIR)/out/lib/pkgconfig

PKG_TARBALL_LIBNL := libnl-3.9.0.tar.gz
PKG_NAME_LIBNL := libnl-3.9.0
ifeq ($(RK_APP_ARCH_TYPE),arm)
PKG_OS_COMPILE_FLAG_OPENSSL := linux-armv4
else
PKG_OS_COMPILE_FLAG_OPENSSL := linux-aarch64
endif

PKG_BIN := out
PKG_BIN_INSTALL_DIR := install_out
PKG_BIN_INSTALL := $(PKG_BIN_INSTALL_DIR)/root

# Conf dir must be writeble
RK96X_CONF_PATH := /userdata/rk96x/conf

ifeq ($(strip $(RK_ENABLE_WIFI_CHIP)),RK960)
BIN_PREFIX:=rk960
else
BIN_PREFIX:=rk962
endif

PKG_CONF_OPTS += $(RK_APP_OPTS) -Os \
				 -I $(CURRENT_DIR)/$(PKG_BIN)/include/libnl3 \
				 -I $(CURRENT_DIR)/$(PKG_BIN)/include \
				 -L $(CURRENT_DIR)/$(PKG_BIN)/lib \
				 -l nl-3 \
				 -l nl-genl-3
ifeq ($(RK_APP_CHIP),rv1126)
PKG_CONF_OPTS += -lpthread
endif
ifeq ($(PKG_USE_THUMB2),YES)
PKG_CONF_OPTS += -mthumb -Wa,-mimplicit-it=thumb -mthumb-interwork
endif

ifeq ($(RK_ENABLE_WIFI),y)
PKG_TARGET += build-wifi
ifeq ($(RK_ENABLE_BLUETOOTH),y)
PKG_TARGET += build-bt
endif
else
ifneq (, $(filter RK_WIFI_APP,$(RK_APP_TYPE)))
PKG_TARGET += build-wifi
ifeq ($(RK_ENABLE_BLUETOOTH),y)
PKG_TARGET += build-bt
endif
endif
endif

all: $(PKG_TARGET)
	@echo "build wifi app done"
	$(call MAROC_COPY_PKG_TO_APP_OUTPUT, $(RK_APP_OUTPUT), $(PKG_BIN_INSTALL_DIR))

build-wifi: libnl
ifeq ($(RK_ENABLE_WIFI_WPA3),y)
	@test -f $(CURRENT_DIR)/$(PKG_BIN)/bin/wpa_supplicant || (\
	mkdir -p $(CURRENT_DIR)/$(PKG_NAME_WPA_TOOLS)/$(PKG_BIN); \
	tar -xf $(PKG_TARBALL_WPA_SUPPLICANT); \
	cp -f $(CURRENT_DIR)/$(PKG_NAME_WPA_TOOLS)/wpa_supplicant/defconfig_rk_dbg $(CURRENT_DIR)/$(PKG_NAME_WPA_TOOLS)/wpa_supplicant/.config ; \
	pushd $(CURRENT_DIR)/$(PKG_NAME_WPA_TOOLS)/; \
		CC="$(RK_APP_CROSS)-gcc" GCC="$(RK_APP_CROSS)-gcc" CXX="$(RK_APP_CROSS)-g++" CPP="$(RK_APP_CROSS)-cpp" \
		CFLAGS="$(PKG_CONF_OPTS)" CPPFLAGS="$(PKG_CONF_OPTS)" \
		CXXFLAGS="$(PKG_CONF_OPTS)" LDFLAGS="$(PKG_CONF_OPTS) $(PKG_WPA_RTK_BUILD_STATIC)" \
		make -j$(RK_APP_JOBS) -C $(CURRENT_DIR)/$(PKG_NAME_WPA_TOOLS)/wpa_supplicant/ ;\
		cp -f $(CURRENT_DIR)/$(PKG_NAME_WPA_TOOLS)/wpa_supplicant/wpa_supplicant $(CURRENT_DIR)/$(PKG_BIN)/bin/ ; \
		cp -f $(CURRENT_DIR)/$(PKG_NAME_WPA_TOOLS)/wpa_supplicant/wpa_cli $(CURRENT_DIR)/$(PKG_BIN)/bin/ ; \
		cp -f $(CURRENT_DIR)/$(PKG_NAME_WPA_TOOLS)/wpa_supplicant/libwpa_client.so $(CURRENT_DIR)/$(PKG_BIN)/lib/ ; \
	popd; )
else
	@test -f $(CURRENT_DIR)/$(PKG_BIN)/bin/wpa_supplicant || (\
	mkdir -p $(CURRENT_DIR)/$(PKG_NAME_WPA_TOOLS)/$(PKG_BIN); \
	tar -xf $(PKG_TARBALL_WPA_SUPPLICANT); \
	cp -f $(CURRENT_DIR)/$(PKG_NAME_WPA_TOOLS)/wpa_supplicant/defconfig_rk_dbg $(CURRENT_DIR)/$(PKG_NAME_WPA_TOOLS)/wpa_supplicant/.config ; \
	cp -f $(CURRENT_DIR)/patch/defconfig-2.6 $(CURRENT_DIR)/$(PKG_NAME_WPA_TOOLS)/wpa_supplicant/.config ; \
	cp -f $(CURRENT_DIR)/patch/wpa_supplicant_patch/* $(CURRENT_DIR)/$(PKG_NAME_WPA_TOOLS)/ ; \
	pushd $(CURRENT_DIR)/$(PKG_NAME_WPA_TOOLS)/; \
		/bin/bash wpa_supplicant_patch.sh ; \
		CC="$(RK_APP_CROSS)-gcc" GCC="$(RK_APP_CROSS)-gcc" CXX="$(RK_APP_CROSS)-g++" CPP="$(RK_APP_CROSS)-cpp" \
		CFLAGS="$(PKG_CONF_OPTS)" CPPFLAGS="$(PKG_CONF_OPTS)" \
		CXXFLAGS="$(PKG_CONF_OPTS)" LDFLAGS="$(PKG_CONF_OPTS) $(PKG_WPA_RTK_BUILD_STATIC)" \
		make -j$(RK_APP_JOBS) -C $(CURRENT_DIR)/$(PKG_NAME_WPA_TOOLS)/wpa_supplicant/ ;\
		cp -f $(CURRENT_DIR)/$(PKG_NAME_WPA_TOOLS)/wpa_supplicant/wpa_supplicant $(CURRENT_DIR)/$(PKG_BIN)/bin/ ; \
		cp -f $(CURRENT_DIR)/$(PKG_NAME_WPA_TOOLS)/wpa_supplicant/wpa_cli $(CURRENT_DIR)/$(PKG_BIN)/bin/ ; \
		cp -f $(CURRENT_DIR)/$(PKG_NAME_WPA_TOOLS)/wpa_supplicant/libwpa_client.so $(CURRENT_DIR)/$(PKG_BIN)/lib/ ; \
	popd; )
endif #RK_ENABLE_WIFI_WPA3

	#hostapd
	@test -f $(CURRENT_DIR)/$(PKG_BIN)/bin/hostapd || (\
	mkdir -p $(CURRENT_DIR)/$(PKG_NAME_HOSTAPD_TOOLS)/$(PKG_BIN); \
	tar -xf $(PKG_TARBALL_HOSTAPD); \
	cp -f $(CURRENT_DIR)/$(PKG_NAME_HOSTAPD_TOOLS)/hostapd/defconfig_rk $(CURRENT_DIR)/$(PKG_NAME_HOSTAPD_TOOLS)/hostapd/.config ; \
	pushd $(CURRENT_DIR)/$(PKG_NAME_HOSTAPD_TOOLS)/; \
		CC="$(RK_APP_CROSS)-gcc" GCC="$(RK_APP_CROSS)-gcc" CXX="$(RK_APP_CROSS)-g++" CPP="$(RK_APP_CROSS)-cpp" \
		CFLAGS="$(PKG_CONF_OPTS)" CPPFLAGS="$(PKG_CONF_OPTS)" \
		CXXFLAGS="$(PKG_CONF_OPTS)" LDFLAGS="$(PKG_CONF_OPTS) $(PKG_WPA_RTK_BUILD_STATIC)" \
		make -j$(RK_APP_JOBS) -C $(CURRENT_DIR)/$(PKG_NAME_HOSTAPD_TOOLS)/hostapd/ ;\
		cp -f $(CURRENT_DIR)/$(PKG_NAME_HOSTAPD_TOOLS)/hostapd/hostapd_cli $(CURRENT_DIR)/$(PKG_BIN)/bin/ ; \
		cp -f $(CURRENT_DIR)/$(PKG_NAME_HOSTAPD_TOOLS)/hostapd/hostapd $(CURRENT_DIR)/$(PKG_BIN)/bin/ ; \
	popd; )

	#dir
	@mkdir -p $(CURRENT_DIR)/$(PKG_BIN_INSTALL) \
		$(CURRENT_DIR)/$(PKG_BIN_INSTALL)/etc \
		$(CURRENT_DIR)/$(PKG_BIN_INSTALL)/usr/bin \
		$(CURRENT_DIR)/$(PKG_BIN_INSTALL)/usr/lib;

#build 962 SDK
	@mkdir -p $(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/bin;

	#NOTE: build hciattach because RK962 needs btmcu_daemon event if bluetooth disabled
ifneq ($(filter RK960 RK962,$(RK_ENABLE_WIFI_CHIP)),)
	make -C $(CURRENT_DIR)/bt/rk_hciattach/ clean
	@CC="$(RK_APP_CROSS)-gcc" GCC="$(RK_APP_CROSS)-gcc" \
	CXX="$(RK_APP_CROSS)-g++" CPP="$(RK_APP_CROSS)-cpp" \
	make -C $(CURRENT_DIR)/bt/rk_hciattach/

	@cp -f $(CURRENT_DIR)/bt/rk_hciattach/rk_hciattach $(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/bin/
	@chmod +x $(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/bin/rk_hciattach
endif

ifneq ($(filter RK962,$(RK_ENABLE_WIFI_CHIP)),)
	# Make lib and app

	make -C $(CURRENT_DIR)/rk96x/ clean;
	@CC="$(RK_APP_CROSS)-gcc" GCC="$(RK_APP_CROSS)-gcc" \
	CXX="$(RK_APP_CROSS)-g++" CPP="$(RK_APP_CROSS)-cpp" \
	PKG_NAME_WPA_TOOLS=$(PKG_NAME_WPA_TOOLS) \
	make -C $(CURRENT_DIR)/rk96x/;

	# Install lib
	# rkwifibt_server

	@mkdir -p $(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/lib

	@cp -f $(CURRENT_DIR)/rk96x/librk96x_sdk.so $(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/lib/

	# Install app

	#rkwifi_server
	@cp -f $(CURRENT_DIR)/rk96x/rkwifi_server $(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/bin/
	@chmod +x $(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/bin/rkwifi_server

	@cp -f $(CURRENT_DIR)/rk96x/rkwifi_kpalive $(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/bin/
	@chmod +x $(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/bin/rkwifi_kpalive
	@cp -f $(CURRENT_DIR)/rk96x/btmcu_daemon $(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/bin/
	@chmod +x $(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/bin/btmcu_daemon

	@cp -f $(CURRENT_DIR)/rk96x/test/kp_demo $(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/bin/
	@chmod +x $(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/bin/kp_demo

	@cp -f $(CURRENT_DIR)/rk96x/test/ble_netconf_demo $(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/bin/
	@chmod +x $(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/bin/ble_netconf_demo

	# install conf
	@rm -fr $(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/etc/rk96x/conf
	@mkdir -p $(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/etc/rk96x/conf

	@cp -f $(CURRENT_DIR)/conf/wpa_supplicant.conf \
		$(CURRENT_DIR)/conf/wifi_user.conf \
		$(CURRENT_DIR)/conf/bt_user.conf \
		$(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/etc/rk96x/conf/
	@chmod 777 $(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/etc/rk96x/conf/wpa_supplicant.conf \
		$(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/etc/rk96x/conf/wifi_user.conf \
		$(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/etc/rk96x/conf/bt_user.conf


	# Install scripts

	# to /etc/profile.d
	@mkdir -p $(CURRENT_DIR)/$(PKG_BIN_INSTALL)/etc/profile.d
	@cp -f $(CURRENT_DIR)/rk96x/script/rk96x_profile.sh $(CURRENT_DIR)/$(PKG_BIN_INSTALL)/etc/profile.d/
	@chmod +x $(CURRENT_DIR)/$(PKG_BIN_INSTALL)/etc/profile.d/rk96x_profile.sh

	# to /oem/usr/bin

	@sed -e "s|^export RK_WIFI_POWERSAVE_ENABLE=.*|export RK_WIFI_POWERSAVE_ENABLE=$(RK_WIFI_POWERSAVE_ENABLE)|" \
	-e "s|^export RK_WIFI_TESTTOOLS_ENABLE=.*|export RK_WIFI_TESTTOOLS_ENABLE=$(RK_WIFI_TESTTOOLS_ENABLE)|" \
	-e "s|^export RK_WIFI_BTMCU_TTYS=.*|export RK_WIFI_BTMCU_TTYS=$(RK_WIFI_BTMCU_TTYS)|" \
	-e "s|^export RK96X_CONF_PATH=.*|export RK96X_CONF_PATH=$(RK96X_CONF_PATH)|" \
	$(CURRENT_DIR)/rk96x/script/rk96x_env.sh > $(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/bin/rk96x_env.sh

	# to rk96x/script

	@cp -f $(CURRENT_DIR)/rk96x/script/wifi_start.sh \
		$(CURRENT_DIR)/rk96x/script/bt_start.sh \
		$(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/bin/

	@chmod +x $(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/bin/wifi_start.sh \
		$(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/bin/bt_start.sh

ifeq ($(RK_WIFI_TESTTOOLS_ENABLE),y)
	#test app
	@cp -f $(CURRENT_DIR)/rk96x/test/rk_kp/rkkp $(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/bin/
	@chmod +x $(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/bin/rkkp

	@cp -f $(CURRENT_DIR)/rk96x/test/rk_net/rknet $(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/bin/
	@chmod +x $(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/bin/rknet

	#test conf
	@cp -f $(CURRENT_DIR)/conf/sys_test.conf \
		$(CURRENT_DIR)/conf/ap_list.conf \
		$(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/etc/rk96x/conf/
	@chmod 777 $(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/etc/rk96x/conf/sys_test.conf \
		$(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/etc/rk96x/conf/ap_list.conf

	#test sh
	@cp -f $(CURRENT_DIR)/rk96x/script/sys_test.sh \
		$(CURRENT_DIR)/rk96x/script/sys_test_fw_switch.sh \
		$(CURRENT_DIR)/rk96x/script/sys_test_wifi_switch_ap_random.sh \
		$(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/bin/

	@chmod +x $(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/bin/sys_test.sh \
		$(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/bin/sys_test_fw_switch.sh \
		$(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/bin/sys_test_wifi_switch_ap_random.sh
endif 

	@cp -f $(CURRENT_DIR)/rk96x/script/wifi_debug.sh $(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/bin/wifi_debug.sh

else # not RK96X
	make -C $(CURRENT_DIR)/wifi/ clean;
	@CC="$(RK_APP_CROSS)-gcc" GCC="$(RK_APP_CROSS)-gcc" \
	CXX="$(RK_APP_CROSS)-g++" CPP="$(RK_APP_CROSS)-cpp" \
	make -C $(CURRENT_DIR)/wifi/;
	@chmod +x $(CURRENT_DIR)/wifi/test/rkwifi_server ;
	@cp -fa $(CURRENT_DIR)/wifi/test/rkwifi_server $(CURRENT_DIR)/$(PKG_BIN_INSTALL)/usr/bin;
	@cp -fa $(CURRENT_DIR)/wifi/librkwifibt.so $(CURRENT_DIR)/$(PKG_BIN_INSTALL)/usr/lib;
endif #RK_ENABLE_WIFI_CHIP


	@cp -fa $(CURRENT_DIR)/$(PKG_BIN)/bin/wpa_supplicant \
		$(CURRENT_DIR)/$(PKG_BIN)/bin/wpa_cli \
		$(CURRENT_DIR)/$(PKG_BIN_INSTALL)/usr/bin;

	@cp -fa $(CURRENT_DIR)/conf/wpa_supplicant.conf $(CURRENT_DIR)/$(PKG_BIN_INSTALL)/etc;
	@cp -fa $(CURRENT_DIR)/$(PKG_BIN)/lib/libwpa_client.so $(CURRENT_DIR)/$(PKG_BIN_INSTALL)/usr/lib;
ifeq ($(RK_ENABLE_WIFI_WPA3),y)
	@cp -fa $(CURRENT_DIR)/$(PKG_NAME_OPENSSL)/libcrypto.so.3 $(CURRENT_DIR)/$(PKG_BIN_INSTALL)/usr/lib;
	@cp -fa $(CURRENT_DIR)/$(PKG_NAME_OPENSSL)/libssl.so.3 $(CURRENT_DIR)/$(PKG_BIN_INSTALL)/usr/lib;
endif

ifneq ($(findstring $(RK_ENABLE_WIFI_CHIP),"AP6XXX"),)
	@mkdir -p $(CURRENT_DIR)/$(PKG_BIN_INSTALL)/vendor/etc/firmware;
	@cp -fa $(CURRENT_DIR)/AP6256/nvram_ap6256.txt $(CURRENT_DIR)/$(PKG_BIN_INSTALL)/vendor/etc/firmware;
	@cp -fa $(CURRENT_DIR)/AP6256/fw_bcm43456c5_ag.bin $(CURRENT_DIR)/$(PKG_BIN_INSTALL)/vendor/etc/firmware;
endif

	@cp -fa $(CURRENT_DIR)/$(PKG_BIN)/bin/hostapd $(CURRENT_DIR)/$(PKG_BIN_INSTALL)/usr/bin;

libnl:
	@test -f $(CURRENT_DIR)/$(PKG_BIN)/lib/libnl-3.a || (\
	rm -rf $(CURRENT_DIR)/$(PKG_BIN); \
	rm -fr $(CURRENT_DIR)/$(PKG_NAME_LIBNL)/; \
	tar -xf $(PKG_TARBALL_LIBNL); \
	mkdir -p $(CURRENT_DIR)/$(PKG_BIN); \
	pushd $(CURRENT_DIR)/$(PKG_NAME_LIBNL)/; \
		CC="$(RK_APP_CROSS)-gcc" GCC="$(RK_APP_CROSS)-gcc" \
		CXX="$(RK_APP_CROSS)-g++" CPP="$(RK_APP_CROSS)-cpp" \
			./configure \
			--prefix=$(CURRENT_DIR)/$(PKG_BIN) --host=arm-linux --enable-static ;\
		make -j$(RK_APP_JOBS) > /dev/null || exit -1; \
		make install > /dev/null; \
		rm $(CURRENT_DIR)/$(PKG_BIN)/lib/lib*0 ;\
	popd; )

ifeq ($(RK_ENABLE_WIFI_WPA3),y)
	@test -f $(CURRENT_DIR)/$(PKG_BIN)/lib/libssl.so.3 || (\
	tar -xf $(PKG_TARBALL_OPENSSL); \
	pushd $(CURRENT_DIR)/$(PKG_NAME_OPENSSL); \
		CC="$(RK_APP_CROSS)-gcc" GCC="$(RK_APP_CROSS)-gcc" \
		CXX="$(RK_APP_CROSS)-g++" CPP="$(RK_APP_CROSS)-cpp" \
		AR="$(RK_APP_CROSS)-ar" NM="$(RK_APP_CROSS)-nm" \
			./Configure \
			--prefix=$(CURRENT_DIR)/$(PKG_BIN) linux-armv4 no-asm shared --openssldir=$(CURRENT_DIR)/$(PKG_BIN)/etc/ssl ;\
		make -j$(RK_APP_JOBS) > /dev/null || exit -1; \
		make install > /dev/null; \
	popd; )
endif

build-bt: build-wifi
ifeq ($(RK_ENABLE_BLUETOOTH),y)

	#Rockchip bt
	@chmod +x $(CURRENT_DIR)/rk96x/script/rk960_bt.sh
	@cp -f $(CURRENT_DIR)/rk96x/script/rk960_bt.sh $(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/bin/

	#Realtek bluetooth
ifneq ($(filter RTL8821CS RTL8723DS,$(RK_ENABLE_WIFI_CHIP)),)
	make -C $(CURRENT_DIR)/bt/rtk_hciattach/ clean
	@CC="$(RK_APP_CROSS)-gcc" GCC="$(RK_APP_CROSS)-gcc" \
	CXX="$(RK_APP_CROSS)-g++" CPP="$(RK_APP_CROSS)-cpp" \
	make -C $(CURRENT_DIR)/bt/rtk_hciattach/

	@cp -fa $(CURRENT_DIR)/bt/rtk_hciattach/rtk_hciattach $(CURRENT_DIR)/$(PKG_BIN_INSTALL)/usr/bin;
	@chmod +x $(CURRENT_DIR)/$(PKG_BIN_INSTALL)/bin/rtk_hciattach
endif

	#build bluez source
	@CC="$(RK_APP_CROSS)-gcc" GCC="$(RK_APP_CROSS)-gcc" \
	CXX="$(RK_APP_CROSS)-g++" CPP="$(RK_APP_CROSS)-cpp" \
	make -C $(CURRENT_DIR)/bt/;

	#GATT APP
	make -C $(CURRENT_DIR)/bt/rk_btgatt/ clean
	@CC="$(RK_APP_CROSS)-gcc" GCC="$(RK_APP_CROSS)-gcc" \
	CXX="$(RK_APP_CROSS)-g++" CPP="$(RK_APP_CROSS)-cpp" \
	make -C $(CURRENT_DIR)/bt/rk_btgatt/
	#Server
	@cp -f $(CURRENT_DIR)/bt/rk_btgatt/rkbt_gatt_server $(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/bin/
	@chmod +x $(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/bin/rkbt_gatt_server
	#Client
	@cp -f $(CURRENT_DIR)/bt/rk_btgatt/rkbt_gatt_client $(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/bin/
	@chmod +x $(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/bin/rkbt_gatt_client

	# for debug
ifeq ($(RK_BLUETOOTH_ENABLE_BTMON),y)
	@cp -fa $(CURRENT_DIR)/bt/external/bluez-5.77/monitor/btmon $(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/bin/
	@chmod +x $(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/bin/btmon
	@cp -fa $(CURRENT_DIR)/bt/external/out/lib/libglib-2.0.so* $(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/lib/
	@cp -fa $(CURRENT_DIR)/bt/external/out/lib/libiconv.so.* $(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/lib/
	@cp -fa $(CURRENT_DIR)/bt/external/out/lib/libpcre.so.* $(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/lib/
	@cp -fa $(CURRENT_DIR)/bt/external/out/lib/libffi.so.* $(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/lib/
	@cp -fa $(CURRENT_DIR)/bt/external/out/lib/libz.so.* $(CURRENT_DIR)/$(PKG_BIN_INSTALL_DIR)/lib/
endif

	#bluez tools
	@chmod +x $(CURRENT_DIR)/bt/external/bluez-5.77/tools/hciconfig
	@chmod +x $(CURRENT_DIR)/bt/external/bluez-5.77/tools/hcidump
	@chmod +x $(CURRENT_DIR)/bt/external/bluez-5.77/tools/hcitool
	@cp -fa $(CURRENT_DIR)/bt/external/bluez-5.77/tools/hciconfig $(CURRENT_DIR)/$(PKG_BIN_INSTALL)/usr/bin;
	@cp -fa $(CURRENT_DIR)/bt/external/bluez-5.77/tools/hcidump $(CURRENT_DIR)/$(PKG_BIN_INSTALL)/usr/bin;
	@cp -fa $(CURRENT_DIR)/bt/external/bluez-5.77/tools/hcitool $(CURRENT_DIR)/$(PKG_BIN_INSTALL)/usr/bin;
endif #RK_ENABLE_BLUETOOTH

distclean:
	@make -C $(CURRENT_DIR)/wifi clean;
	@make -C $(CURRENT_DIR)/rk96x clean;
	@make -C $(CURRENT_DIR)/bt distclean;
	@make -C $(CURRENT_DIR)/bt/rk_hciattach clean;
	@make -C $(CURRENT_DIR)/bt/rk_btgatt clean;
	@rm -f ./conf/version_info.txt;
	@rm -fr $(PKG_NAME_LIBNL);
	@rm -rf $(PKG_NAME_WPA_RTK_TOOLS);
	@rm -rf $(PKG_NAME_WPA_TOOLS);
	@rm -rf $(PKG_NAME_HOSTAPD_TOOLS);
	@rm -rf $(PKG_NAME_OPENSSL_TOOLS);
	@rm -rf $(PKG_BIN);
	@rm -rf $(PKG_BIN_INSTALL_DIR);
	@rm -rf $(PKG_NAME_WPA_TOOLS)/$(PKG_BIN);

clean: distclean

info:
ifneq ($(RK_ENABLE_WIFI),)
	@echo -e "$(C_YELLOW)-------------------------------------------------------------------------$(C_NORMAL)"
	@echo -e "$(C_YELLOW)-------------------------------------------------------------------------$(C_NORMAL)"
endif
