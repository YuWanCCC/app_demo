# /*********************************************************************************
# * Copyright (C),2020-2030,  Rockchip Beijing Bansband Center
# * All Rights Reserved
# * V1.00
# * FileName : sys_test.sh
# * Author :  wangqunyang
# * Description:   config user's data and start system test
# * History :
# *   <author>      <time>        <version>         <desc>
# *   wangqunyang   2024/08/15      2.0               ORG
# *
# *
# *********************************************************************************/

#!/bin/sh

cmd=$(realpath "$0")
_DIR=$(dirname "$cmd")
cd "$_DIR" || exit 1

####################################################
##   Public Configuraton
####################################################
test ! -f "rk96x_env.sh"  || source "rk96x_env.sh"

WIFI_USER_CONF=${RK96X_CONF_PATH}/wifi_user.conf
SYS_TEST_CONF=${RK96X_CONF_PATH}/sys_test.conf

dev_path=$RK_WIFI_BTMCU_TTYS

## default is auto
SYS_TEST_CTRL="auto"

# ---------------------------------------------------
#         check configuration files
# ---------------------------------------------------
if [ ! -f "$WIFI_USER_CONF" ]; then
	echo "System test: $WIFI_USER_CONF not exist."
	echo "Set WIFI_USER_CONF is: null."
	WIFI_USER_CONF=""
#	exit 1
fi

if [ ! -f "$SYS_TEST_CONF" ]; then
	echo "$SYS_TEST_CONF not exist, copy config file with /etc/sys_test.conf"
	cp -rf /etc/sys_test.conf ${RK96X_CONF_PATH}/sys_test.conf

	if [ ! $? -eq 0 ];then
		echo "Copy config file of sys_test.conf failure!"
		exit 1
	fi
fi

####################################################
##   Global variables declaration
####################################################
## system test 001: tcp keep alive
remote_ip_user=
remote_ip_port=
keep_alive_step=

if [ -n "$WIFI_USER_CONF" ]; then
	remote_ip_user=$(sed -n 's/^remote_ip_user=\(.*\)$/\1/p' $WIFI_USER_CONF)
	remote_ip_port=$(sed -n 's/^remote_ip_port=\(.*\)$/\1/p' $WIFI_USER_CONF)
	keep_alive_step=$(sed -n 's/^keep_alive_step=\(.*\)$/\1/p' $WIFI_USER_CONF)
fi

## set default value if get value
if [ -z "$remote_ip_user" ]; then
    echo "Get [remote_ip_user] null, set default value: 192.168.0.102"
    remote_ip_user=192.168.0.102
fi

if [ -z "$remote_ip_port" ]; then
    echo "Get [remote_ip_port] null, set default value: 8888"
    remote_ip_port=8888
fi

if [ -z "$keep_alive_step" ]; then
    echo "Get [keep_alive_step] null, set default value: 60"
    keep_alive_step=60
fi

## system test 001: status
st001_test_status=$(sed -n 's/^st001_test_status=\(.*\)$/\1/p' $SYS_TEST_CONF)
st001_num_total=$(sed -n 's/^st001_num_total=\(.*\)$/\1/p' $SYS_TEST_CONF)
st001_num_done=$(sed -n 's/^st001_num_done=\(.*\)$/\1/p' $SYS_TEST_CONF)
st001_num_pass=$(sed -n 's/^st001_num_pass=\(.*\)$/\1/p' $SYS_TEST_CONF)
st001_num_fail=$(sed -n 's/^st001_num_fail=\(.*\)$/\1/p' $SYS_TEST_CONF)

## system test 002: wifi rate and distance test
rk96X_multi_ip_flag="n"
rk96X_local_ip_address=
remote_win_pc_ip_addr=
st002_test_status=$(sed -n 's/^st002_test_status=\(.*\)$/\1/p' $SYS_TEST_CONF)
st002_num_total=$(sed -n 's/^st002_num_total=\(.*\)$/\1/p' $SYS_TEST_CONF)
st002_num_done=$(sed -n 's/^st002_num_done=\(.*\)$/\1/p' $SYS_TEST_CONF)
st002_num_pass=$(sed -n 's/^st002_num_pass=\(.*\)$/\1/p' $SYS_TEST_CONF)
st002_num_fail=$(sed -n 's/^st002_num_fail=\(.*\)$/\1/p' $SYS_TEST_CONF)

