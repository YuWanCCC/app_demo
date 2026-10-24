#!/bin/bash
if [ ! -f "patched" ]; then
patch -p1 < 0001-auto-type-sizeof-rework-autotest-to-be-cross-compila.patch
patch -p1 < 0002-auto-feature-add-mechanism-allowing-to-force-feature.patch
patch -p1 < 0003-auto-set-ngx_feature_run_force_result-for-each-featu.patch
patch -p1 < 0004-auto-lib-libxslt-conf-use-pkg-config.patch
patch -p1 < 0005-auto-unix-make-sys_nerr-guessing-cross-friendly.patch
patch -p1 < 0006-auto-lib-openssl-conf-use-pkg-config.patch
patch -p1 < 0007-auto-lib-libgd-conf-use-pkg-config.patch
patch -p1 < 0008-src-os-unix-ngx_linux_config.h-only-include-dlfcn.h-.patch
patch -p1 < 0009-auto-os-linux-fix-build-with-libxcrypt.patch
patch -p1 < 0010-Allow-forcing-of-endianness-for-cross-compilation.patch
fi
if [ "$?" = "0" ]; then
	touch "patched"
fi
