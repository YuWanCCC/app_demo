ifeq ($(SYSDRV_PARAM), )
    SYSDRV_PARAM:=../../Makefile.param
    include $(SYSDRV_PARAM)
endif

# Cross-compilation toolchain settings
#TOOLCHAIN_PATH := /home/xy/sdk/ipc/rv1126b/tools/linux/toolchain/arm-rockchip1240-linux-gnueabihf/bin
#CROSS_COMPILE := arm-rockchip1240-linux-gnueabihf-
CROSS_COMPILE := $(RK_APP_CROSS)-
CC = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++
AR = $(CROSS_COMPILE)ar
RANLIB = $(CROSS_COMPILE)ranlib
STRIP = $(CROSS_COMPILE)strip

# Number of parallel jobs for compilation (default: number of CPU cores)
JOBS ?= $(shell nproc)

$(info CROSS_COMPILE= $(RK_APP_CROSS) $(CROSS_COMPILE) $(CC))

# Project paths
CURRENT_DIR := $(shell pwd)
PROJ_PATH = $(CURRENT_DIR)
EXTERNAL_PATH := $(PROJ_PATH)/external
OUTPUT_PATH := $(EXTERNAL_PATH)/out

export PKG_CONFIG_PATH=$(OUTPUT_PATH)/lib/pkgconfig
$(info PKG_CONFIG_PATH $(PKG_CONFIG_PATH))

# Compilation flags
CFLAGS := -I$(OUTPUT_PATH)/include -w -fPIC
CPPFLAGS := -I$(OUTPUT_PATH)/include
CXXFLAGS := -I$(OUTPUT_PATH)/include
LDFLAGS := -L$(OUTPUT_PATH)/lib -Wl,-rpath-link,$(OUTPUT_PATH)/lib

#if rv1103 or rv1106, need wordexp and gettext
ifneq ($(filter rv1103 rv1106,$(RK_CHIP)),)
DEPS_WORDEXP := wordexp
DEPS_GETTEXT := gettext
EXTRA_LIBS := -lncurses -lwordexp
else
DEPS_WORDEXP :=
DEPS_GETTEXT :=
EXTRA_LIBS := -lncurses
endif

# Source packages
EXPAT_SRC := expat-2.2.6.tar.bz2
LIBFFI_SRC := libffi-3.4.2.tar.gz
PCRE_SRC := pcre-8.42.tar.bz2
ZLIB_SRC := zlib-1.2.11.tar.xz
NCURSES_SRC := ncurses-6.1.tar.gz
READLINE_SRC := readline-7.0.tar.gz
WORDEXP_SRC := wordexp.tar.gz
DBUS_SRC := dbus-1.12.10.tar.gz
GLIB_SRC := glib-2.56.1.tar.xz
BLUEZ_SRC := bluez-5.77.tar.xz
LIBICONV_SRC := libiconv-1.16.tar.gz
GETTEXT_SRC := gettext-0.22.5.tar.gz

# Extracted directory names
EXPAT_DIR := expat-2.2.6
LIBFFI_DIR := libffi-3.4.2
PCRE_DIR := pcre-8.42
ZLIB_DIR := zlib-1.2.11
NCURSES_DIR := ncurses-6.1
READLINE_DIR := readline-7.0
DBUS_DIR := dbus-1.12.10
GLIB_DIR := glib-2.56.1
BLUEZ_DIR := bluez-5.77
LIBICONV_DIR := libiconv-1.16
GETTEXT_DIR := gettext-0.22.5
WORDEXP_DIR := wordexp

# Check if already compiled
#check_bluez:
#	@echo "Checking if bluez is already compiled..."
#	@if [ -f "$(EXTERNAL_PATH)/$(BLUEZ_DIR)/src/bluetoothd" ] && [ -f "$(EXTERNAL_PATH)/$(BLUEZ_DIR)/tools/hciconfig" ]; then \
#		echo "bluetoothd and hciconfig already exist, skipping compilation..."; \
#		exit 0; \
#	fi

