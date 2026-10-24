# /*********************************************************************************
# * Copyright (C),2020-2030,  Rockchip Beijing Baseband Center
# * All Rights Reserved
# * V1.00
# * FileName : wifi_start.sh
# * Author :  wangqunyang
# * Description:   config user's data and start wifi network
# * History :
# *   <author>      <time>        <version>         <desc>
# *   wangqunyang   2024/03/02      1.0               ORG
# *
# *
# *********************************************************************************/
#!/bin/sh

################################################################################
##   Public Configuraton
################################################################################
cmd=$(realpath "$0")
_DIR=$(dirname "$cmd")
cd "$_DIR" || exit 1
ROOT_DIR="/"
BASE_DIR="userdata"
SUB_DIR="rk96x/conf"

test ! -f "rk96x_env.sh"  || source "rk96x_env.sh"
test ! -f "wifi_debug.sh"  || sh "wifi_debug.sh"

FW_PATH=${RK96X_TARGET_PATH}/fw
WPA_CONF=${RK96X_CONF_PATH}/wpa_supplicant.conf
WIFI_USER_CONF=${RK96X_CONF_PATH}/wifi_user.conf

## default is auto
PARAM_NUM=$#
PARAM_1=$1
WIFI_INIT_CTRL="auto"

## default is needing retry
WIFI_START_RETRY=1

## check parameters nums and $1 value
if [ "$PARAM_NUM" -eq 1 ] && [ "$PARAM_1" == "1" ]; then
    WIFI_INIT_CTRL="manual"
fi

# ---------------------------------------------------
#         check configuration files
# ---------------------------------------------------
if [ ! -f "$WPA_CONF" ]; then
    echo "$WPA_CONF not exist, copy config file with /etc/wpa_supplicant.conf"
    cp -f /etc/wpa_supplicant.conf ${RK96X_CONF_PATH}/wpa_supplicant.conf

    if [ ! $? -eq 0 ];then
        echo "Copy config file failure!"
        exit 1
    fi
fi

if [ ! -f "$WIFI_USER_CONF" ]; then
    echo "$WIFI_USER_CONF not exist."
    echo "Set WIFI_USER_CONF is: null."
    WIFI_USER_CONF=""
#    exit 1
fi

## default printk setting
echo ""
echo "+++++++ Before wifi start, current printk status: +++++++ "
cat /proc/sys/kernel/printk
echo "Reset printk to: 0 "
echo ""
#echo 0 > /proc/sys/kernel/printk

# ---------------------------------------------------
#         get default wpa data
# ---------------------------------------------------
ap_scan=$(sed -n 's/^ap_scan=\(.*\)$/\1/p' $WPA_CONF)
update_config=$(sed -n 's/^update_config=\(.*\)$/\1/p' $WPA_CONF)
ssid=$(sed -n 's/^ssid=\(.*\)$/\1/p' $WPA_CONF)
scan_ssid=$(sed -n 's/^scan_ssid=\(.*\)$/\1/p' $WPA_CONF)
psk=$(sed -n 's/^psk=\(.*\)$/\1/p' $WPA_CONF)
key_mgmt=NONE

# ---------------------------------------------------
#         get default user data
# ---------------------------------------------------
if [ "$RK_WIFI_POWERSAVE_ENABLE" = "y" ]; then
    fw_no_sleep=$(sed -n 's/^fw_no_sleep=\(.*\)$/\1/p' $WIFI_USER_CONF)
else
    fw_no_sleep=1
fi
wifi_start_counter=$(sed -n 's/^wifi_start_counter=\(.*\)$/\1/p' $WIFI_USER_CONF)
wifi_auto_start=$(sed -n 's/^wifi_auto_start=\(.*\)$/\1/p' $WIFI_USER_CONF)
wifi_rf_log=$(sed -n 's/^wifi_rf_log=\(.*\)$/\1/p' $WIFI_USER_CONF)
sdio_width=$(sed -n 's/^sdio_width=\(.*\)$/\1/p' $WIFI_USER_CONF)
linux_kernel_log=$(sed -n 's/^linux_kernel_log=\(.*\)$/\1/p' $WIFI_USER_CONF)
debug_level=$(sed -n 's/^debug_level=\(.*\)$/\1/p' $WIFI_USER_CONF)
local_ip_user=$(sed -n 's/^local_ip_user=\(.*\)$/\1/p' $WIFI_USER_CONF)
local_ip_auto=""
remote_ip_user=$(sed -n 's/^remote_ip_user=\(.*\)$/\1/p' $WIFI_USER_CONF)
broadcast_user=$(sed -n 's/^broadcast_user=\(.*\)$/\1/p' $WIFI_USER_CONF)
broadcast_auto=""
netmask_user=$(sed -n 's/^netmask_user=\(.*\)$/\1/p' $WIFI_USER_CONF)
netmask_auto=""
gate_user=$(sed -n 's/^gate_user=\(.*\)$/\1/p' $WIFI_USER_CONF)
gate_auto=""

