#!/bin/bash
#
# app_demo standalone build environment configuration
#
# Usage: sourced by build.sh automatically; can also be sourced manually:
#   source env.sh && make -C app
#
# All paths below are configurable. Point RK_TOOLCHAIN_DIR to any
# Rockchip toolchain installation to make this project fully portable.

APP_DEMO_TOP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

################################################################################
# 1. Board / platform configuration
#    (equivalent to project/cfg/BoardConfig*.mk in the original SDK)
################################################################################
export RK_CHIP=rv1126b
export RK_ARCH=arm64
export RK_TOOLCHAIN_CROSS=aarch64-rockchip1240-linux-gnu
export RK_BOOT_MEDIUM=emmc

# which apps to build, space separated. supported items include:
#   RKIPC_RV1126B / RKIPC_RV1126B_DUAL_IPC / RKIPC_RV1126B_DV  -> rkipc
#   RK_APP_RKAI                                                 -> rkai
#   UVC_TINY                                                    -> uvc_app_tiny
#   RK_WIFI_APP                                                 -> wifi_app
#   YOLOWORLD_DEMO / PROJECTOR_DEMO                             -> testdemo
export RK_APP_TYPE="RKIPC_RV1126B RK_APP_RKAI UVC_TINY"

# build type: RELEASE / DEBUG / DEBUG_ASAN
export RK_BUILD_VERSION_TYPE=RELEASE

# parallel jobs
export RK_JOBS=$(( $(getconf _NPROCESSORS_ONLN) / 2 + 1 ))

################################################################################
# 2. Dependency paths (the key of portability)
################################################################################
# Cross toolchain root directory, layout must be:
#   $RK_TOOLCHAIN_DIR/<RK_TOOLCHAIN_CROSS>/bin/<RK_TOOLCHAIN_CROSS>-gcc
# Default uses the toolchain shipped with this project, fully self-contained.
# You may also point it to an external toolchain installation.
export RK_TOOLCHAIN_DIR=${RK_TOOLCHAIN_DIR:-"$APP_DEMO_TOP_DIR/toolchain"}

# Media prebuilt libraries (include/lib/root), shipped with this project
export RK_PROJECT_PATH_MEDIA="$APP_DEMO_TOP_DIR/media_out"

# Prebuilt openssl/zlib for nginx, shipped with this project
export RK_APP_TOOLKITS_DIR="$APP_DEMO_TOP_DIR/third_party/toolkits"
export RK_APP_PCRE2_TAR_DIR="$APP_DEMO_TOP_DIR/third_party/pcre2"

# Optional: PC packaging tools (mkfs.* etc.). Only needed when packaging
# rootfs images, NOT required for building apps. Leave empty if absent.
export RK_PROJECT_PATH_PC_TOOLS="${RK_PROJECT_PATH_PC_TOOLS:-}"