# Default target
#all: check_bluez setup expat libffi pcre zlib libiconv ncurses readline dbus glib bluez
all: bluez


# Build setup
setup:
	@echo "Setting up build environment..."
	@mkdir -p $(OUTPUT_PATH)
	@echo "Setting PATH variable..."
	#@export PATH=$(TOOLCHAIN_PATH):$$PATH

# Build expat
expat: setup
	@if [ ! -f "$(OUTPUT_PATH)/lib/pkgconfig/expat.pc" ]; then \
		echo "Building expat..."; \
		if [ ! -d $(EXTERNAL_PATH)/$(EXPAT_DIR) ]; then \
			tar xf $(EXTERNAL_PATH)/$(EXPAT_SRC) -C $(EXTERNAL_PATH); \
		fi; \
		cd $(EXTERNAL_PATH)/$(EXPAT_DIR) && \
		./configure --host=$(CROSS_COMPILE:%-=%) \
			--prefix=$(OUTPUT_PATH) \
			--without-docbook  \
			CFLAGS="$(CFLAGS)" \
			CPPFLAGS="$(CPPFLAGS)" \
			CXXFLAGS="$(CXXFLAGS)" \
			LDFLAGS="$(LDFLAGS)" && \
		make -j$(JOBS) && \
		make install; \
	fi

# Build gettext
gettext: libiconv
	@if [ ! -f "$(OUTPUT_PATH)/lib/libgettextlib.so" ]; then \
		echo "Building gettext..."; \
		if [ ! -d $(EXTERNAL_PATH)/$(GETTEXT_DIR) ]; then \
			tar xf $(EXTERNAL_PATH)/$(GETTEXT_SRC) -C $(EXTERNAL_PATH); \
		fi; \
		cd $(EXTERNAL_PATH)/$(GETTEXT_DIR) && \
		./configure --host=$(CROSS_COMPILE:%-=%) \
			--prefix=$(OUTPUT_PATH) \
			CFLAGS="$(CFLAGS)" \
			CPPFLAGS="$(CPPFLAGS)" \
			CXXFLAGS="$(CXXFLAGS)" \
			LDFLAGS="$(LDFLAGS)" && \
		make -j$(JOBS) && \
		make install; \
	fi

# Build wordexp
wordexp: setup
	@if [ ! -f "$(OUTPUT_PATH)/lib/libwordexp.a" ]; then \
		echo "Building wordexp..."; \
		if [ ! -d $(EXTERNAL_PATH)/$(WORDEXP_DIR) ]; then \
			tar xf $(EXTERNAL_PATH)/$(WORDEXP_SRC) -C $(EXTERNAL_PATH); \
		fi; \
		cd $(EXTERNAL_PATH)/$(WORDEXP_DIR) && \
		make CC=$(CC) && \
		make install LIB_DIR=$(OUTPUT_PATH); \
	fi

# Build libffi
libffi: setup
	@if [ ! -f "$(OUTPUT_PATH)/lib/pkgconfig/libffi.pc" ]; then \
		echo "Building libffi..."; \
		if [ ! -d $(EXTERNAL_PATH)/$(LIBFFI_DIR) ]; then \
			tar xf $(EXTERNAL_PATH)/$(LIBFFI_SRC) -C $(EXTERNAL_PATH); \
		fi; \
		cd $(EXTERNAL_PATH)/$(LIBFFI_DIR) && \
		./configure --host=$(CROSS_COMPILE:%-=%) \
			--prefix=$(OUTPUT_PATH) \
			CFLAGS="$(CFLAGS)" \
			CPPFLAGS="$(CPPFLAGS)" \
			CXXFLAGS="$(CXXFLAGS)" \
			LDFLAGS="$(LDFLAGS)" && \
		make -j$(JOBS) && \
		make install; \
	fi