## set default if get null value
if [ -z "$wifi_start_counter" ]; then
    echo "Get [wifi_start_counter] null, set default value: 0"
    wifi_start_counter=0
fi

if [ -z "$wifi_auto_start" ]; then
    echo "Get [wifi_auto_start] null, set default value: 1"
    wifi_auto_start=1
fi

if [ -z "$sdio_width" ]; then
    echo "Get [sdio_width] null, set default value: 1"
    sdio_width=1
fi

if [ -z "$fw_no_sleep" ]; then
if [ "$RK_WIFI_POWERSAVE_ENABLE" = "y" ]; then
    fw_no_sleep=0
else
    fw_no_sleep=1
fi
    echo "Get [fw_no_sleep] null, set default value: $fw_no_sleep"
fi

if [ -z "$debug_level" ]; then
    echo "Get [debug_level] null, set default value: 0x6"
    debug_level=0x6
fi

## check wifi statr need retry 2 times or not
## 0, 1, new image, user not set specified ssid and psk, not need to retry
## >=2, need retry when connect AP unsuccessful
if [ $wifi_start_counter -lt 2 ]; then
    WIFI_START_RETRY=0
#    echo " "
#    echo "--- This time $wifi_start_counter: NOT retry to start wifi when connect AP unsuccessful ---"
    let "wifi_start_counter=wifi_start_counter+1"

    # update counter
    if [ -n "$WIFI_USER_CONF" ]; then
        sed -i "s/^.*wifi_start_counter=.*/wifi_start_counter=${wifi_start_counter}/" $WIFI_USER_CONF
    fi
fi

# ---------------------------------------------------
#         wifi: copyright info display
# ---------------------------------------------------
function wifi_copyright_info(){

    # Default use 962 with V2
    # V1V2 BT MCU Offset
    # rk96x_bt_mcu  212 version (old)
    # rk96x_bt_mcu  68710(0x10c66) date (now)

    # V2 Offset
    # xxx_bt_rf_v2.bin  192
    # xxx_bt_v2.bin  196
    # xxx_wifi_rf_v2.bin  224
    # xxx_wifi_v2.bin  9060

    # V1 Offset
    # xxx_bt_rf.bin  320
    # xxx_bt.bin  14936
    # xxx_wifi_rf.bin  320
    # xxx_wifi.bin 8952

    # ---------------------------------------------------
    #         get default version info
    #   962 v2: version value (only for rk962 v2)
    #   Others: 00000
    # ---------------------------------------------------
    bt_mcu_version=00000
    bt_rf_version=00000
    bt_version=00000
    wifi_rf_version=00000
    wifi_version=00000

    # read offset+1 value until with 0x20, blank space
    test ! -f "$FW_PATH/rk962_bt_mcu.bin" || bt_mcu_version=$(tail -c +68711 "$FW_PATH/rk962_bt_mcu.bin" | tr '\x20' '\n' | head -n1)

    # read offset+1 value until with 0x0
    test ! -f "$FW_PATH/rk962_bt_rf_v2.bin" || bt_rf_version=$(tail -c +193 "$FW_PATH/rk962_bt_rf_v2.bin" | tr '\0' '\n' | head -n1)

    # read offset+1 value until with 0x0
    test ! -f "$FW_PATH/rk962_bt_v2.bin" || bt_version=$(tail -c +197 "$FW_PATH/rk962_bt_v2.bin" | tr '\0' '\n' | head -n1)

    # read offset+1 value until with 0x0
    test ! -f "$FW_PATH/rk962_wifi_rf_v2.bin" || wifi_rf_version=$(tail -c +225 "$FW_PATH/rk962_wifi_rf_v2.bin" | tr '\0' '\n' | head -n1)

    # read offset+1 value until with 0x0
    test ! -f "$FW_PATH/rk962_wifi_v2.bin" || wifi_version=$(tail -c +9061 "$FW_PATH/rk962_wifi_v2.bin" | tr '\0' '\n' | head -n1)

    # read tail date info
    host_version=$(cat /proc/version | awk '{print $(NF-4)" "$(NF-3)" "$(NF-2)" "$(NF-1)" "$NF}')

    # discard null char in the tail
    host_version=${host_version%%$'\n'}

    echo " "
    echo " - -       |    /"
    echo "|    |	   |   /"
    echo "|    |     |  /"
    echo "|- -       |  \\"
    echo "|    \\     |   \\"
    echo "|     \\    |    \\"
    echo " "
    echo "All rights reserved. @2024. Rockchip Electronics Co., Ltd."
    echo " "
    echo "BT_MCU: ${bt_mcu_version}"
    echo "WIFI: rk96x_svn${wifi_version}, WIFI_RF: rk96x_svn${wifi_rf_version}"
    echo "BT: rk96x_svn${bt_version}, BT_RF: rk96x_svn${bt_rf_version}"
    echo "HOST: ${host_version}"
    echo " "
    echo "Try 'cmd' for abbreviation commands list or 'help' for Built-in commands."
    echo " "
}