## system test 003: reboot(warm start)
st003_test_status=$(sed -n 's/^st003_test_status=\(.*\)$/\1/p' $SYS_TEST_CONF)
st003_num_total=$(sed -n 's/^st003_num_total=\(.*\)$/\1/p' $SYS_TEST_CONF)
st003_num_done=$(sed -n 's/^st003_num_done=\(.*\)$/\1/p' $SYS_TEST_CONF)
st003_num_pass=$(sed -n 's/^st003_num_pass=\(.*\)$/\1/p' $SYS_TEST_CONF)
st003_num_fail=$(sed -n 's/^st003_num_fail=\(.*\)$/\1/p' $SYS_TEST_CONF)
st003_ping_fail=$(sed -n 's/^st003_ping_fail=\(.*\)$/\1/p' $SYS_TEST_CONF)

## system stress test counter
stress_test_total=0
stress_test_done=0

# Function to reset system state
function reboot_test_reset_sys_state() {

    echo " "
    echo "Reset state: start btmcu_daemon ..."
    echo " "

    killall btmcu_daemon
    wait $!
    sleep 1
    btmcu_daemon start $dev_path &
    sleep 1

    echo " "

    for i in $(seq 5 -1 1); do
       echo -ne "Will reboot in ${i} seconds...\r"
       sleep 1
    done

    echo " "

    echo "Wait for system reboot...       "
    echo " "
    echo " "
    sync
}

# ---------------------------------------------------
#         get_local_ip
# ---------------------------------------------------
function get_local_ip() {
    if [ -n "$(ifconfig -a | grep 'inet addr')" ]; then
        echo "ifconfig -a output: $(ifconfig -a | grep 'inet addr')"
        echo " "

        echo "RK_APP_TYPE=$RK_APP_TYPE"
        case "$RK_APP_TYPE" in
            RKIPC_RV1126B)
                rk96X_local_ip_address=$(ifconfig | grep -w 'inet' | grep -v '127.0.0.1' | \
                                          sed -n 's/.*addr:\([0-9.]*\).*/\1/p' | head -n 1)
                ;;
            RK_FASTBOOT_SERVER|RK_FASTBOOT_CLIENT)
                data12=$(ifconfig -a | grep -E -m 1 'inet ' | grep -v '127.0.0.1' | awk '{print $2}')
                rk96X_local_ip_address=${data12##*:}
                ;;
            *)
                echo "*** Unknown RK_APP_TYPE=$RK_APP_TYPE, using default IP detection method..."
                rk96X_local_ip_address=$(ifconfig | grep -w 'inet' | grep -v '127.0.0.1' | \
                                          sed -n 's/.*addr:\([0-9.]*\).*/\1/p' | head -n 1)
                ;;
        esac

        echo "Auto-gain: rk96X_local_ipaddr=$rk96X_local_ip_address"
        echo " "
    else
        echo "*** Auto gain local ip is NULL !!!"
        echo "*** Please config valid AP "
        echo " "
        exit 1
    fi
}

