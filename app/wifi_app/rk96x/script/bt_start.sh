# /*********************************************************************************
# * Copyright (C),2020-2030,  Rockchip Beijing Bansband Center
# * All Rights Reserved
# * V1.00
# * FileName : bt_start.sh
# * Author :  wangqunyang
# * Description:   config user's data and start bt service
# * History :
# *   <author>      <time>        <version>         <desc>
# *   wangqunyang   2024/03/27      1.0               ORG
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
#ROOT_DIR="/"
#BASE_DIR="userdata"
#SUB_DIR="rk96x/conf"

test ! -f "rk96x_env.sh"  || source "rk96x_env.sh"

#RK96X_CONF_PATH="${ROOT_DIR}${BASE_DIR}/${SUB_DIR}"
#export RK96X_CONF_PATH
FW_PATH=${RK96X_TARGET_PATH}/fw
#export RK96X_TARGET_PATH="$FW_PATH/.."
BT_USER_CONF=${RK96X_CONF_PATH}/bt_user.conf
dev_path=$RK_WIFI_BTMCU_TTYS

# ---------------------------------------------------
#         check configuration files
# ---------------------------------------------------
if [ ! -f "$BT_USER_CONF" ]; then
	echo "$BT_USER_CONF not exist."
	echo "Set BT_USER_CONF is: null."
	BT_USER_CONF=""
#	exit 1
fi

# ---------------------------------------------------
#         get default user data
# ---------------------------------------------------
bt_auto_start=$(sed -n 's/^bt_auto_start=\(.*\)$/\1/p' $BT_USER_CONF)
bt_rf_log=$(sed -n 's/^bt_rf_log=\(.*\)$/\1/p' $BT_USER_CONF)
mskrm_dl_loader_baud=$(sed -n 's/^mskrm_dl_loader_baud=\(.*\)$/\1/p' $BT_USER_CONF)
loader_dl_fw_hciserv_baud=$(sed -n 's/^loader_dl_fw_hciserv_baud=\(.*\)$/\1/p' $BT_USER_CONF)
bt_vendor=$(sed -n 's/^bt_vendor=\(.*\)$/\1/p' $BT_USER_CONF)
flow_ctrl=$(sed -n 's/^flow_ctrl=\(.*\)$/\1/p' $BT_USER_CONF)
lpc_ctrl=$(sed -n 's/^lpc_ctrl=\(.*\)$/\1/p' $BT_USER_CONF)
bt_addr=$(sed -n 's/^bt_addr=\(.*\)$/\1/p' $BT_USER_CONF)


## set default if get null value
if [ -z "$bt_auto_start" ]; then
    echo "Get [bt_auto_start] null, set default value: 1"
    bt_auto_start=1
fi

if [ -z "$mskrm_dl_loader_baud" ]; then
    echo "Get [mskrm_dl_loader_baud] null, set default value: 115200"
    mskrm_dl_loader_baud=115200
fi

if [ -z "$loader_dl_fw_hciserv_baud" ]; then
    echo "Get [loader_dl_fw_hciserv_baud] null, set default value: 3000000"
    loader_dl_fw_hciserv_baud=3000000
fi

if [ -z "$dev_path" ]; then
    echo "ERROR: not 'dev_path' found."
    exit 1
fi

if [ -z "$bt_vendor" ]; then
    echo "Get [bt_vendor] null, set default value: rockchip"
    bt_vendor="rockchip"
fi

if [ -z "$flow_ctrl" ]; then
    echo "Get [flow_ctrl] null, set default value: flow"
    flow_ctrl="flow"
fi

if [ -z "$lpc_ctrl" ]; then
    echo "Get [lpc_ctrl] null, set default value: nosleep"
    lpc_ctrl="nosleep"
fi

if [ -z "$bt_addr" ]; then
    echo "Get [bt_addr] null, set default value: 55:66:77:88:99:aa"
    bt_addr=55:66:77:88:99:aa
fi

