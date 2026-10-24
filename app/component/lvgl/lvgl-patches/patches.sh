
#!/bin/bash
set -x
if [ ! -f "$1/.patched" ]; then
patch -p1 < 0001-fix-arc-fix-setting-value-in-REVERSED-mode.patch
patch -p1 < 0002-chore-group-add-some-missing-assertions-5941.patch
patch -p1 < 0003-fix-cache-gradient-key-computing-5930.patch
patch -p1 < 0004-fix-event-fix-LV_EVENT_PREPROCESS-flag-was-not-proce.patch
patch -p1 < 0005-fix-lv_table-suppressed-compilation-warnings-due-to-.patch
patch -p1 < 0006-fix-roller-set-the-position-of-the-selected-test-cor.patch
patch -p1 < 0007-fix-roller-Avoid-divided-by-zero-during-draw-event-6.patch
patch -p1 < 0008-fix-flex-handle-malloc-fail-6443.patch
patch -p1 < 0009-fix-display-delete-previous-screen-instead-of-curren.patch
patch -p1 < 0010-fix-png-use-color-rounding-like-img-conv-tool-6626.patch
patch -p1 < 0011-feat-style-add-style_cnt-and-styles-checks-6873.patch
patch -p1 < 0012-fix-label-add-missing-repeat_cnt-animation-parameter.patch
patch -p1 < 0013-fix-chart-fix-memory-leak-with-scatter-charts-7096.patch
patch -p1 < 0014-env_support-cmake-custom.cmake-Build-shared-librarie.patch
patch -p1 < 0015-Increase-width-and-height-limits.patch
patch -p1 < 0016-gif-add-cache.patch
patch -p1 < 0017-add-_lv_img_cache_get_info.patch
patch -p1 < 0018-env_support-cmake-custom.cmake-Add-lvgl-demos-switch.patch
patch -p1 < 0019-HACK-disable-too-complicated-demo.patch
patch -p1 < 0020-env_support-cmake-support-sdl.patch
patch -p1 < 0021-fix-draw-sdl-fix-sdl-crash.patch
patch -p1 < 0022-env_support-cmake-add-freetype-header-file-path.patch
patch -p1 < 0023-src-add-lv_systick.patch
patch -p1 < 0024-src-lv_span-fix-conflicting-function-definitions.patch
patch -p1 < 0025-demos-benchmark-modify-benchmark-test-scenes.patch
patch -p1 < 0026-src-extra-New-widgets-inputbox.patch
patch -p1 < 0027-src-extra-New-widgets-stiker_matrix.patch
patch -p1 < 0028-src-lv_lru-Add-error-log-for-lru-testing.patch
patch -p1 < 0029-HACK-draw-lv_draw_sdl_composite-Decrease-sdl-texture.patch
patch -p1 < 0030-src-draw-sdl-Fix-wrong-scale-of-angle-in-lv_draw_sdl.patch
patch -p1 < 0031-fix-snapshot-always-use-sw-driver-to-taking-sanpshot.patch
patch -p1 < 0032-fix-indev-Fix-indev-would-still-trigger-feedback-whe.patch
patch -p1 < 0033-lvgl-implement-deinit-funcs.patch
patch -p1 < 0034-fix-sdl-use-real_radius-to-create-SDL_Texture.patch
patch -p1 < 0035-fix-sdl-add-API-to-update-cached-texture.patch
patch -p1 < 0036-fix-rlottie-update-texture-if-SDL-is-enabled.patch
patch -p1 < 0037-fix-sdl-fix-crash-in-lv_draw_sdl_draw_line.patch
fi
if [ "$?" = "0" ]; then
	touch "$1/.patched"
fi