# ---------------------------------------------------
#         update_wifi_conf_content
# ---------------------------------------------------
function update_wifi_conf_content(){

    #01: ap_scan
    echo "+++: *UPDATE* ap_scan? y/Y=yes, others=no:"
    read need
    case $need in
        y|Y)
            echo "Input new [ ap_scan ]:"
            read data
            ap_scan=$data

            # replace data into file
            sed -i "s/^.*ap_scan=.*/ap_scan=${data}/" $WPA_CONF
            ;;
        n|N|*)
            # read data from conf file
            ap_scan=$(sed -n 's/^ap_scan=\(.*\)$/\1/p' $WPA_CONF)
            echo "get current ap_scan: "$ap_scan
            echo " "
            ;;
    esac

    #02: update_config
    echo "+++: *UPDATE* update_config? y/Y=yes, others=no: "
    read need
    case $need in
        y|Y)
            echo "Input new [ update_config ]:"
            read data
            update_config=$data

            # replace data into file
            sed -i "s/^.*update_config=.*/update_config=${data}/" $WPA_CONF
            ;;
        n|N|*)
            # read data from conf file
            update_config=$(sed -n 's/^update_config=\(.*\)$/\1/p' $WPA_CONF)
            echo "get current update_config: "$update_config
            echo " "
            ;;
    esac

    #03: ssid
    echo "+++: *UPDATE* ssid? y/Y=yes, others=no: "
    read need
    case $need in
        y|Y)
            echo "Input new [ ssid ]:"
            read data
            ssid=$data

            # replace data into file
            sed -i "s/^.*ssid=\".*/    ssid=\"${data}\"/" $WPA_CONF
            ;;
        n|N|*)
            # read data from conf file
            ssid=$(sed -n 's/^    ssid=\(.*\)$/\1/p' $WPA_CONF)
            echo "get current ssid: "$ssid
            echo " "
            ;;
    esac

    #04: scan_ssid
    echo "+++: *UPDATE* scan_ssid? y/Y=yes, others=no: "
    read need
    case $need in
        y|Y)
            echo "Input new [ scan_ssid ]:"
            read data
            scan_ssid=$data

            # replace data into file
            sed -i "s/^.*scan_ssid=.*/    scan_ssid=${data}/" $WPA_CONF
            ;;
        n|N|*)
            # read data from conf file
            scan_ssid=$(sed -n 's/^    scan_ssid=\(.*\)$/\1/p' $WPA_CONF)
            echo "get current scan_ssid: "$scan_ssid
            echo " "
            ;;
    esac

    #05: psk/key_mgmt
    echo "+++: *UPDATE* psk/key_mgmt? y/Y=yes, others=no: "
    read need
    case $need in
        y|Y)
            echo "Input new [ psk/key_mgmt(only value: NONE) ]:"
            read data

            case "$data" in
              NONE|none|None)
                    # replace data into file
                    sed -i "s/^.*psk=\".*/    key_mgmt=NONE/" $WPA_CONF

                ;;
              *)
                    psk=$data

                    # replace data into file
                    result=`sed -i "s/^.*psk=\".*/    psk=\"${data}\"/" $WPA_CONF`

                    if [ -z "$result" ];then
                        sed -i "s/^.*key_mgmt=.*/    psk=\"${data}\"/" $WPA_CONF
                    fi

                ;;
            esac
            ;;
        n|N|*)
            # read data from conf file
            psk=$(sed -n 's/^    psk=\(.*\)$/\1/p' $WPA_CONF)
            if [ -n "$psk" ]; then
                echo "get current psk: "$psk
                echo " "
            else
                echo "get current key_mgmt: NONE"
                echo " "
            fi
            ;;
    esac

    #06: sdio_width
    echo "+++: *UPDATE* sdio_width? y/Y=yes, others=no: "
    read need
    case $need in
        y|Y)
            echo "Input new [ sdio_width (1,4) ]:"
            read data
            sdio_width=$data

            # replace data into file
            if [ "$data" == "1" ] || [ "$data" == "4" ]; then
                sed -i "s/^.*sdio_width=.*/sdio_width=${data}/" $WIFI_USER_CONF
            fi
            ;;
        n|N|*)
            # read data from conf file
            sdio_width=$(sed -n 's/^sdio_width=\(.*\)$/\1/p' $WIFI_USER_CONF)
            echo "get current sdio_width: "$sdio_width
            echo " "
            ;;
    esac
    
    #06-1: fw_no_sleep
    echo "+++: *UPDATE* fw_no_sleep? y/Y=yes, others=no: "
    read need
    case $need in
        y|Y)
            echo "Input new [ fw_no_sleep (0,1) ]:"
            read data
            fw_no_sleep=$data

            # replace data into file
            if [ "$data" == "0" ] || [ "$data" == "1" ]; then
                sed -i "s/^.*fw_no_sleep=.*/fw_no_sleep=${data}/" $WIFI_USER_CONF
            fi
            ;;
        n|N|*)
            # read data from conf file
            fw_no_sleep=$(sed -n 's/^fw_no_sleep=\(.*\)$/\1/p' $WIFI_USER_CONF)
            echo "get current fw_no_sleep: "$fw_no_sleep
            echo " "
            ;;
    esac

    #07: local_ip_user
    echo "[Option]: *UPDATE* local_ip_user? y/Y=yes, others=no: "
    read need
    case $need in
        y|Y)
            echo "Input new [ local_ip_user ]:"
            read data
            local_ip_user=$data

            # replace data into file
            sed -i "s/^.*local_ip_user=\".*/local_ip_user=\"${data}\"/" $WIFI_USER_CONF
          ;;
        n|N|*)
            # read data from conf file
            local_ip_user=$(sed -n 's/^local_ip_user=\(.*\)$/\1/p' $WIFI_USER_CONF)
            echo "get current local_ip_user: "$local_ip_user
            echo " "
            ;;
    esac

    #08: remote_ip_user
    echo "[Option]: *UPDATE* remote_ip_user? y/Y=yes, others=no: "
    read need
    case $need in
        y|Y)
            echo "Input new [ remote_ip_user ]:"
            read data
            remote_ip_user=$data
            # replace data into file
            sed -i "s/^.*remote_ip_user=\".*/remote_ip_user=\"${data}\"/" $WIFI_USER_CONF
            ;;
        n|N|*)
            # read data from conf file
            remote_ip_user=$(sed -n 's/^remote_ip_user=\(.*\)$/\1/p' $WIFI_USER_CONF)
            echo "get current remote_ip_user: "$remote_ip_user
            echo " "
            ;;
    esac

    #09: broadcast_user
    echo "[Option]: *UPDATE* broadcast_user? y/Y=yes, others=no: "
    read need
    case $need in
        y|Y)
            echo "Input new [ broadcast_user ]:"
            read data
            broadcast_user=$data
            # replace data into file
            sed -i "s/^.*broadcast_user=\".*/broadcast_user=\"${data}\"/" $WIFI_USER_CONF
            ;;
        n|N|*)
            # read data from conf file
            broadcast_user=$(sed -n 's/^broadcast_user=\(.*\)$/\1/p' $WIFI_USER_CONF)
            echo "get current broadcast_user: "$broadcast_user
            echo " "
            ;;
    esac

    #10: netmask_user
    echo "[Option]: *UPDATE* netmask_user? y/Y=yes, others=no: "
    read need
    case $need in
        y|Y)
            echo "Input new [ netmask_user ]:"
            read data
            netmask_user=$data
            # replace data into file
            sed -i "s/^.*netmask_user=\".*/netmask_user=\"${data}\"/" $WIFI_USER_CONF
            ;;
        n|N|*)
            # read data from conf file
            netmask_user=$(sed -n 's/^netmask_user=\(.*\)$/\1/p' $WIFI_USER_CONF)
            echo "get current netmask_user: "$netmask_user
            echo " "
            ;;
    esac

    #11: gate_user
    echo "[Option]: *UPDATE* gate_user? y/Y=yes, others=no: "
    read need
    case $need in
        y|Y)
            echo "Input new [ gate_user ]:"
            read data
            gate_user=$data
            # replace data into file
            echo "gate: $gate_user"
            echo "data: $data"
            sed -i "s/^.*gate_user=\".*/gate_user=\"${data}\"/" $WIFI_USER_CONF
            ;;
        n|N|*)
            # read data from conf file
            gate_user=$(sed -n 's/^gate_user=\(.*\)$/\1/p' $WIFI_USER_CONF)
            echo "get current gate_user: "$gate_user
            echo " "
            ;;
    esac
}