# ---------------------------------------------------
#         check_network_status
# ---------------------------------------------------
function check_network_status(){
	echo "Check network status ... "

	#01: check network status
	wpa_cli status &
	wait $!
	echo " "

	if [ -n "$( wpa_cli status | grep "wpa_state=COMPLETED" )" ]; then

		ssid_data=$( wpa_cli status | grep "ssid=" | grep -v "bssid=")
		ip_data=$( wpa_cli status | grep "ip_address=" )

		current_ssid=$(echo "$ssid_data" | sed 's/^.*ssid=\(.*\)$/\1/')
		current_ip=$(echo "$ip_data" | sed 's/^.*ip_address=\(.*\)$/\1/')

		echo "+++ Current AP SSID: $current_ssid"
		echo "+++ Current IP Addr: $current_ip"
		echo " "

	fi

	echo "+++: AP's need to reconfig? y/Y=yes, others=no: "
	if [ "$SYS_TEST_CTRL" == "manual" ] ; then
		read need
	else
		need=n
	fi
	case $need in
		y|Y)
			echo "Switch to new AP:"
			echo "Input new [ ap_ssid ]:"
			read ap_ssid
			echo "Input new [ ap_psk ['NONE' when open] ]:"
			read ap_psk

			if [ "$ap_psk" == "NONE" ]  || [ "$ap_psk" == "None" ] || [ "$ap_psk" == "none" ]; then
				echo "rkwifi_server connect $ap_ssid"
				rkwifi_server connect $ap_ssid &
				wait $!
			else
				echo "rkwifi_server connect $ap_ssid $ap_psk"
				rkwifi_server connect $ap_ssid $ap_psk &
				wait $!
			fi

			sleep 1s
			scan_retry=1

			while true; do

				if [ $(($scan_retry % 5)) -eq 0 ]; then
					echo "Scan retry, please wait ... "
				fi

				if [ -n "$( wpa_cli status | grep "wpa_state=COMPLETED" )" ] && [ -n "$( wpa_cli status | grep "ip_address=" )" ]; then
			    break
				fi

				if [ -n "$( wpa_cli status | grep "wpa_state=DISCONNECTED" )" ]; then
			    break
				fi

				if [ -n "$( wpa_cli status | grep "wpa_state=CONNECTFAILED" )" ]; then
			    break
				fi

				# non-global ctrl_ifname: (nil)
				if [ -z "$( wpa_cli status | grep "wpa_state=" )" ]; then
			    break
				fi

				if [ "$scan_retry" == "35" ]; then
			    break
				fi

				sleep 0.5
				let "scan_retry=scan_retry+1"
			done

			sleep 3s

		#	ps & > /data/myps.txt
		#	echo "ps | grep wifi_connect_state_check | grep -v grep"
		#	ps | grep wifi_connect_state_check | grep -v grep

			echo " "
			if [ -z "$( wpa_cli status | grep "wpa_state=COMPLETED" )" ]; then
				echo "*** Connect AP failure !!!"
				echo "*** Please reconfig correct AP !!!"
				echo " "
				exit 1
			fi

			;;
		n|N|*)
			;;
	esac

	#02: auto gain local ip address
	get_local_ip
#	if [ -n "$( ifconfig -a | grep "inet addr" )" ]; then

#		echo "ifconfig -a output: "$( ifconfig -a | grep "inet addr" )
#		echo " "

#		data12=$(ifconfig -a | grep -E -m 1 'inet ' | grep -v '127.0.0.1' | awk '{print $2}')
#		rk96X_local_ip_address=${data12##*:}

#		echo "Auto-gain: rk96X_local_ipaddr=$rk96X_local_ip_address"
#		echo " "
#	else
#		echo "*** Auto gain local ip is NULL !!!"
#		echo "*** Please config valid AP "
#		echo " "
#		exit 1
#	fi
}

# ---------------------------------------------------
#         check device multi ip
# ---------------------------------------------------
function check_device_multi_ip(){

	echo "Check device multi ip ... "

	#01: multiple ip address
	echo "+++: *CHECK* rk96X is multiple ipaddr? y/Y=yes, others=no: "
	if [ "$SYS_TEST_CTRL" == "manual" ] ; then
		read checkip
	else
		checkip=n
	fi
	case $checkip in
		y|Y)
			echo "Input[ 96X device working ip address ]:"
			read data
			rk96X_local_ip_address=$data
			echo "Active 96X device work IP: $rk96X_local_ip_address"
			rk96X_multi_ip_flag="y"
			;;
		n|N|*)
			;;
	esac	
}

# ---------------------------------------------------
#         check network connectivity
# ---------------------------------------------------
function check_network_connectivity(){

	echo "Check network connectivity ... "
	echo " "
	local devcie_ip=$1
	local remote_ip=$2

	if [ -z "$devcie_ip" ] || [ -z "$remote_ip" ] ; then
		echo "IP is null, devcie_ip=$devcie_ip, remote_ip=$remote_ip"
		echo "Exit, check and retry !!!"
		echo " "
		exit 1
	fi

	ping -c 2 $remote_ip | grep "ttl"

	if [ $? -eq 0 ]; then
		echo "Local: $devcie_ip connected to Remote: $remote_ip"
		echo " "
	else
		echo "*** Local: $devcie_ip unreachable with Remote: $remote_ip"
		echo " "
		exit 1
	fi

}

