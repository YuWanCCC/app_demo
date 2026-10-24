#!/bin/sh

# RK_WIFI suite variables, that will be replaced by actual values when building
export RK_WIFI_POWERSAVE_ENABLE=
export RK_WIFI_TESTTOOLS_ENABLE=
export RK_WIFI_BTMCU_TTYS=
export RK96X_CONF_PATH=
export RK_ENABLE_WIFI_CHIP=
export RK96X_TARGET_PATH=$(find / -type d -path "*/rk96x/fw" 2>/dev/null | head -n 1 | xargs -I {} dirname "{}" 2>/dev/null)