# Build iconv
libiconv: setup
	@if [ ! -f "$(OUTPUT_PATH)/lib/libiconv.so" ]; then \
		echo "Building iconv..."; \
		if [ ! -d $(EXTERNAL_PATH)/$(LIBICONV_DIR) ]; then \
			tar xf $(EXTERNAL_PATH)/$(LIBICONV_SRC) -C $(EXTERNAL_PATH); \
		fi; \
		cd $(EXTERNAL_PATH)/$(LIBICONV_DIR) && \
		./configure --host=$(CROSS_COMPILE:%-=%) \
			--prefix=$(OUTPUT_PATH) \
			CFLAGS="$(CFLAGS)" \
			CPPFLAGS="$(CPPFLAGS)" \
			CXXFLAGS="$(CXXFLAGS)" \
			LDFLAGS="$(LDFLAGS)" && \
		make -j$(JOBS) && \
		make install; \
	fi

# Build pcre
pcre: setup
	@if [ ! -f "$(OUTPUT_PATH)/lib/pkgconfig/libpcre.pc" ]; then \
		echo "Building pcre..."; \
		if [ ! -d $(EXTERNAL_PATH)/$(PCRE_DIR) ]; then \
			tar xf $(EXTERNAL_PATH)/$(PCRE_SRC) -C $(EXTERNAL_PATH); \
		fi; \
		cd $(EXTERNAL_PATH)/$(PCRE_DIR) && \
		./configure --host=$(CROSS_COMPILE:%-=%) \
			--prefix=$(OUTPUT_PATH) \
			CFLAGS="$(CFLAGS)" \
			CPPFLAGS="$(CPPFLAGS)" \
			CXXFLAGS="$(CXXFLAGS)" \
			LDFLAGS="$(LDFLAGS)" && \
		make -j$(JOBS) && \
		make install; \
	fi

# Build zlib
zlib: setup
	@if [ ! -f "$(OUTPUT_PATH)/lib/pkgconfig/zlib.pc" ]; then \
		echo "Building zlib... CC=$(CC)"; \
		if [ ! -d $(EXTERNAL_PATH)/$(ZLIB_DIR) ]; then \
			tar xf $(EXTERNAL_PATH)/$(ZLIB_SRC) -C $(EXTERNAL_PATH); \
		fi; \
		cd $(EXTERNAL_PATH)/$(ZLIB_DIR) && \
		CC=$(CC) ./configure --prefix=$(OUTPUT_PATH) && \
		make -j$(JOBS) && \
		make install; \
	fi

# Build ncurses
ncurses: setup
	@if [ ! -f "$(OUTPUT_PATH)/lib/libncurses.a" ]; then \
		echo "Building ncurses..."; \
		if [ ! -d $(EXTERNAL_PATH)/$(NCURSES_DIR) ]; then \
			tar xf $(EXTERNAL_PATH)/$(NCURSES_SRC) -C $(EXTERNAL_PATH); \
		fi; \
		cd $(EXTERNAL_PATH)/$(NCURSES_DIR) && \
		./configure --host=$(CROSS_COMPILE:%-=%) \
			--prefix=$(OUTPUT_PATH) \
			CFLAGS="$(CFLAGS)" \
			CPPFLAGS="$(CPPFLAGS)" \
			CXXFLAGS="$(CXXFLAGS)" \
			LDFLAGS="$(LDFLAGS)" \
			--disable-stripping && \
		make -j$(JOBS) && \
		make install; \
	fi

# Build readline
readline: ncurses
	@if [ ! -f "$(OUTPUT_PATH)/lib/libreadline.a" ]; then \
		echo "Building readline..."; \
		if [ ! -d $(EXTERNAL_PATH)/$(READLINE_DIR) ]; then \
			tar xf $(EXTERNAL_PATH)/$(READLINE_SRC) -C $(EXTERNAL_PATH); \
		fi; \
		cd $(EXTERNAL_PATH)/$(READLINE_DIR) && \
		./configure --host=$(CROSS_COMPILE:%-=%) \
			--prefix=$(OUTPUT_PATH) \
			CFLAGS="$(CFLAGS)" \
			CPPFLAGS="$(CPPFLAGS)" \
			CXXFLAGS="$(CXXFLAGS)" \
			LDFLAGS="$(LDFLAGS)" && \
		make -j$(JOBS) && \
		make install; \
	fi