# ---------------------------------------------------
#         start_wifi_keep_alive
# ---------------------------------------------------
function start_wifi_keep_alive(){

	echo "===================================="
	echo " Start wifi keep alive ... "
	echo "===================================="
	echo " "

	#01: set server ip
	echo "From config latest paramters: "
	echo "remote_ip_user=$remote_ip_user"
	echo "remote_port=$remote_ip_port"
	echo "keep_alive_step=$keep_alive_step (s)"
	echo " "
	echo "+++: *UPDATE* these parameters? y/Y=yes, others=no: "
	if [ "$SYS_TEST_CTRL" == "manual" ] ; then
		read need
	else
		need=n
	fi
	case $need in
		y|Y)
			echo "Input new [ remote_ip_user <xxx.xxx.xxx.xxx> ]:"
			read data
			remote_ip_user=$data

			# replace data into file
			if [ -n "$WIFI_USER_CONF" ]; then
				sed -i "s/^.*remote_ip_user=.*/remote_ip_user=${data}/" $WIFI_USER_CONF
			fi

			echo "Input new [ remote_port <8000-8999> ]:"
			read data
			remote_ip_port=$data

			# replace data into file
			if [ -n "$WIFI_USER_CONF" ]; then
				sed -i "s/^.*remote_ip_port=.*/remote_ip_port=${data}/" $WIFI_USER_CONF
			fi

			echo "Input new [ keep_alive_step <10-60> s ]:"
			read data
			keep_alive_step=$data

			# replace data into file
			if [ -n "$WIFI_USER_CONF" ]; then
				sed -i "s/^.*keep_alive_step=.*/keep_alive_step=${data}/" $WIFI_USER_CONF
			fi
			echo " "
			;;
		n|N|*)
			# read data from conf file
			;;
	esac

	remote_win_pc_ip_addr=$remote_ip_user
	
	#02: check connectivity
	check_network_connectivity $rk96X_local_ip_address $remote_win_pc_ip_addr

	#03: keep alive
	kp_demo $remote_win_pc_ip_addr $remote_ip_port $keep_alive_step &

	wait $!
	echo " "

	echo "TCP keep alive ongoing ... "
	echo " "

	exit 0
}
# ---------------------------------------------------------
#      [1] wifi_rate_UDP_96Xserver_pcclient
#  udp server, parameter: sup
#  Common CMD: iperf -s -u -p 5002
#              [-B 192.168.0.XXX(if multi ip, use -B bind server IP)]
# ---------------------------------------------------------
function wifi_rate_UDP_96Xserver_pcclient(){
	echo " "
	echo "[SECURECRT]"
	echo "wifi rate: UDP.96X.Server <--> WIN-PC-Client <96X UDP RX> "

	if [ "$rk96X_multi_ip_flag" == "y" ]; then
		echo " "
		echo "iperf -s -u -p 5002 -B $rk96X_local_ip_address"
		iperf -s -u -p 5002 -B $rk96X_local_ip_address &
		wait $!
	else
		echo " "
		echo "iperf -s -u -p 5002"
		iperf -s -u -p 5002 &
		wait $!
	fi
	echo " "
	wifi_throughput_rate_test
}

# ---------------------------------------------------------
#      [2] wifi_rate_UDP_96Xclient_pcserver
#  udp client, parameter: cubtip
#  Common CMD: iperf -c 192.168.0.XXX (pc server ip)
#              -u -b 60M -t 100 -i 1 -p 5003
#              [-B 192.168.0.XXX(if multi ip, use -B bind server IP)]
# ---------------------------------------------------------
function wifi_rate_UDP_96Xclient_pcserver(){
	echo " "
	echo "[SECURECRT]"
	echo " "
	echo "wifi rate: UDP.96X.Client <--> WIN-PC-Server <96X UDP TX> "

	if [ "$rk96X_multi_ip_flag" == "y" ]; then
		echo " "
		echo "iperf -c $remote_win_pc_ip_addr -u -b 60M -t 100 -i 1 -p 5003 -B $rk96X_local_ip_address"
		iperf -c $remote_win_pc_ip_addr -u -b 60M -t 100 -i 1 -p 5003 -B $rk96X_local_ip_address &
		wait $!
	else
		echo " "
		echo "iperf -c $remote_win_pc_ip_addr -u -b 60M -t 100 -i 1 -p 5003"
		iperf -c $remote_win_pc_ip_addr -u -b 60M -t 100 -i 1 -p 5003 &
		wait $!
	fi

	echo " "
	wifi_throughput_rate_test
}