# ---------------------------------------------------
#         start_wifi retry 3times
#  param1=1: manual
#  param1=2,3,4:  auto retry 1-3 time, recursion call
#  param1=5: normal start, no recursion call
# ---------------------------------------------------
function retry_start_wifi_network(){

    if [ "$PARAM_NUM" -eq 1 ]; then
        # retry finished
        if [ $PARAM_1 -le 2 ]; then
            echo "*** Retry re-connect to AP, reach to max retry times: 3 !!!"
            echo " "
        else
            # retry times
            echo "--- Retry re-connect wifi network ---"
            let "PARAM_1=PARAM_1-1"
            let "RETRY_INDEX=5-PARAM_1"
            echo "---> Current shell param1: $PARAM_1, retry index: $RETRY_INDEX -->"
            echo " "
            wifi_start.sh $PARAM_1
        fi
    else
        echo "*** Parameter number of 'wifi_start.sh' NOT 1"
        echo " "
    fi
}
# ---------------------------------------------------
#         start_wifi_network
# ---------------------------------------------------
function start_wifi_network(){
    echo "Start wifi network ... "    

    if [ "$linux_kernel_log" == "7" ]; then
        echo "Open linux kernel log : "
        echo 7 > /proc/sys/kernel/printk
    fi

    function load_kernel_module() {
        local module_name=$1
        local module_path="/oem/usr/ko/${module_name}.ko"
        
        if [ -z "$( lsmod | grep ${module_name} )" ]; then
            if [ ! -f "${module_path}" ]; then
                return
            fi
            echo "***[->] insmod ${module_path}"
            insmod ${module_path}
            if [ ! $? -eq 0 ]; then
                echo "*** Exec insmod ${module_name}.ko command failure!"
                exit 1
            fi
        fi
    }

    cat /sys/bus/sdio/devices/*/uevent | grep "0296\:5349"
    if [ ! $? -eq 0 ];then
        echo "*** ERROR: SDIO driver is not ready!"
        exit 1
    fi

    load_kernel_module "cfg80211"
    load_kernel_module "libarc4"
    load_kernel_module "mac80211"
    load_kernel_module "ctr"
    load_kernel_module "ccm"
    load_kernel_module "libaes"
    load_kernel_module "aes_generic"
    load_kernel_module "ecc"
    load_kernel_module "ecdh_generic"
    load_kernel_module "crc16"
    load_kernel_module "bluetooth"
    load_kernel_module "hci_uart"

    # insmod rk96x driver by chip type
    if [ -z "$( lsmod | grep rk96x )" ]; then
        # Execute insmod
        cmd="insmod /oem/usr/ko/rk96x.ko sdio_width=$sdio_width fw_no_sleep=$fw_no_sleep debug_level=$debug_level"
        echo "***[->]" $cmd
        eval "$cmd"
        if [ ! $? -eq 0 ];then
            echo "*** Exec insmod rk96x.ko command failure!"
            exit 1
        fi
    #    sleep 0.1
    fi

    # kill wpa_supplicant
    if [ -n "$( ps | grep wpa_supplicant | grep -v grep )" ]; then
        echo "kill wpa_supplicant --> "
        killall -9 wpa_supplicant
        wait $!
        if [ ! $? -eq 0 ];then
            echo "*** Kill wpa_supplicant program failure!"
            exit 1
        fi
        sleep 0.3
    fi

    # kill udhcpc 001
    if [ -n "$( ps | grep udhcpc | grep -v grep )" ]; then
        echo "Init: kill udhcpc process by -15"
        killall -15 udhcpc
        wait $!
        if [ ! $? -eq 0 ];then
            echo "*** Init: kill udhcpc program failure by -15!"
            exit 1
        fi
        sleep 0.3

        # kill udhcpc 001A
        if [ -n "$( ps | grep udhcpc | grep -v grep )" ]; then
            echo "Init: kill udhcpc process by -9"
            killall -9 udhcpc
            wait $!
            if [ ! $? -eq 0 ];then
                echo "*** Init: kill udhcpc program failure by -9!"
                exit 1
            fi
            sleep 0.3
        fi
    fi

    # kill rkwifi_server
    if [ -n "$( ps | grep rkwifi_server | grep -v grep )" ]; then
        echo "kill rkwifi_server --> "
        killall -9 rkwifi_server
        wait $!
        if [ ! $? -eq 0 ];then
            echo "*** Kill rkwifi_server program failure!"
            exit 1
        fi
        sleep 0.3
    fi

    ############################################
    # process with manual start mode
    ############################################
    if [ "$WIFI_INIT_CTRL" == "manual" ]; then

        # waiting for ko insmod finish
        wait $!
        sleep 1

        # start wpa_supplicant
        if [ -z "$( ps | grep wpa_supplicant | grep -v grep )" ]; then
            echo "wpa_supplicant start --> "

            if [ -f "/data/cfg/wpa_supplicant.conf" ]; then
                echo "wpa_supplicant -B -i wlan0 -c /data/cfg/wpa_supplicant.conf -ddd"
                wpa_supplicant -B -i wlan0 -c /data/cfg/wpa_supplicant.conf -ddd
                wait $!
            else
                echo "wpa_supplicant -B -i wlan0 -c ${RK96X_CONF_PATH}/wpa_supplicant.conf -ddd"
                wpa_supplicant -B -i wlan0 -c ${RK96X_CONF_PATH}/wpa_supplicant.conf -ddd
                wait $!
            fi

            if [ ! $? -eq 0 ];then
                echo " "
                echo "======================================="
                echo "***: Start wpa_supplicant program failure!"
                echo "============ wlan not start ============"
                echo " "
                exit 1
            fi

            sleep 1
        fi

        # start udhcpc 001
        if [ -z "$( ps | grep udhcpc | grep -v grep )" ]; then
            echo "udhcpc start --> "

            udhcpc -i wlan0 -T 1 -A 0 -b -q
            wait $!

            if [ ! $? -eq 0 ];then
                echo "*** Start udhcpc program failure!"
                exit 1
            fi

            # wait for 3s
            if [ -z "$( ifconfig -a | grep "inet addr" )" ]; then
                sleep 0.4
                if [ -z "$( ifconfig -a | grep "inet addr" )" ]; then
                    sleep 0.6
                    if [ -z "$( ifconfig -a | grep "inet addr" )" ]; then
                        sleep 0.8
                        if [ -z "$( ifconfig -a | grep "inet addr" )" ]; then
                            sleep 1.2
                        fi
                    fi
                fi
            fi

        fi

    ############################################
    # process with auto start mode
    ############################################
    else
        # start rkwifi_server
        if [ -z "$( ps | grep rkwifi_server | grep -v grep )" ]; then
            echo "rkwifi_server starting ... "

            rkwifi_server start &

            # Just return here because checking is done in rkwifi_server
            exit 0

            if [ ! $? -eq 0 ];then
                echo " "
                echo "======================================="
                echo "***: Start rkwifi_server program failure!"
                echo "============ wlan not start ============"
                echo " "
                exit 1
            fi

            # waiting for 6s rkwifi_server finished
            if [ -z "$( ifconfig -a | grep "inet addr" )" ]; then
                sleep 0.5
                if [ -z "$( ifconfig -a | grep "inet addr" )" ]; then
                    sleep 0.7
                    if [ -z "$( ifconfig -a | grep "inet addr" )" ]; then
                        sleep 0.9
                        if [ -z "$( ifconfig -a | grep "inet addr" )" ]; then
                            sleep 1.1
                            if [ -z "$( ifconfig -a | grep "inet addr" )" ]; then
                                sleep 1.3
                                if [ -z "$( ifconfig -a | grep "inet addr" )" ]; then
                                    sleep 1.5
                                fi
                            fi
                        fi
                    fi
                fi
            fi

        fi

    fi

    # Check network by wait 3s
    if [ -z "$( ifconfig -a | grep "inet addr" )" ]; then
        sleep 0.4
        if [ -z "$( ifconfig -a | grep "inet addr" )" ]; then
            sleep 0.6
            if [ -z "$( ifconfig -a | grep "inet addr" )" ]; then
                sleep 0.8
                if [ -z "$( ifconfig -a | grep "inet addr" )" ]; then
                    sleep 1.2
                fi
            fi
        fi
    fi

    echo " "
    echo "+++: Check network status ===> "
    wpa_cli status &
    wait $!
    echo " "

    if [ -z "$( ifconfig -a | grep "inet addr" )" ]; then
        echo "Ifconfig null, retry, please wait ..."
        echo " "
        echo "udhcpc start once more --> udhcpc -i wlan0 -T 1 -A 0 -b -q"
        udhcpc -i wlan0 -T 1 -A 0 -b -q -n
        wait $!

        if [ -z "$( ifconfig -a | grep "inet addr" )" ]; then
            sleep 0.4
            if [ -z "$( ifconfig -a | grep "inet addr" )" ]; then
                sleep 0.6
                if [ -z "$( ifconfig -a | grep "inet addr" )" ]; then
                    sleep 0.8
                    if [ -z "$( ifconfig -a | grep "inet addr" )" ]; then
                        sleep 1.2
                    fi
                fi
            fi
        fi

    fi

    # ifconfig have ip address
    if [ -n "$( ifconfig -a | grep "inet addr" )" ]; then
        echo "ifconfig -a output: "$( ifconfig -a | grep "inet addr" )
        echo " "

        data12=$(ifconfig -a | grep -E -m 1 'inet ' | grep -v '127.0.0.1' | awk '{print $2}')
        data13=$(ifconfig -a | grep -E -m 1 'inet ' | grep -v '127.0.0.1' | awk '{print $3}')
        data14=$(ifconfig -a | grep -E -m 1 'inet ' | grep -v '127.0.0.1' | awk '{print $4}')
        local_ip_auto=${data12##*:}
        broadcast_auto=${data13##*:}
        netmask_auto=${data14##*:}

        getdata=$(cat /proc/net/route | awk '{if ($2=="00000000") print $3}')

        gate4=${getdata:0:2}
        gate3=${getdata:2:2}
        gate2=${getdata:4:2}
        gate1=${getdata:6:2}
        int1=$(printf "%d" 0x$gate1)
        int2=$(printf "%d" 0x$gate2)
        int3=$(printf "%d" 0x$gate3)
        int4=$(printf "%d" 0x$gate4)
        gate_auto=$int1.$int2.$int3.$int4

        if [ "${local_ip_user//\"/}" != "${local_ip_auto}" ] && [ "${local_ip_auto}" != "0.0.0.0" ]; then
        echo "*UPDATE* local_ip_user replace by local_ip_auto"

            local_ip_user=$local_ip_auto

            # replace data into file
            if [ -n "$WIFI_USER_CONF" ]; then
                sed -i "s/^.*local_ip_user=\".*/local_ip_user=\"${local_ip_user}\"/" $WIFI_USER_CONF
            fi
        fi

        if [ "${broadcast_user//\"}" != "${broadcast_auto}" ] && [ "${broadcast_auto}" != "0.0.0.0" ]; then
          echo "*UPDATE* broadcast_user replace by broadcast_auto"
            broadcast_user=$broadcast_auto

            # replace data into file
            if [ -n "$WIFI_USER_CONF" ]; then
                sed -i "s/^.*broadcast_user=\".*/broadcast_user=\"${broadcast_user}\"/" $WIFI_USER_CONF
            fi
        fi

        if [ "${netmask_user//\"/}" != "${netmask_auto}" ] && [ "${netmask_auto}" != "0.0.0.0" ]; then
            netmask_user=$netmask_auto

            # replace data into file
            if [ -n "$WIFI_USER_CONF" ]; then
                sed -i "s/^.*netmask_user=\".*/netmask_user=\"${netmask_user}\"/" $WIFI_USER_CONF
            fi
        fi

        if [ "${gate_user//\"/}" != "${gate_auto}" ] && [ "${gate_auto}" != "0.0.0.0" ]; then
           echo "*UPDATE* gate_user replace by gate_auto"
            echo " "
            gate_user=$gate_auto

            # replace data into file
            if [ -n "$WIFI_USER_CONF" ]; then
                sed -i "s/^.*gate_user=\".*/gate_user=${gate_user}/" $WIFI_USER_CONF
            fi
        fi

    else
        echo " "
        echo "Wait 2s when ip null and check again ..."

        if [ -z "$( ifconfig -a | grep "inet addr" )" ]; then
            sleep 0.2
            if [ -z "$( ifconfig -a | grep "inet addr" )" ]; then
                sleep 0.4
                if [ -z "$( ifconfig -a | grep "inet addr" )" ]; then
                    sleep 0.6
                    if [ -z "$( ifconfig -a | grep "inet addr" )" ]; then
                        sleep 0.8
                    fi
                fi
            fi
        fi
    fi

    if [ -z "$( ifconfig -a | grep "inet addr" )" ]; then
        echo " "
        echo "======================================="
        echo "***: Fetch ip address is still NULL."
        echo "***: Please check:"
        echo "[1]: evb power off and restart?"
        echo "[2]: ssid/psk right?"
        echo "[3]: netmask/gateway valid?"
        echo "[4]: other cause?"
        echo "======================================="
        echo "Connect wifi network failure ***"
        echo " "

    # retry re-connect AP by 3 times
        if [ "$WIFI_INIT_CTRL" == "auto" ] && [ "$WIFI_START_RETRY" -eq 1 ] ; then
            retry_start_wifi_network
        fi

    #    wifi_copyright_info
        exit 0
    fi

    ip_raw_data=$(ifconfig -a | grep -E -m 1 'inet ' | grep -v '127.0.0.1' | awk '{print $2}')
    current_inet_addr=${ip_raw_data##*:}
    echo "Current_inet_addr=$current_inet_addr"
    echo " "

    echo "Connect wifi network successful!"
    echo "You can try 'wcs' check it!"
    echo " "

#    wifi_copyright_info
    exit 0
}

# ---------------------------------------------------
#  wifi start mode
# ---------------------------------------------------
echo "+++++++ WIFI bootup +++++++"
echo "System bootup, WIFI auto-start default."
echo "[1] Before start, 'rkkp 2' set wifi not auto-start"
echo "[2] Manual mode stage, confirm with input: y or Y "
echo "+++++++++++++++++++++++++++"
ulimit -a
wait $!

if [ "$WIFI_INIT_CTRL" == "manual" ]; then
    echo "Input: y/Y to start with manual -> "
    read userctrl

    if [ "$userctrl" == "y" ] || [ "$userctrl" == "Y" ]; then
        echo "--- WIFI Init By Manual Ctrl ---"
    else
        echo "### WIFI bootup canceled by user"
        exit 0
    fi
else
    echo "--- WIFI Init By Auto Ctrl ---"
fi

if [ "$wifi_auto_start" == 0 ] && [ "$WIFI_INIT_CTRL" == "auto" ] ; then
    echo "--- WIFI Not Start: wifi_auto_start == 0 ----"
    exit 0
fi

# ---------------------------------------------------
#  main: get the user's choice to update config or not
# ---------------------------------------------------

choice=n

if [ "$WIFI_INIT_CTRL" == "manual" ]; then
    echo "+++ wifi config data content: ===>"
    cat $WPA_CONF
    cat $WIFI_USER_CONF
    echo "+++: *UPDATE* config file's data? y/Y=yes, others=no: "
    read choice
fi

case $choice in
    y|Y) update_wifi_conf_content;;
    n|N|*) start_wifi_network;;
esac

# ---------------------------------------------------
#  re-config user's data or not
# ---------------------------------------------------
# echo "+++++++++++++++++++++++++++++++++++++++"
echo "+++ wifi re-config data content: ===>"
cat $WPA_CONF
cat $WIFI_USER_CONF

echo "+++: Is ok? data need *RE-CONFIG*? y/Y=yes, others=no ===>"
read select
case $select in
    y|Y) update_wifi_conf_content;;
    n|N|*) start_wifi_network;;
esac

echo "+++++++++++++++++++++++++++++++++++++++"
echo "+++ wpa config data content: ===>"
echo "+++++++++++++++++++++++++++++++++++++++"
cat $WPA_CONF
cat $WIFI_USER_CONF

echo "+++: Is ok? data need *RE-CONFIG*? y/Y=yes, others=no ===>"
read select
case $select in
    y|Y)
        echo    "*** Modified data more than 2 times, please check your operation."
        echo    "*** Start wifi network failure, please restart wifi_start.sh!"
        exit 0
        ;;
    n|N|*)
        start_wifi_network
        ;;
esac