# Build glib
glib: libffi libiconv pcre zlib $(DEPS_GETTEXT)
	@if [ ! -f "$(OUTPUT_PATH)/lib/pkgconfig/glib-2.0.pc" ]; then \
		echo "Building glib..."; \
		if [ ! -d $(EXTERNAL_PATH)/$(GLIB_DIR) ]; then \
			tar xf $(EXTERNAL_PATH)/$(GLIB_SRC) -C $(EXTERNAL_PATH); \
		fi; \
		echo "Creating glib.cache file..."; \
		echo "glib_cv_long_long_format=ll" > $(EXTERNAL_PATH)/$(GLIB_DIR)/glib.cache; \
		echo "glib_cv_stack_grows=no" >> $(EXTERNAL_PATH)/$(GLIB_DIR)/glib.cache; \
		echo "glib_cv_have_strlcpy=no" >> $(EXTERNAL_PATH)/$(GLIB_DIR)/glib.cache; \
		echo "glib_cv_have_qsort_r=yes" >> $(EXTERNAL_PATH)/$(GLIB_DIR)/glib.cache; \
		echo "glib_cv_va_val_copy=yes" >> $(EXTERNAL_PATH)/$(GLIB_DIR)/glib.cache; \
		echo "glib_cv_uscore=no" >> $(EXTERNAL_PATH)/$(GLIB_DIR)/glib.cache; \
		echo "glib_cv_rtldglobal_broken=no" >> $(EXTERNAL_PATH)/$(GLIB_DIR)/glib.cache; \
		echo "ac_cv_func_posix_getpwuid_r=yes" >> $(EXTERNAL_PATH)/$(GLIB_DIR)/glib.cache; \
		echo "ac_cv_func_posix_getgrgid_r=yes" >> $(EXTERNAL_PATH)/$(GLIB_DIR)/glib.cache; \
		cd $(EXTERNAL_PATH)/$(GLIB_DIR) && \
		./configure --host=$(CROSS_COMPILE:%-=%) \
			--prefix=$(OUTPUT_PATH) \
			CFLAGS="$(CFLAGS)" \
			CPPFLAGS="$(CPPFLAGS)" \
			CXXFLAGS="$(CXXFLAGS)" \
			LDFLAGS="$(LDFLAGS)" \
			--with-libiconv=gnu \
			--disable-selinux \
			--disable-xattr \
			--disable-libelf \
			--disable-libmount \
			--cache-file=glib.cache \
			glib_cv_printf_guint64_format=ll \
			glib_cv_scanf_guint64_format=ll \
			glib_cv_stack_grows=no && \
		make -j$(JOBS) && \
		make install; \
	fi

# Build dbus
dbus: expat glib
	@if [ ! -f "$(OUTPUT_PATH)/lib/pkgconfig/dbus-1.pc" ]; then \
		echo "Building dbus..."; \
		if [ ! -d $(EXTERNAL_PATH)/$(DBUS_DIR) ]; then \
			tar xf $(EXTERNAL_PATH)/$(DBUS_SRC) -C $(EXTERNAL_PATH); \
		fi; \
		cd $(EXTERNAL_PATH)/$(DBUS_DIR) && \
		./configure --host=$(CROSS_COMPILE:%-=%) \
			--prefix=$(OUTPUT_PATH) \
			CFLAGS="$(CFLAGS)" \
			CPPFLAGS="$(CPPFLAGS)" \
			CXXFLAGS="$(CXXFLAGS)" \
			LDFLAGS="$(LDFLAGS)" \
			--disable-selinux \
			--disable-tests \
			--disable-systemd \
			--disable-apparmor \
			--disable-libaudit \
			--disable-doxygen-docs \
			--disable-xml-docs && \
		make -j$(JOBS) && \
		make install; \
	fi