# ---------------------------------------------------------
#      [3] wifi_rate_TCP_96Xserver_pcclient
#  tcp server, parameter: sp
#  Common CMD: iperf -s -p 5004
#              [-B 192.168.0.XXX(if multi ip, use -B bind server IP)]
# ---------------------------------------------------------
function wifi_rate_TCP_96Xserver_pcclient(){
	echo " "
	echo "[SECURECRT]"
	echo "wifi rate: TCP.96X.Server <--> WIN-PC-Client <96X TCP RX> "

	if [ "$rk96X_multi_ip_flag" == "y" ]; then
		echo " "
		echo "iperf -s -p 5004 -B $rk96X_local_ip_address"
		iperf -s -p 5004 -B $rk96X_local_ip_address &
		wait $!
	else
		echo " "
		echo "iperf -s -p 5004"
		iperf -s -p 5004 &
		wait $!
	fi
	echo " "
	wifi_throughput_rate_test
}

# ---------------------------------------------------------
#      [4] wifi_rate_TCP_96Xclient_pcserver
#  tcp client, parameter: cwtip
#  Common CMD: iperf -c 192.168.0.102 (pc server ip)
#              -w 60M -t 100 -i 1 -p 5005
#              [-B 192.168.0.XXX(if multi ip, use -B bind server IP)]
# ---------------------------------------------------------
function wifi_rate_TCP_96Xclient_pcserver(){
	echo " "
	echo "[SECURECRT]"
	echo " "
	echo "wifi rate: TCP.96X.Client <--> WIN-PC-Server <96X TCP TX> "

	if [ "$rk96X_multi_ip_flag" == "y" ]; then
		echo " "
		echo "iperf -c $remote_win_pc_ip_addr -w 1M -t 100 -i 1 -p 5005 -B $rk96X_local_ip_address"
		iperf -c $remote_win_pc_ip_addr -w 1M -t 100 -i 1 -p 5005 -B $rk96X_local_ip_address &
		wait $!
	else
		echo " "
		echo "iperf -c $remote_win_pc_ip_addr -w 1M -t 100 -i 1 -p 5005"
		iperf -c $remote_win_pc_ip_addr -w 1M -t 100 -i 1 -p 5005 &
		wait $!
	fi

	echo " "
	wifi_throughput_rate_test
}

# ---------------------------------------------------
#         wifi_throughput_rate_test
# ---------------------------------------------------
function wifi_throughput_rate_test(){
	echo " "
	echo "======================================"
	echo " Start wifi throughput rate test ... "
	echo "======================================"
	echo " "

	#01: set server ip
	echo "From config latest paramters: "
	echo "remote_ip_user=$remote_ip_user"
	echo " "
	echo "+++: *UPDATE* the remote IP? y/Y=yes, others=no: "
	if [ "$SYS_TEST_CTRL" == "manual" ] ; then
		read need
	else
		need=n
	fi	
	case $need in
		y|Y)
			echo "Input new [ remote_ip_user <xxx.xxx.xxx.xxx> ]:"
			read data
			remote_ip_user=$data
			remote_win_pc_ip_addr=$remote_ip_user

			# replace data into file
			if [ -n "$WIFI_USER_CONF" ]; then
				sed -i "s/^.*remote_ip_user=.*/remote_ip_user=${data}/" $WIFI_USER_CONF
			fi
			echo " "
			;;
		n|N|*)
			# read data from conf file
			remote_win_pc_ip_addr=$remote_ip_user
			echo "get current remote_win_pc_ip_addr: "$remote_win_pc_ip_addr
			echo " "
			;;
	esac

	#02: check connectivity
	check_network_connectivity $rk96X_local_ip_address $remote_win_pc_ip_addr

	#03: throughput rate test
	echo "-----------------------------------------------------"
	echo "wifi throughput rate test:"
	echo "[1] UDP: 96X + HOST server, WIN-PC client <Simplex: 96X UDP RX> "
	echo "[2] UDP: 96X + HOST client, WIN-PC server <Simplex: 96X UDP TX> "
	echo "[3] TCP: 96X + HOST server, WIN-PC client <Simplex: 96X TCP RX> "
	echo "[4] TCP: 96X + HOST client, WIN-PC server <Simplex: 96X TCP TX> "
	echo "[Others]: Exit Wifi Rate Test"
#	echo "[5] UDP: 96X + HOST S/C, WIN-PC S/C <Duplex: 96X UDP TX/RX> "
#	echo "[6] TCP: 96X + HOST S/C, WIN-PC S/C <Duplex: 96X TCP TX/RX> "
	echo "++++++++++++++++++++++++++++++++++++++++++++++++++++++"
	echo "Input your selection: "
	if [ "$SYS_TEST_CTRL" == "manual" ] ; then
		read rate_test
	else
		rate_test=n
	fi
	case $rate_test in
		1)
			wifi_rate_UDP_96Xserver_pcclient
			;;
		2)
			wifi_rate_UDP_96Xclient_pcserver
			;;
		3)
			wifi_rate_TCP_96Xserver_pcclient
			;;
		4)
			wifi_rate_TCP_96Xclient_pcserver
			;;
