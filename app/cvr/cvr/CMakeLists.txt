cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

project(cvr_app)

include(FindPkgConfig)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(RkAiq REQUIRED)
include_directories(${RKAIQ_INCLUDE_DIRS})

set(COMMON_COMPILE_FLAGS
    -Wall -g -O3 -ggdb -gdwarf -funwind-tables -rdynamic
    -Wno-unused-function
    -Wno-unused-parameter
    -D_GNU_SOURCE
)
add_definitions(${COMMON_COMPILE_FLAGS})

option(USE_REMOTE_CONFIG "Remote configuration function" ON)
option(USE_RKAIQ "Enable RKAIQ" OFF)
option(USE_LVGL "Enable LVGL" OFF)
option(USE_RKAUTO "Enable RKAUTO" OFF)
option(USE_RKAUDIO "Enable RKAUDIO" OFF)
option(USE_RKFASTBOOT "Enable RKFASTBOOT" OFF)
option(USE_FILECACHE "Enable file cache" OFF)
option(USE_RKAOV "Enable RKAOV" OFF)

function(add_feature_definition FEATURE_NAME DEFINE_NAME)
    if(${FEATURE_NAME})
        add_definitions(-D${DEFINE_NAME})
    endif()
endfunction()

add_feature_definition(SOC_RV1106 PLATFORM_RV1106)
add_feature_definition(SOC_RV1103B PLATFORM_RV1103B)
add_feature_definition(SOC_RV1126 PLATFORM_RV1126)
add_feature_definition(SOC_RK3576 PLATFORM_RK3576)
add_feature_definition(SOC_RV1126B PLATFORM_RV1126B)

add_feature_definition(USE_RKAIQ RKAIQ)
add_feature_definition(USE_REMOTE_CONFIG REMOTE_CONFIG)
add_feature_definition(USE_LVGL USE_LVGL)
add_feature_definition(USE_RKAUTO RKAUTO)
add_feature_definition(USE_RKAUDIO RKAUDIO)
add_feature_definition(USE_RKFASTBOOT RKFASTBOOT)
add_feature_definition(USE_FILECACHE FILE_CACHE)
add_feature_definition(USE_META_PARAM META_PARAM)
add_feature_definition(USE_RKAOV USE_RKAOV)

if (
	SOC_RV1106 OR
	SOC_RV1103B
	)
	add_definitions(-DUI_PHY_KEY)
	add_definitions(-DUI_RKADK_ENGINE)
	add_definitions(-DUI_USE_RGA_CPP)
endif()

if (
	SOC_RV1126 OR
	SOC_RK3576
	)
	add_definitions(-DUI_VO_INTF_MIPI)
	add_definitions(-DMPU_OSD_TYPE_EXTRA)
	add_definitions(-DUI_DRM_ENGINE)
	add_definitions(-DUI_USE_RGA_CPP)
endif()

if (
	SOC_RV1126B
	)
	add_definitions(-DUI_RKADK_ENGINE)
	add_definitions(-DUI_VO_INTF_MIPI)
	add_definitions(-DUI_EVDEV_CALIBRATE)
	add_definitions(-DUI_USE_RGA)
endif()


set(SYSTEM_INCLUDE_DIRS
	${CMAKE_SYSROOT}/usr/include/rkaiq
	${CMAKE_SYSROOT}/usr/include/rkaiq/isp
	${CMAKE_SYSROOT}/usr/include/rkaiq/uAPI
	${CMAKE_SYSROOT}/usr/include/rkaiq/uAPI2
	${CMAKE_SYSROOT}/usr/include/rkaiq/algos
	${CMAKE_SYSROOT}/usr/include/rkaiq/common
	${CMAKE_SYSROOT}/usr/include/rkaiq/xcore
	${CMAKE_SYSROOT}/usr/include/rkaiq/iq_parser
	${CMAKE_SYSROOT}/usr/include/rkaiq/iq_parser_v2
	${CMAKE_SYSROOT}/usr/include/libdrm
	${CMAKE_SYSROOT}/usr/include/freetype2
	${CMAKE_SYSROOT}/usr/include/libjpeg9
	${CMAKE_SYSROOT}/usr/include/rkadk
	${CMAKE_SYSROOT}/usr/include/lvgl
)

function(configure_soc_include PATH)
    include_directories(${PROJECT_SOURCE_DIR}/src/mpu/isp/${PATH})
    include_directories(${PROJECT_SOURCE_DIR}/cfg/${PATH})