# Build bluez
bluez: dbus glib readline $(DEPS_WORDEXP)
	@if [ ! -f "$(EXTERNAL_PATH)/$(BLUEZ_DIR)/tools/btgatt-server" ]; then \
		echo "Building bluez..."; \
		if [ ! -d $(EXTERNAL_PATH)/$(BLUEZ_DIR) ]; then \
			tar xf $(EXTERNAL_PATH)/$(BLUEZ_SRC) -C $(EXTERNAL_PATH); \
		fi; \
		cd $(EXTERNAL_PATH)/$(BLUEZ_DIR) && \
		./configure --host=$(CROSS_COMPILE:%-=%) \
			--prefix=$(OUTPUT_PATH) \
			PKG_CONFIG_PATH=$(OUTPUT_PATH)/lib/pkgconfig \
			CFLAGS="$(CFLAGS)" \
			CPPFLAGS="$(CPPFLAGS)" \
			CXXFLAGS="$(CXXFLAGS)" \
			LDFLAGS="$(LDFLAGS)" \
			LIBS="$(EXTRA_LIBS)" \
			--sysconfdir=$(OUTPUT_PATH)/etc \
			--localstatedir=$(OUTPUT_PATH)/var \
			--disable-udev \
			--disable-cups \
			--disable-obex \
			--enable-client \
			--enable-deprecated \
			--enable-tools \
			--disable-systemd \
			--disable-manpages && \
		make -j$(JOBS); \
	fi

# Clean build files
clean:
	@echo "Cleaning build files..."
	rm -rf $(EXTERNAL_PATH)/$(EXPAT_DIR)
	rm -rf $(EXTERNAL_PATH)/$(LIBFFI_DIR)
	rm -rf $(EXTERNAL_PATH)/$(PCRE_DIR)
	rm -rf $(EXTERNAL_PATH)/$(ZLIB_DIR)
	rm -rf $(EXTERNAL_PATH)/$(NCURSES_DIR)
	rm -rf $(EXTERNAL_PATH)/$(READLINE_DIR)
	rm -rf $(EXTERNAL_PATH)/$(DBUS_DIR)
	rm -rf $(EXTERNAL_PATH)/$(GLIB_DIR)
	rm -rf $(EXTERNAL_PATH)/$(BLUEZ_DIR)
	rm -rf $(EXTERNAL_PATH)/$(LIBICONV_DIR)
	rm -rf $(EXTERNAL_PATH)/$(WORDEXP_DIR)
	rm -rf $(EXTERNAL_PATH)/$(GETTEXT_DIR)
	rm -rf rk_hciattach/rk_hciattach
	rm -rf rtk_hciattach/rtk_hciattach
	rm -rf $(OUTPUT_PATH)

# Deep clean (including output directory)
distclean: clean
	@echo "Performing deep clean..."
	rm -rf $(OUTPUT_PATH)

# Set environment variables (execute before applying)
env:
	@echo "Setting environment variables..."
	@echo "export TOOLCHAIN_PATH=$(TOOLCHAIN_PATH)"
	@echo "export PATH=\$$TOOLCHAIN_PATH:\$$PATH"
	@echo "export CROSS_COMPILE=$(CROSS_COMPILE)"
	@echo "export CC=\$${CROSS_COMPILE}gcc"
	@echo "export CXX=\$${CROSS_COMPILE}g++"
	@echo "export AR=\$${CROSS_COMPILE}ar"
	@echo "export RANLIB=\$${CROSS_COMPILE}ranlib"
	@echo "export STRIP=\$${CROSS_COMPILE}strip"

.PHONY: all clean distclean env
.PHONY: setup wordexp gettext expat libffi pcre zlib libiconv ncurses readline dbus glib bluez