#		5)
#			wifi_rate_UDP_96Xserver_pcclient
#			wifi_rate_UDP_96Xclient_pcserver
#			;;
#		6)
#			wifi_rate_TCP_96Xserver_pcclient
#			wifi_rate_TCP_96Xclient_pcserver
#			;;
		*)
			echo "*** select number error ***"
			echo " "
			exit 0
			;;
	esac

#	echo "WIFI throughput rate test finish!"
	echo " "
	exit 0
}

# ---------------- Helper Functions ----------------

function reload_st003_status_from_file() {
    st003_test_status=$(sed -n 's/^st003_test_status=\(.*\)$/\1/p' "$SYS_TEST_CONF")
    st003_num_total=$(sed -n 's/^st003_num_total=\(.*\)$/\1/p' "$SYS_TEST_CONF")
    st003_num_done=$(sed -n 's/^st003_num_done=\(.*\)$/\1/p' "$SYS_TEST_CONF")
    st003_num_pass=$(sed -n 's/^st003_num_pass=\(.*\)$/\1/p' "$SYS_TEST_CONF")
    st003_num_fail=$(sed -n 's/^st003_num_fail=\(.*\)$/\1/p' "$SYS_TEST_CONF")
    st003_ping_fail=$(sed -n 's/^st003_ping_fail=\(.*\)$/\1/p' "$SYS_TEST_CONF")
}

function save_status() {
    sed -i "s/^.*st003_num_total=.*/st003_num_total=${st003_num_total}/" "$SYS_TEST_CONF"
    sed -i "s/^.*st003_num_done=.*/st003_num_done=${st003_num_done}/" "$SYS_TEST_CONF"
    sed -i "s/^.*st003_num_pass=.*/st003_num_pass=${st003_num_pass}/" "$SYS_TEST_CONF"
    sed -i "s/^.*st003_num_fail=.*/st003_num_fail=${st003_num_fail}/" "$SYS_TEST_CONF"
    sed -i "s/^.*st003_ping_fail=.*/st003_ping_fail=${st003_ping_fail}/" "$SYS_TEST_CONF"
    sed -i "s/^.*st003_test_status=.*/st003_test_status=${st003_test_status}/" "$SYS_TEST_CONF"
}

function system_reboot_stest003_reset_status() {
    st003_num_total=0
    st003_num_done=0
    st003_num_pass=0
    st003_num_fail=0
    st003_ping_fail=0
    st003_test_status=0
    save_status
}

function system_reboot_test_part_statistics() {
    reload_st003_status_from_file
    part_done=$st003_num_done
    part_pass=$st003_num_pass

    echo "+++ Part-TestResult: $part_pass/$part_done"
    if [ "$part_done" -ne 0 ]; then
        partsuccess_rate=$(awk -v b=$part_pass -v a=$part_done 'BEGIN{printf "%.1f\n", (a>0) ? (b/a)*100 : 0}')
        echo "### Part-SuccessRate: $partsuccess_rate %"
    fi
}

# ---------------- Main Function ----------------