endfunction()

if(SOC_RV1106)
    configure_soc_include("rv1106_1103")
elseif(SOC_RV1103B)
	configure_soc_include("rv1103b")
elseif(SOC_RV1126)
	configure_soc_include("rv1126")
elseif(SOC_RK3576)
	configure_soc_include("rk3576")
elseif(SOC_RV1126B)
	configure_soc_include("rv1126b")
endif()

set(PROJECT_INCLUDE_DIRS
	${RK_PROJECT_PATH_MEDIA}
	${PROJECT_SOURCE_DIR}
	${PROJECT_SOURCE_DIR}/src
)

include_directories(${SYSTEM_INCLUDE_DIRS} ${PROJECT_INCLUDE_DIRS})

function(configure_isp_source PATH)
    aux_source_directory(${PROJECT_SOURCE_DIR}/src/mpu/isp/${PATH} ISP_SRC)
endfunction()

if(SOC_RV1106)
	aux_source_directory(${PROJECT_SOURCE_DIR}/src/mpu/isp/rv1106_1103 ISP_SRC)
elseif(SOC_RV1103B)
    aux_source_directory(${PROJECT_SOURCE_DIR}/src/mpu/isp/rv1103b ISP_SRC)
elseif(SOC_RV1126)
	aux_source_directory(${PROJECT_SOURCE_DIR}/src/mpu/isp/rv1126 ISP_SRC)
elseif(SOC_RK3576)
	aux_source_directory(${PROJECT_SOURCE_DIR}/src/mpu/isp/rk3576 ISP_SRC)
elseif(SOC_RV1126B)
	aux_source_directory(${PROJECT_SOURCE_DIR}/src/mpu/isp/rv1126b ISP_SRC)
endif()
aux_source_directory(${PROJECT_SOURCE_DIR}/src/mpu/isp/  ISP_SRC)

aux_source_directory(${PROJECT_SOURCE_DIR}/ SRCS)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/utils/ SRCS)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/core/ SRCS)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/app/   SRCS)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/net/  SRCS)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/mpu/ SRCS)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/mpu/param/ SRCS)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/mpu/disp/ SRCS)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/mpu/record/ SRCS)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/mpu/rtsp/ SRCS)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/mpu/photo/ SRCS)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/mpu/sys/  SRCS)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/mpu/osd/  SRCS)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/mpu/player/ SRCS)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/mpu/storage/ SRCS)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/algorithm/  SRCS)