################################################################################
##   01: bt init called by bt test or ble app [ special scene ]
################################################################################
if [ "$#" -eq 1 ] && [ "$1" == "2" ]; then
	echo "--------------- bt conifg network init ----------------"
	mkdir -p /var/run/dbus
	dbus-daemon --config-file=/oem/usr/ko/system.conf
	# rk_hciattach -n -s 115200 -m 1  ${dev_path//\"/} rockchip 115200 flow nosleep 55:66:77:88:99:aa &
	rk_hciattach -n -s 115200 -m 1 $dev_path rockchip 3000000 flow nosleep 55:66:77:88:99:aa &
	bluetoothd -n -d -P Battery &
	exit 0
fi

################################################################################
##   02: bt start called by system bootup [ special scene ]
################################################################################

## default is auto
BT_INIT_CTRL="auto"

if [ "$#" -eq 1 ] && [ "$1" == "1" ]; then
	BT_INIT_CTRL="manual"
fi

# ---------------------------------------------------
#         bt: copyright info display
# ---------------------------------------------------
function bt_copyright_info(){

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
#    bt_mcu close charge control and host
# ---------------------------------------------------
function close_charge_control_and_host(){

# -----------------------------------
# 001
# -----------------------------------
	echo "Bt mcu stop charge control ... "

# wake up bt
	echo -e -n "\xFF\xFF\x01\x00" > $dev_path

# delay 100ms
	sleep 0.1

# close charge
	echo -e -n "\xFF\xFF\x02\x14\x00" > $dev_path

	echo "Stop charge control finish!"
	echo " "

# -----------------------------------
# 002
# -----------------------------------
# 	echo "Bt mcu close host ... "

# # wake up bt
# 	echo -e -n "\xFF\xFF\x01\x00">$dev_path

# # delay 100ms
# 	sleep 0.1

# # close soc_pwr_en
# 	echo -e -n "\xFF\xFF\x01\x12">$dev_path

# 	echo "Close host finish!"

}

# ---------------------------------------------------
#         Start btmcu_daemon app
# ---------------------------------------------------
function start_btmcu_daemon_app(){

	echo "+++++++++++++++++++++++++++++++++++++++"
	echo "Start btmcu_daemon app ..."
	echo "+++++++++++++++++++++++++++++++++++++++"
	if [ -z "$( ps | grep "btmcu_daemon" | grep -v "grep" )" ]; then
		btmcu_daemon start $dev_path &
	#	wait $!
		sleep 1.5

		if [ ! $? -eq 0 ];then
			echo "*** Exec btmcu_daemon program failure!"
			exit 1
		fi

	#	ps | grep "btmcu_daemon" | grep -v "grep"
	else
		echo "+++ btmcu_daemon app always running!"
	fi

}

# ---------------------------------------------------
#         Start emode_server app testing
# ---------------------------------------------------
function start_emode_server_app(){

	echo "+++++++++++++++++++++++++++++++++++++++"
	echo "Start emode_server app ..."
	echo "+++++++++++++++++++++++++++++++++++++++"
	if [ -z "$( ps | grep "emode_server" | grep -v "grep" )" ]; then
		/usr/bin/emode_server start &
	#	wait $!
		sleep 1.5

		if [ ! $? -eq 0 ];then
			echo "*** Exec emode_server program failure!"
			exit 1
		fi

	#	ps | grep "emode_server" | grep -v "grep"
	else
		echo "+++ emode_server app always running!"
	fi

}

# ---------------------------------------------------
#         update_bt_conf_content
# ---------------------------------------------------
function update_bt_conf_content(){

	#01: mskrm_dl_loader_baud
	echo "+++: *UPDATE* mskrm_dl_loader_baud? y/Y=yes, others=no: "
	read need
	case $need in
		y|Y)
			echo "Input new [ mskrm_dl_loader_baud ]:"
			echo "(9600/115200/460800/921600/1.5M/3M)"
			read data
			mskrm_dl_loader_baud=$data

			# replace data into file
			sed -i "s/^.*mskrm_dl_loader_baud=.*/mskrm_dl_loader_baud=${data}/" $BT_USER_CONF
			;;
		n|N|*)
			# read data from conf file
			mskrm_dl_loader_baud=$(sed -n 's/^mskrm_dl_loader_baud=\(.*\)$/\1/p' $BT_USER_CONF)
			echo "get current mskrm_dl_loader_baud: "$mskrm_dl_loader_baud
			echo " "
			;;
	esac

	#02: loader_dl_fw_hciserv_baud
	echo "+++: *UPDATE* loader_dl_fw_hciserv_baud? y/Y=yes, others=no: "
	read need
	case $need in
		y|Y)
			echo "Input new [ loader_dl_fw_hciserv_baud ]:"
			echo "(9600/115200/460800/921600/1.5M/3M)"
			read data
			loader_dl_fw_hciserv_baud=$data

			# replace data into file
			sed -i "s/^.*loader_dl_fw_hciserv_baud=.*/loader_dl_fw_hciserv_baud=${data}/" $BT_USER_CONF
			;;
		n|N|*)
			# read data from conf file
			loader_dl_fw_hciserv_baud=$(sed -n 's/^loader_dl_fw_hciserv_baud=\(.*\)$/\1/p' $BT_USER_CONF)
			echo "get current loader_dl_fw_hciserv_baud: "$loader_dl_fw_hciserv_baud
			echo " "
			;;
	esac

	#03: dev_path
	echo "+++: *UPDATE* dev_path? y/Y=yes, others=no: "
	read need
	case $need in
		y|Y)
			echo "Input new [ dev_path ]:"
			read data
			dev_path=$data

			# replace data into file
			sed -i "s/^.*dev_path=\".*/dev_path=\"${data}\"/" $BT_USER_CONF
  		;;
		n|N|*)
			# read data from conf file
			dev_path=$(sed -n 's/^dev_path=\(.*\)$/\1/p' $BT_USER_CONF)
			echo "get current dev_path: "$dev_path
			echo " "
			;;
	esac

	#04: bt_vendor
	echo "+++: *UPDATE* bt_vendor? y/Y=yes, others=no: "
	read need
	case $need in
		y|Y)
			echo "Input new [ bt_vendor ]:"
			read data
			bt_vendor=$data
			# replace data into file
			sed -i "s/^.*bt_vendor=\".*/bt_vendor=\"${data}\"/" $BT_USER_CONF
			;;
		n|N|*)
			# read data from conf file
			bt_vendor=$(sed -n 's/^bt_vendor=\(.*\)$/\1/p' $BT_USER_CONF)
			echo "get current bt_vendor: "$bt_vendor
			echo " "
			;;
	esac

	#05: flow_ctrl
	echo "+++: *UPDATE* flow_ctrl? y/Y=yes, others=no: "
	read need
	case $need in
		y|Y)
			echo "Input new [ flow_ctrl ]:"
			read data
			flow_ctrl=$data
			# replace data into file
			sed -i "s/^.*flow_ctrl=\".*/flow_ctrl=\"${data}\"/" $BT_USER_CONF
			;;
		n|N|*)
			# read data from conf file
			flow_ctrl=$(sed -n 's/^flow_ctrl=\(.*\)$/\1/p' $BT_USER_CONF)
			echo "get current flow_ctrl: "$flow_ctrl
			echo " "
			;;
	esac

	#06: lpc_ctrl
	echo "+++: *UPDATE* lpc_ctrl? y/Y=yes, others=no: "
	read need
	case $need in
		y|Y)
			echo "Input new [ lpc_ctrl ]:"
			read data
			lpc_ctrl=$data
			# replace data into file
			sed -i "s/^.*lpc_ctrl=\".*/lpc_ctrl=\"${data}\"/" $BT_USER_CONF
			;;
		n|N|*)
			# read data from conf file
			lpc_ctrl=$(sed -n 's/^lpc_ctrl=\(.*\)$/\1/p' $BT_USER_CONF)
			echo "get current lpc_ctrl: "$lpc_ctrl
			echo " "
			;;
	esac

	#07: bt_addr
	echo "+++: *UPDATE* bt_addr? y/Y=yes, others=no: "
	read need
	case $need in
		y|Y)
			echo "Input new [ bt_addr ]:"
			echo "(XX:XX:XX:XX:XX:XX)"
			read data
			bt_addr=$data

			# replace data into file
			sed -i "s/^.*bt_addr=.*/bt_addr=${data}/" $BT_USER_CONF
			;;
		n|N|*)
			# read data from conf file
			bt_addr=$(sed -n 's/^bt_addr=\(.*\)$/\1/p' $BT_USER_CONF)
			echo "get current bt_addr: "$bt_addr
			echo " "
			;;
	esac
}