function system_reboot_test() {
    echo "======================================"
    echo " Start system reboot test ... "
    echo "======================================"
    echo ""
	
    reload_st003_status_from_file

    if [ "$SYS_TEST_CTRL" = "manual" ]; then
        system_reboot_stest003_reset_status

        echo "+++: *UPDATE* test repeat times:"
        echo "Input new [ st003_num_total ]:"
        read data

        if ! echo "$data" | grep -Eq "^[0-9]+$" || [ "$data" -le 0 ]; then
            echo "Error: invalid input '$data'"
            echo "System test exit!"
            exit 0
        fi

        st003_num_total=$data
        st003_test_status=1
        save_status

        reboot_test_reset_sys_state

        echo "Begin reboot ... "
        reboot
        exit 0

    else
        st003_num_done=$((st003_num_done + 1))

        max_wait=30
        inet_addr_local=""
        for i in $(seq 1 $max_wait); do
            inet_addr_local=$(ifconfig wlan0 2>/dev/null | grep 'inet addr:' | grep -v '127.0.0.1' | sed -E 's/.*inet addr:([0-9.]+).*/\1/')
            if [ -n "$inet_addr_local" ]; then
                break
            fi
				echo "Waiting for IP address... ($i/$max_wait)"
				sleep 1
		done

		echo "Gain: inet_addr_local=$inet_addr_local"

        if [ -n "$inet_addr_local" ]; then
            echo "=========== Detected Valid IP Address ==========="
            echo "Pinging www.baidu.com ..."
            if ping -c 5 -W 2 www.baidu.com > /dev/null 2>&1; then
                echo "=========== Ping Successful ==========="
            else
                echo "----------- Ping Failed -----------"
                echo "Ping test failed but considered as success."
                st003_ping_fail=$((st003_ping_fail + 1))
            fi
            st003_num_pass=$((st003_num_pass + 1))
        else
            echo "----------- No Valid IP Address Detected -----------"
            echo "----------- Test Failure -----------"
            st003_num_fail=$((st003_num_fail + 1))
        fi

        save_status
        system_reboot_test_part_statistics
        echo ""

        if [ "$st003_num_done" -ge "$st003_num_total" ] || [ "$st003_num_total" -eq 0 ]; then
            echo "##############################################"
            echo "+++ Reboot test finish!!!"
            echo "+++ TestResult: $st003_num_pass/$st003_num_total"
            echo "+++ Ping failures: $st003_ping_fail"
            if [ "$st003_num_total" -ne 0 ]; then
                success_rate=$(awk -v b=$st003_num_pass -v a=$st003_num_total 'BEGIN{printf "%.1f\n", (a>0) ? (b/a)*100 : 0}')
                echo "+++ SuccessRate: $success_rate %"
            fi
            echo "##############################################"
            system_reboot_stest003_reset_status
            exit 0
        else
            echo "Reboot ongoing ... "

            reboot_test_reset_sys_state

            reboot
            exit 0
        fi
    fi
}
# ---------------------------------------------------
#         WiFi on/off pressure test
# ---------------------------------------------------
# Function to perform WiFi on/off pressure test
start_wifi_onoff_test() {
	local cycles=$1
	if [[ -z "$cycles" || "$cycles" -le 0 ]]; then
		echo "Invalid number of cycles. Please provide a positive integer."
		return 1
	fi
	ulimit -s 20480
	ulimit -l 128
	ulimit -p 168
	ulimit -n 2048
	echo "Starting WiFi on/off pressure test with $cycles cycles..."
	ulimit -a
	rkwifi_server onoff_test "$cycles"
}
# ---------------------------------------------------
#         AP switch test
# ---------------------------------------------------
# Function to perform AP switch test
start_ap_switch_test() {
	local ap_num=$1
	if [[ -z "$ap_num" || "$ap_num" -le 0 ]]; then
		echo "Invalid AP count. Please provide a positive integer."
		return 1
	fi
	echo "Starting AP switch test with $ap_num APs..."
	sys_test_wifi_switch_ap_random.sh "$ap_num"
}
# ---------------------------------------------------
#         Firmware switch test
# ---------------------------------------------------
# Function to perform firmware switch test
start_fw_switch() {
	local fw_num=$1
	if [[ -z "$fw_num" || "$fw_num" -le 0 ]]; then
		echo "Invalid firmware switch count. Please provide a positive integer."
		return 1
	fi
	echo "Starting firmware switch test with $fw_num times ..."
	sys_test_fw_switch.sh "$fw_num"
}
# ---------------------------------------------------
#         Deep Sleep power test
# ---------------------------------------------------
# Function to deep sleep power test
start_deep_sleep() {
	echo "===================================="
	echo "DUT will run into deep sleep mode ..."
	echo "===================================="
	echo " "
	echo -e -n "\xFF\xFF\x02\x19\x10" > /dev/ttyS5
	sleep 1
	btmcu_daemon notcharge
	sleep 1
	btmcu_daemon deepsleep 1
}
# ---------------------------------------------------
#         AP Keep Alive test
# ---------------------------------------------------
# Function to AP keepalive test
start_ap_keep_alive() {
	echo "===================================="
	echo " Start AP keep alive ... "
	echo "===================================="
	echo " "
	
	echo -e -n "\xFF\xFF\x02\x19\x10" > /dev/ttyS5
	sleep 1
	btmcu_daemon notcharge
	sleep 1
	btmcu_daemon deepsleep 3
}
# ---------------------------------------------------
#      sig process, help by kill -l 
# ---------------------------------------------------
sys_test_term_handler() {

	echo "+++++++ Receive TERM signal +++++++"
	echo "### Reboot test un-finish, total=$stress_test_total !!!"

	# part test result
	system_reboot_test_part_statistics

	echo "+++ Reset init value!"
	system_reboot_stest003_reset_status
	echo "System test exit due to terminate !!!"
	echo " "

	exit 0
}