file(GLOB_RECURSE CONFIG_SRC ${PROJECT_SOURCE_DIR}/src/conf/*)

if(USE_RKAUTO)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/algorithm/auto/  SRCS)
endif()

if(USE_RKAUDIO)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/algorithm/audio/  SRCS)
endif()

if(USE_RKAOV)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/mpu/aov/  SRCS)
endif()

if(USE_LVGL)
	aux_source_directory(${PROJECT_SOURCE_DIR}/src/lvgl/  SRCS)
	aux_source_directory(${PROJECT_SOURCE_DIR}/src/lvgl/common   SRCS)
	aux_source_directory(${PROJECT_SOURCE_DIR}/src/lvgl/drivers  SRCS)
	aux_source_directory(${PROJECT_SOURCE_DIR}/src/lvgl/porting  SRCS)
	aux_source_directory(${PROJECT_SOURCE_DIR}/src/ui/common  SRCS)
if(SOC_RV1106 OR SOC_RV1103B)
	aux_source_directory(${PROJECT_SOURCE_DIR}/src/ui/480x320  SRCS)
elseif(SOC_RV1126 OR SOC_RK3576 OR SOC_RV1126B)
	aux_source_directory(${PROJECT_SOURCE_DIR}/src/ui/1280x720  SRCS)
endif()
endif()

add_executable(cvr_app ${SRCS} ${ISP_SRC} ${CONFIG_SRC})

if(USE_RKAUTO)
	target_link_libraries(cvr_app rockauto rknnmrt_rockauto)
endif()

if(USE_RKAUDIO)
	target_link_libraries(cvr_app rkaudio_vqe rkaudio_common rknnmrt)
endif()

if(USE_LVGL)
	target_link_libraries(cvr_app lvgl png)
endif()

if(SOC_RV1106 OR SOC_RV1103B)
	target_link_libraries(cvr_app rockit_full jpeg)
elseif(SOC_RV1126 OR SOC_RK3576 OR SOC_RV1126B)
	target_link_libraries(cvr_app rockit)
endif()

target_link_libraries(
			cvr_app rkadk rkaudio rga rkmuxer
			rtsp z freetype rkfsmk drm
			rkdemuxer rkaiq pthread
			)

if(USE_LVGL)
	install(DIRECTORY res/sound DESTINATION share/res/)
	install(DIRECTORY res/conf DESTINATION share/res/)
	install(PROGRAMS res/font/PuHuiTi.ttf DESTINATION share/res/font/)
	if(SOC_RV1106 OR SOC_RV1103B)
		file(GLOB STRING_FILES ${PROJECT_SOURCE_DIR}/res/string/rv1106_1103/*)
		install(FILES ${STRING_FILES} DESTINATION share/res/string/)

		file(GLOB ICON_FILES ${PROJECT_SOURCE_DIR}/res/ui/rv1106_1103/*)
		install(FILES ${ICON_FILES} DESTINATION share/res/ui/)
	elseif(SOC_RV1126 OR SOC_RK3576 OR SOC_RV1126B)
		file(GLOB STRING_FILES ${PROJECT_SOURCE_DIR}/res/string/rv1126_1109/*)
		install(FILES ${STRING_FILES} DESTINATION share/res/string/)

		file(GLOB ICON_FILES ${PROJECT_SOURCE_DIR}/res/ui/rv1126_1109/*)
		install(FILES ${ICON_FILES} DESTINATION share/res/ui/)
	endif()
else()
	install(DIRECTORY res/conf DESTINATION share/res/)
	install(PROGRAMS res/font/PuHuiTi_Tiny.ttf DESTINATION share/res/font/)
endif()

install(TARGETS cvr_app DESTINATION "bin")
# Choose different startup scripts based on USE_RKFASTBOOT option
if(USE_RKFASTBOOT)
    install(PROGRAMS RkLunch_fastboot.sh DESTINATION bin RENAME RkLunch.sh)
else()
    install(PROGRAMS RkLunch-RKCVR_RV1126B.sh DESTINATION bin)
endif()

install(PROGRAMS RkLunch-stop-RKCVR_RV1126B.sh DESTINATION bin)

if(USE_REMOTE_CONFIG)
	add_subdirectory(tools/remote_config)
endif()
add_subdirectory(tools/sound)
add_subdirectory(tools/ntp)

if(USE_RKFASTBOOT)
add_subdirectory(tools/make_param)

add_custom_target(param_img ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/tools/make_param/out/bin
    COMMAND make -C ${CMAKE_CURRENT_SOURCE_DIR}/tools/make_param
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/make_param/make_param
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/make_param/make_param_host
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/make_param/out/bin/
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tools/make_param/out/bin/make_param_host
        --ini_dir ${CMAKE_CURRENT_SOURCE_DIR}/tools/make_param/ini
        --output ${CMAKE_CURRENT_SOURCE_DIR}/tools/make_param/out/bin
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/make_param/out/bin
        ${CMAKE_INSTALL_PREFIX}/bin
    DEPENDS cvr_app
)

# Ensure that the cleaning operation of the make_param directory is executed
add_custom_target(clean_make_param
    COMMAND ${CMAKE_COMMAND} -E echo "Cleaning make_param directory..."
    COMMAND make -C ${CMAKE_CURRENT_SOURCE_DIR}/tools/make_param clean
    COMMAND ${CMAKE_COMMAND} -E echo "Removing additional files..."
    COMMAND ${CMAKE_COMMAND} -E rm -rf
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/make_param/out
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/make_param/make_param
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/make_param/make_param_host
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/make_param/param.img
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/make_param/ini.tar.gz
    COMMAND ${CMAKE_COMMAND} -E echo "Clean make_param completed"
)

# 将 clean_make_param 添加为 clean 目标的依赖
if(TARGET clean)
    add_dependencies(clean clean_make_param)
endif()

set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES
	"${CMAKE_CURRENT_SOURCE_DIR}/tools/make_param/out;
	${CMAKE_CURRENT_SOURCE_DIR}/tools/make_param/make_param;
	${CMAKE_CURRENT_SOURCE_DIR}/tools/make_param/make_param_host;
	${CMAKE_CURRENT_SOURCE_DIR}/tools/make_param/param.img;
	${CMAKE_CURRENT_SOURCE_DIR}/tools/make_param/ini.tar.gz")
endif()