# ---------------------------------------------------
#         bt_preruning
# ---------------------------------------------------
function bt_preruning(){

	echo "Reset bt power ..."
	echo 0 > /sys/class/rfkill/rfkill0/state
	sleep 0.1
	echo 1 > /sys/class/rfkill/rfkill0/state
	echo "Reset bt power finish!"
	echo " "

	echo "kill  btmcu_daemon ..."
	if [ -n "$( ps | grep btmcu_daemon | grep -v grep )" ]; then
	    echo "kill btmcu_daemon --> "
	    killall -9 btmcu_daemon
 	    wait $!
	    if [ ! $? -eq 0 ];then
	        echo "*** Kill btmcu_daemon program failure!"
	        exit 1
	    fi
	fi
	echo "btmcu_daemon kill finish!"
	echo " "
}

# ---------------------------------------------------
#         start_bt_service
# ---------------------------------------------------
function start_bt_service(){
	echo "Start bt service: ... "
	echo " "

	#01: stop wifi service
	echo "=========================================="
	echo "+++: RK962 not support PTA now, should stop wifi service."
	echo "+++: You can try to not stop wifi service firstly,"
	echo "+++: If bt have problem, can stop wifi by restart script."
	echo "=========================================="
	echo "+++: [Option] Stop wifi service? y/Y=yes, others=no: "
	if [ "$BT_INIT_CTRL" == "manual" ]; then
		read need
	else
		need=n
	fi
	case $need in
		y|Y)
			echo "******************************************"
			echo "Stop wifi service ... "
			echo " "

			# kill wpa_supplicant
			if [ -n "$( ps | grep wpa_supplicant | grep -v grep )" ]; then
				echo "kill wpa_supplicant --> "
				killall -9 wpa_supplicant
				wait $!
				if [ ! $? -eq 0 ];then
					echo "*** Kill wpa_supplicant program failure!"
					exit 1
				fi
			#	sleep 0.1
			fi

			# kill udhcpc
			if [ -n "$( ps | grep udhcpc | grep -v grep )" ]; then
				echo "kill udhcpc --> "
				killall -9 udhcpc
				wait $!
				if [ ! $? -eq 0 ];then
					echo "*** Kill udhcpc program failure!"
					exit 1
				fi
			#	sleep 0.1
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
			#	sleep 0.1
			fi

			# rmmod rk96x
			if [ -n "$( lsmod | grep rk96x )" ]; then
				echo "Removed rk96x.ko --> "
				rmmod rk96x
				wait $!
				if [ ! $? -eq 0 ];then
					echo "*** Exec insmod rk96x.ko command failure!"
					exit 1
				fi
				sync
				sleep 0.1
			fi
			echo "******************************************"
			echo ""
			;;
		n|N|*)
			echo "**********************************************"
			echo "Not stop wifi, bt maybe have unknown problem"
			echo "**********************************************"
			echo " "
