cmake_minimum_required(VERSION 2.8.0 FATAL_ERROR)
set(CMAKE_CXX_STANDARD 11)
add_definitions(-DDBUG)
add_definitions(-g)
PROJECT(rk_mpi_uvc)
include(FindPkgConfig)

add_definitions(
		-Wall -g -O3 -ggdb -gdwarf -funwind-tables -rdynamic
		-Wno-unused-function
		-Wno-unused-parameter -D_GNU_SOURCE
		)

if (RK_UVC_ENABLE_FASTBOOT)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--allow-multiple-definition")
endif()

if (RK_UVC_ENABLE_EPTZ)
    add_definitions(-DUVC_ENABLE_EPTZ)
    message(STATUS "uvc enable eptz")
endif()

if (RK_UVC_ENABLE_IVA)
    add_definitions(-DUVC_ENABLE_IVA)
    message(STATUS "uvc enable iva")
endif()


include_directories(uvc)
include_directories(uvc/cJSON)
include_directories(isp)
include_directories(param)
include_directories(rockiva)
include_directories(eptz)

include_directories(${PROJECT_SOURCE_DIR}/include
                                        ${CMAKE_SYSROOT}/usr/include/rkaiq/
                                        ${CMAKE_SYSROOT}/usr/include/rkaiq/uAPI
                                        ${CMAKE_SYSROOT}/usr/include/rkaiq/uAPI2
                                        ${CMAKE_SYSROOT}/usr/include/rkaiq/algos
                                        ${CMAKE_SYSROOT}/usr/include/rkaiq/common
                                        ${CMAKE_SYSROOT}/usr/include/rkaiq/xcore
                                        ${CMAKE_SYSROOT}/usr/include/rkaiq/iq_parser
                                        ${CMAKE_SYSROOT}/usr/include/rkaiq/iq_parser_v2
                                        ${CMAKE_SYSROOT}/usr/include/libdrm
                                        )

option(COMPILE_FOR_UVC_UAC "compile for uac" OFF)

# If this mode is enabled, mono audio output will be used.
# You must also modify the following functions in the usb_config.sh configuration file:
# uac1_device_config and uac2_device_config
# The following parameters must be set to 1:
# p_chmask (playback channel mask)
# c_chmask (recording channel mask)
option(RK_UAC_USE_MONO "UAC Mono Mode" OFF)

if (RK_UAC_USE_MONO)
    option(RK_UAC_OPEN_VQE "UAC OPEN VQE" OFF)
endif()

if(COMPILE_FOR_UVC_UAC)
  if(RK_UAC_USE_MONO)
    add_definitions(-DUAC_MONO_MODE)
    if(RK_UAC_OPEN_VQE)
      add_definitions(-DUAC_OPEN_VQE)
    endif()
   endif()
  include(uac/uac.cmake)
endif()

if(${RK_APP_CHIP} STREQUAL "rv1106")
    include_directories(isp/rv1106)
    set(ISP_SOURCE isp/rv1106/isp.c)
    set(UVC_LIBRARY_DIR ${PROJECT_SOURCE_DIR}/lib/rv1106)
    add_definitions(-DUVC_IO_METHOD_USERPTR)
    add_definitions(-DRV1106)

elseif(${RK_APP_CHIP} STREQUAL "rv1103b")
    include_directories(isp/rv1103b)
    set(ISP_SOURCE isp/rv1103b/isp.c)
    set(UVC_LIBRARY_DIR ${PROJECT_SOURCE_DIR}/lib/rv1103b)
    add_definitions(-DRV1103B)
    add_definitions(-DUVC_IO_METHOD_USERPTR)

elseif(${RK_APP_CHIP} STREQUAL "rv1126b")
    include_directories(isp/rv1126b)
    set(UVC_LIBRARY_DIR ${PROJECT_SOURCE_DIR}/lib/rv1126b)
    set(ISP_SOURCE isp/rv1126b/isp.c)
    add_definitions(-DRV1126B)
    add_definitions(-DUVC_IO_METHOD_MMAP)

else()
message(FATAL_ERROR "\n\n!!! FATAL ERROR !!!\n"
                    "Unsupported chip type: ${RK_APP_CHIP}\n"
                    "Supported chip types are: rv1106, rv1103b, rv1126b\n"
                    "Please specify a valid chip type using -DRK_APP_CHIP=<chip>\n")
endif()