# ---------------------------------------------------
#      system test beginning
# ---------------------------------------------------
# multi ip or not
# check_device_multi_ip

# ---------------------------------------------------
#  system test start mode
# ---------------------------------------------------
trap 'sys_test_term_handler' TERM

if [ "$#" -eq 1 ] && [ "$1" == "1" ]; then
	SYS_TEST_CTRL="manual"
	echo "-----------------------------------------"
	echo "---- System Test Init By Manual Ctrl ----"
	echo "-----------------------------------------"
fi

# check the start mode
if [ "$SYS_TEST_CTRL" == "auto" ] ; then
	if [ "$st001_test_status" == "0" ] && [ "$st002_test_status" == "0" ] && [ "$st003_test_status" == "0" ] ; then
			exit 0
	else
		if [ "$st001_test_status" == "1" ] ; then
			stress_test_done=$st001_num_done
			stress_test_total=$st001_num_total
		elif [ "$st002_test_status" == "1" ] ; then
			stress_test_done=$st002_num_done
			stress_test_total=$st002_num_total
		elif [ "$st003_test_status" == "1" ] ; then
			stress_test_done=$st003_num_done
			stress_test_total=$st003_num_total
		else
			stress_test_done=0
			stress_test_total=0
		fi

		# counter from 1, not 0
		test_counter=$stress_test_done
		let "test_counter=test_counter+1"
		echo "-----------------------------------------"
		echo "### Lastest test unfinished ($test_counter/$stress_test_total), continue ... ['rkkp t' to cancel]"
		system_reboot_test_part_statistics
		echo "-----------------------------------------"
		echo "Wait 2s ... "
		sleep 2s
		echo " "
	fi
fi

echo "-----------------------------------------"
echo "RV1103G + RK962 System Test Menu: "
echo " "
echo "[1] TCP Server Keep Alive Test "
echo "[2] WIFI Throughput Rate Test "
echo "[3] Reboot Warm Start Test "
echo "[4] WiFi On_off Test "
echo "[5] WiFi Switch AP Test "
echo "[6] Firmware Switch Test "
echo "[7] Deep Sleep(Mode 1) Test "
echo "[8] AP Keep Alive Test "
echo "[Others] Exit system test "
echo " "
echo "-----------------------------------------"
echo "Input your selection: "

if [ "$SYS_TEST_CTRL" == "manual" ] ; then
	read select_test
else
	if [ "$st001_test_status" == "1" ] ; then
		select_test=1
	elif [ "$st002_test_status" == "1" ] ; then
		select_test=2
	elif [ "$st003_test_status" == "1" ] ; then
		select_test=3
	elif [ "$st004_test_status" == "1" ] ; then
		select_test=4
	elif [ "$st005_test_status" == "1" ] ; then
		select_test=5
	elif [ "$st006_test_status" == "1" ] ; then
		select_test=6
	elif [ "$st007_test_status" == "1" ] ; then
		select_test=7
	elif [ "$st008_test_status" == "1" ] ; then
		select_test=8
	else
		select_test=n
	fi
fi

echo "---------------------------------------------"
echo "-- [auto/manual] Select Test Number: $select_test"
echo "---------------------------------------------"

case $select_test in
	1)
		check_network_status
		start_wifi_keep_alive
		;;
	2)
		check_network_status
		wifi_throughput_rate_test
		;;
	3)
		system_reboot_test
		;;
	4)
		read -p "Input repeat numbers of wifi on/off: " onoff_times
		start_wifi_onoff_test "$onoff_times"
		;;
	5)
		read -p "Input repeat numbers of wifi switch APs: " ap_switches
		start_ap_switch_test "$ap_switches"
		;;
	6)
		read -p "Input repeat numbers of firmware switch: " fw_switches
		start_fw_switch "$fw_switches"
		;;
	7)
		start_deep_sleep
		;;
	8)
		check_network_status
		start_ap_keep_alive
		;;
	*)
		echo "*** Exit system test ***"
		echo " "
		exit 1
		;;
esac