#			exit 0
			;;
	esac

	echo "+++++++++++++++++++++++++++++++++++++++"
	echo "Bt begin ..."

: <<'END'	# no need to start dbus-daemon now
	echo "Mkdir /var/run/dbus -->"
	mkdir -p /var/run/dbus
	if [ ! $? -eq 0 ];then
		echo "*** mkdir /var/run/dbus  failure!"
		exit 1
	fi

	# kill dbus-daemon
	if [ -n "$( ps | grep dbus-daemon | grep -v grep )" ]; then
		echo "kill dbus-daemon --> "
		killall -9 dbus-daemon
		wait $!
		sleep 0.1

		if [ ! $? -eq 0 ];then
			echo "*** Kill dbus-daemon program failure!"
			exit 1
		fi
	fi

	if [ -z "$( ps | grep "dbus-daemon" | grep -v "grep" )" ]; then
		if [ -e "/run/messagebus.pid" ]; then
			rm -rfv /run/messagebus.pid
			sleep 0.1
		fi

		echo "Start service: dbus-daemon -->"
		/oem/usr/bin/dbus-daemon --config-file=/oem/usr/ko/system.conf
		wait $!
		sleep 0.1
		if [ ! $? -eq 0 ];then
			echo "*** Exec dbus-daemon program failure!"
			exit 1
		fi
		ps | grep "dbus-daemon" | grep -v "grep"
	fi
	
	#start_emode_server_app   # emode_server must start after dbus-daemon or bt start process will be blocked, why?
