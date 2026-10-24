#!/bin/bash
#
# app_demo standalone build entry
#
# Usage:
#   ./build.sh                 # build all enabled apps (see RK_APP_TYPE in env.sh)
#   ./build.sh clean           # clean
#   ./build.sh distclean       # distclean
#   ./build.sh info            # show build configuration
#
set -e

APP_DEMO_TOP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# load environment configuration
source "$APP_DEMO_TOP_DIR/env.sh"

# non-empty RK_PROJECT_OUTPUT makes app/Makefile.param skip the SDK
# .BoardConfig.mk, i.e. standalone mode
export RK_PROJECT_OUTPUT="$APP_DEMO_TOP_DIR/app"

echo "=============================================================="
echo " app_demo standalone build"
echo "   CHIP      : $RK_CHIP"
echo "   CROSS     : $RK_TOOLCHAIN_CROSS"
echo "   TOOLCHAIN : $RK_TOOLCHAIN_DIR"
echo "   MEDIA     : $RK_PROJECT_PATH_MEDIA"
echo "   APP_TYPE  : $RK_APP_TYPE"
echo "=============================================================="

exec make -C "$APP_DEMO_TOP_DIR/app" "$@"