set(LIB_SOURCE
    uvc/uvc_process.cpp
    uvc/uvc-gadget.c
    uvc/uvc_video.cpp
    uvc/uvc_control.cpp
    uvc/uvc_process.cpp
    uvc/uvc_mpi_config.c
    uvc/uvc_mpi_venc.cpp
    uvc/uvc_mpi_vi.cpp
    uvc/uvc_mpi_vpss.cpp
    uvc/uevent.c
    uvc/uvc_configfs.c
    uvc/camera_control.c
    uvc/camera_pu_control.c
    uvc/cJSON/cJSON.c
    uvc/osd.c
    param/dictionary.c
    param/iniparser.c
    param/param.c
)

if (RK_UVC_ENABLE_IVA)
    list(APPEND LIB_SOURCE uvc/nn_process.cpp rockiva/rockiva.c)
endif()

if (RK_UVC_ENABLE_EPTZ)
    list(APPEND LIB_SOURCE eptz/eptz.c)
endif()

if(COMPILE_FOR_UVC_UAC)
    add_definitions(-DCOMPILE_FOR_UVC_UAC)
    set(SOURCE
        main.c
        ${ISP_SOURCE}
        ${LIB_SOURCE}
        ${UAC_SOURCE}
    )
else()
    set(SOURCE
        main.c
        ${ISP_SOURCE}
        ${LIB_SOURCE}
    )
endif()

if (RK_UVC_ENABLE_FASTBOOT)
    set(RK_MPI_UVC_DEPENDENT_LIBS
        pthread
        rt
        rkaiq
        librockit.a
        librockchip_mpp.a
        librkaudio_detect.a
        librga.a
    )
    if (RK_UVC_ENABLE_IVA)
        list(APPEND RK_MPI_UVC_DEPENDENT_LIBS librockiva.so)
    endif()
    if (RK_UVC_ENABLE_EPTZ)
        list(APPEND RK_MPI_UVC_DEPENDENT_LIBS libeptz.so)
    endif()
    message(STATUS "RK_UVC_ENABLE_FASTBOOT is enabled.")
else()
    set(RK_MPI_UVC_DEPENDENT_LIBS
        pthread
        rt
        rkaiq
        rockit
    )
    if (RK_UVC_ENABLE_IVA)
        list(APPEND RK_MPI_UVC_DEPENDENT_LIBS rockiva)
    endif()
    if (RK_UVC_ENABLE_EPTZ)
        list(APPEND RK_MPI_UVC_DEPENDENT_LIBS eptz)
    endif()
    message(STATUS "RK_UVC_ENABLE_FASTBOOT is disabled.")
endif()

link_directories(${UVC_LIBRARY_DIR})

add_definitions(-fno-rtti)

ADD_EXECUTABLE(rk_mpi_uvc ${SOURCE})

target_link_libraries(rk_mpi_uvc ${RK_MPI_UVC_DEPENDENT_LIBS})

if (RK_UAC_OPEN_VQE)
    if (RK_UVC_ENABLE_FASTBOOT)
        target_link_libraries(rk_mpi_uvc PRIVATE
            "-Wl,--whole-archive"
            rkaudio_vqe.a
            "-Wl,--no-whole-archive"
            rkaudio_common.a
        )
    else()
        target_link_libraries(rk_mpi_uvc
            rkaudio_vqe
            rkaudio_common
        )
    endif()
endif()

install(DIRECTORY ./uvc DESTINATION include
        FILES_MATCHING PATTERN "*.h")
install(TARGETS rk_mpi_uvc DESTINATION bin)
install(FILES uvc_mpi_cfg.conf DESTINATION share)
install(FILES rkuvc.ini DESTINATION share)
install(FILES usb_config.sh DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_WRITE GROUP_EXECUTE
        WORLD_READ WORLD_WRITE WORLD_EXECUTE)
install(FILES RkLunch-UVC_TINY.sh DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_WRITE GROUP_EXECUTE
        WORLD_READ WORLD_WRITE WORLD_EXECUTE)
install(FILES RkLunch-stop-UVC_TINY.sh DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_WRITE GROUP_EXECUTE
        WORLD_READ WORLD_WRITE WORLD_EXECUTE)

if (RK_UVC_ENABLE_EPTZ)
    install(FILES ${UVC_LIBRARY_DIR}/libeptz.so DESTINATION "lib")
endif()