END

	# bt select type of download firmware
	echo "+++++++++++++++++++++++++++++++++++++++"
	echo "+++ Select type of download firmware:"
	echo "+++++++++++++++++++++++++++++++++++++++"

	echo "1: Download bt config net fw: rk962_bt_rf.bin & rk962_bt.bin"
	echo "2: Download bt standard mcu fw: rk962_bt_mcu.bin"
	if [ "$BT_INIT_CTRL" == "manual" ]; then
		read need
	else
		if [ "$RK_ENABLE_WIFI_CHIP" = "RK960" ]; then
			need=1
			bt_preruning
		else
			need=2
		fi
	fi
	case $need in
		1)
			echo "Download config network firmware fw1/fw2 ..."
			echo "Begin fw1 time: "
			date '+%H:%M:%S'
			echo " "
			echo "rk_hciattach -n -s" $mskrm_dl_loader_baud "-m 1"  $dev_path ${bt_vendor//\"/} $loader_dl_fw_hciserv_baud ${flow_ctrl//\"/} ${lpc_ctrl//\"/} $bt_addr "&"
			rk_hciattach -n -s $mskrm_dl_loader_baud -m 1  $dev_path ${bt_vendor//\"/} $loader_dl_fw_hciserv_baud ${flow_ctrl//\"/} ${lpc_ctrl//\"/} $bt_addr &
			RK_HCIATTACH_PID=$!
#			wait $!
			wait $RK_HCIATTACH_PID
			sleep 2.5
			RK_HCIATTACH_STATUS=$?
			
			echo " "
#			echo "+++: Download BT CFG_NET firmware successful? y/Y=yes, others=no: "
			echo "+++: Check download BT CFG_NET firmware result"
			if [ $RK_HCIATTACH_STATUS -eq 0 ]; then
				echo "download BT CFG_NET firmware successfully."
				#echo "Saved BT firmware status."
				#emode_server btfw_write 1 0
    		# kill bluetoothd
				if [ -n "$( ps | grep bluetoothd | grep -v grep )" ]; then
					echo "kill bluetoothd --> "
					killall -9 bluetoothd
					wait $!
					#	sleep 0.5

					if [ ! $? -eq 0 ];then
						echo "*** Kill bluetoothd program failure!"
						exit 1
					fi
				fi

				if [ -z "$( ps | grep "bluetoothd" | grep -v "grep" )" ]; then
					echo "Start service: bluetoothd - sacn/link/mgmt -->"
					/oem/usr/bin/bluetoothd -n -d -P Battery
					wait $!
					sleep 0.5

					if [ ! $? -eq 0 ];then
						echo "*** Exec bluetoothd program failure!"
						exit 1
					fi
					ps | grep "bluetoothd" | grep -v "grep"
				fi

				echo "Start rkwifibt app testing..."
				echo "+++++++++++++++++++++++++++++++++++++++"
				echo "Common options ==> "
				echo "bt init                     01 input on"
				echo "bt on/off power:            04 input on"
				echo "bt on/off discoverable:     05 input on"
				echo "bt enable a2dp sink:        16 input on"
				echo "+++++++++++++++++++++++++++++++++++++++"
				if [ -z "$( ps | grep "rkwifibt_app_test" | grep -v "grep" )" ]; then
					/oem/usr/bin/rkwifibt_app_test BT
					sleep 1

					if [ ! $? -eq 0 ];then
						echo "*** Exec rkwifibt_app_test program failure!"
						exit 1
					fi

				#	ps | grep "rkwifibt_app_test" | grep -v "grep"
				fi
			else
    		echo "download BT CFG_NET firmware failed with status $RK_HCIATTACH_STATUS."
    		#echo "Saved BT firmware status."
    		#emode_server btfw_write 1 -1
    		echo "Start bt service end!"
				echo " "
				exit 1
			fi
			;;
		2)
			echo "Download standard firmware bt_mcu ..."
			echo "Begin fw2 time: "
			date '+%H:%M:%S'
			echo " "
			echo "rk_hciattach -n -s" $mskrm_dl_loader_baud "-m 2"  $dev_path ${bt_vendor//\"/} $loader_dl_fw_hciserv_baud ${flow_ctrl//\"/} ${lpc_ctrl//\"/} $bt_addr "&"
			rk_hciattach -n -s $mskrm_dl_loader_baud -m 2  $dev_path ${bt_vendor//\"/} $loader_dl_fw_hciserv_baud ${flow_ctrl//\"/} ${lpc_ctrl//\"/} $bt_addr &
			RK_HCIATTACH_PID=$!
#			wait $!
			wait $RK_HCIATTACH_PID
			sleep 2s
      RK_HCIATTACH_STATUS=$? 
			echo " "
#			echo "+++: Download BT_MCU firmware successful? y/Y=yes, others=no: " 
			echo "+++: Check download BT_MCU firmware result" 
			if [ $RK_HCIATTACH_STATUS -eq 0 ]; then
    		echo "download BT_MCU firmware successfully."
    		#echo "Saved BT firmware status."
    		#emode_server btfw_write 2 0
    		# kill bluetoothd
				if [ -n "$( ps | grep bluetoothd | grep -v grep )" ]; then
					echo "kill bluetoothd --> "
					killall -9 bluetoothd
					wait $!
					sleep 0.5

					if [ ! $? -eq 0 ];then
						echo "*** Kill bluetoothd program failure!"
						exit 1
					fi
				fi

				echo "+++++++++++++++++++++++++++++++++++++++"
				echo "Change TTY HCI mode to normal mode"
				echo "+++++++++++++++++++++++++++++++++++++++"
				echo " "
				echo "TTY line discipline: 15(N_HCI)"
				echo "----------------------------------------"
				stty -F $dev_path
				echo "----------------------------------------"
				sleep 0.1

				# kill rk_hciattach
				if [ -n "$( ps | grep rk_hciattach | grep -v grep )" ]; then
					echo "kill rk_hciattach ... "
					echo "----------------------------------------"
					killall -9 rk_hciattach
					wait $!
					sleep 0.2

					if [ ! $? -eq 0 ];then
						echo "*** Kill rk_hciattach program failure!"
						exit 1
					fi
				fi
				# manual mode
				if [ "$BT_INIT_CTRL" == "manual" ]; then
					#10: select stop charge control and host or not
					echo "BT_INIT_CTRL: manual mode"
					echo "+++: *STOP* charge ctr and host? y/Y=yes, others=no: "
					read need
					case $need in
						y|Y)
							# close_charge_control_and_host
							;;
						n|N|*)
							;;
					esac

					#11: select start_btmcu_daemon_app or not
					echo "+++: *START* start uart test appt? y/Y=yes, others=no: "
					read need
					case $need in
						y|Y)
							start_btmcu_daemon_app
							;;
						n|N|*)
							;;
					esac
				else # auto mode
					# close_charge_control_and_host
					echo "BT_INIT_CTRL: auto mode"
					start_btmcu_daemon_app
				fi
    		
			else
    		echo "download BT_MCU firmware failed with status $RK_HCIATTACH_STATUS."
    		#echo "Saved BT firmware status."
    		#emode_server btfw_write 2 -1
    		echo "Start bt service end!"
				echo " "
				exit 1
			fi
			;;
		*)
			echo "*** Select option error !!!"
			exit 0
			;;
	esac

	echo " "

:<<'END'
	# wait 3s if possible
	if [ -z "$( ifconfig -a | grep "inet addr" )" ]; then
		sleep 0.3
		if [ -z "$( ifconfig -a | grep "inet addr" )" ]; then
			sleep 0.5
			if [ -z "$( ifconfig -a | grep "inet addr" )" ]; then
				sleep 0.7
				if [ -z "$( ifconfig -a | grep "inet addr" )" ]; then
					sleep 0.9
					if [ -z "$( ifconfig -a | grep "inet addr" )" ]; then
						sleep 1.1
					fi
				fi
			fi
		fi
	fi
END

	echo "========== BT MCU Start Complete =========="

	bt_copyright_info
	exit 0
}

# ---------------------------------------------------
#  bt start mode
# ---------------------------------------------------
echo "+++++++ BT bootup ++++++++"
echo "System bootup, BT auto-start default."
echo "[1] Before start, 'rkkp 6' set bt not auto-start"
echo "[2] Manual mode stage, confirm with input: y or Y "
echo "++++++++++++++++++++++++++"

if [ "$BT_INIT_CTRL" == "manual" ]; then
	echo "Input: y/Y to start with manual -> "
	read userctrl

	if [ "$userctrl" == "y" ] || [ "$userctrl" == "Y" ]; then
		echo "--- BT Init By Manual Ctrl ---"
	else
		echo "### BT bootup canceled by user"
		exit 0
	fi
else
	echo "--- BT Init By Auto Ctrl ---"
fi

if [ "$bt_auto_start" == 0 ] && [ "$BT_INIT_CTRL" == "auto" ] ; then
	echo "--- BT Not Start: bt_auto_start == 0 ---"
	exit 0
fi

# ---------------------------------------------------
#  main: get the user's choice to update config or not
# ---------------------------------------------------

choice=n

if [ "$BT_INIT_CTRL" == "manual" ]; then
	echo "+++ bt config data content: ===>"
	cat $BT_USER_CONF
	echo "+++: *UPDATE* config file's data? y/Y=yes, others=no: "
	read choice
fi

case $choice in
	y|Y) update_bt_conf_content;;
	n|N|*) start_bt_service;;
esac

# ---------------------------------------------------
#  re-config user's data or not
# ---------------------------------------------------
echo "+++++++++++++++++++++++++++++++++++++++"
echo "+++ bt config data content: ===>"
echo "+++++++++++++++++++++++++++++++++++++++"
cat $BT_USER_CONF

echo "+++: Is ok? data need *RE-CONFIG*? y/Y=yes, others=no ===>"
read select
case $select in
	y|Y) update_bt_conf_content;;
	n|N|*) start_bt_service;;
esac

echo "+++++++++++++++++++++++++++++++++++++++"
echo "+++ bt config data content: ===>"
echo "+++++++++++++++++++++++++++++++++++++++"
cat $BT_USER_CONF

echo "+++: Is ok? data need *RE-CONFIG*? y/Y=yes, others=no ===>"
read select
case $select in
	y|Y) ;;
	n|N|*) start_bt_service;;
esac

echo	"*** Modified data more than 2 times, please check your operation."
echo	"*** Start bt service failure!"
