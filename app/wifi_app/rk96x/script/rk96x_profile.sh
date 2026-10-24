#!/bin/sh

SCRIPT=$(find / -name "rk96x_env.sh" 2>/dev/null | head -n1) && [ -f "$SCRIPT" ] && source "$SCRIPT"

update_ap() {
	local file_name

	if [ -f "${RK96X_CONF_PATH}/wpa_supplicant.conf" ]; then
		file_name=${RK96X_CONF_PATH}/wpa_supplicant.conf
	fi

	if [ ! -f "$file_name" ]; then
		echo "$file_name not exist, please check it!"
		exit 1
	fi

	echo "Add new network to AP's configuration"
	echo " "
	read -p "Need to add new AP's ? (y/n): " answer

	if [ "$answer" == "y" ] ||  [ "$answer" == "Y" ]; then
		read -p "Input new [ SSID ]: " new_ssid
		read -p "Input new [ psk ]( input 'NONE' when no psk): " new_psk

		if [ "$new_psk" == "NONE" ] ||  [ "$new_psk" == "None" ]  ||  [ "$new_psk" == "none" ] ; then
			cat << EOF >> $file_name

network={
	ssid="$new_ssid"
	scan_ssid=1
	key_mgmt=NONE
}
EOF
		else
			cat << EOF >> $file_name

network={
	ssid="$new_ssid"
	scan_ssid=1
	psk="$new_psk"
}
EOF
		fi

		echo "The new AP added: "
		echo "ssid=\"$new_ssid\""

		if [ "$new_psk" == "NONE" ] ||  [ "$new_psk" == "None" ]  ||  [ "$new_psk" == "none" ] ; then
			echo "key_mgmt=NONE"
		else
			echo "psk=\"$new_psk\""
		fi

		echo "Must power-off and restart device !!!"
		echo " "
	else
		echo "No new AP added!"
		echo " "
	fi
}

show_help() {
    cat <<'EOF'
## +++++++++++++++++++++++++++++++++++++++++
## Help: RV1103g Command Abbreviation List
## +++++++++++++++++++++++++++++++++++++++++
## cmd='show cmd abbreviation list'
## rb='reboot'
## rl='reboot bootloader'
## ll='ls -la'
## sr='system resource'
## cls='clear'
## fs='fastboot_server'
## fc='fastboot_client'
## bs='bt_start.sh 1'
## ws='wifi_start.sh 1'
## st='sys_test.sh 1'
## pt='/data/prod_test.sh 1'
## al='cat ap_list.conf'[router list]
## cwpa='cat /data/cfg/wpa_supplicant.conf'[config net]
## vcwpa='vi /data/cfg/wpa_supplicant.conf'[config net]
## wpa='cat wpa_supplicant.conf'
## vwpa='vi wpa_supplicant.conf'
## wu='cat wifi_user.conf'
## vwu='vi wifi_user.conf'
## bu='cat bt_user.conf'
## vbu='vi bt_user.conf'
## tu='cat sys_test.conf'
## vtu='vi sys_test.conf'
## wpas="wpa_supplicant -B -i wlan0 -c ${RK96X_CONF_PATH}/wpa_supplicant.conf -d"
## ud='udhcpc -i wlan0'
## ap='update_ap'
## k0='close linux kernel log (default)'
## k7='open linux kernel log'
## d0='close wifi device firmware logs'
## h0='close wifi host logs in the kernel'
## v11i='set 1.1V internal LDO power supply (default)'
## v11e='set 1.1v external DCDC power supply'
## wcs='wpa_cli status'
## wsp='wpa_cli signal_poll'
## bd='btmcu_daemon'
## rs='rkwifi_server'
## rss='rkwifi_server status'
## rsl='rkwifi_server list'
## rsc='rkwifi_server connect'
## rsd='rkwifi_server disconnect'
## rskp='kp_demo'
## rssc='rkwifi_server scan'
## rssr='rkwifi_server scanr'
## rkkp 5='set bt_auto_start=1'
## rkkp 6='set bt_auto_start=0'
## rkkp 3='set wifi_auto_start=1'
## rkkp 2='set wifi_auto_start=0'
## rkkp 0='set wifi_kernel_log=0'
## rkkp 7='set wifi_kernel_log=7'
## rkkp n='Set wifi fw_no_sleep=0'
## rkkp y='Set wifi fw_no_sleep=1'
## rkkp 1='Set wifi sdio_width=1'
## rkkp 4='Set wifi sdio_width=4'
## rkkp f='Set wifi_rf_log=0'
## rkkp g='Set wifi_rf_log=1'
## rkkp p='Set bt_rf_log=0'
## rkkp q='Set bt_rf_log=1'
## rkkp b='kill bt auto-start'
## rkkp w='kill wifi auto-start'
## rkkp t='kill system test'
## rkkp s='kill rkwifi_server'
## rkkp u='kill uart_test'
## rkkp i='kill iperf'
## rkkp h='rkkp help information'
## +++++++++++++++++++++++++++++++++++++++++
EOF
}

# +++++++++++++++++++++++++++++++++++++++++
# alias list
# +++++++++++++++++++++++++++++++++++++++++
alias cmd='show_help'
alias rb='reboot'
alias rl='reboot bootloader'
alias ll='ls -la'
alias sr='echo " "; echo "========== system resource list =========="; echo "mount map ---> "; mount; echo " "; echo "top info ---> "; top -n 1; echo "process ---> "; ps; echo "file resource ---> "; lsof; echo "mem info ---> "; free -h; echo "disk info ---> "; df -h; echo "net info ---> "; netstat -a; echo "user-ps limit ---> "; ulimit -a; echo "ps thread ---> "; cat /proc/$(ps | grep rkwifi_server | grep -v grep | awk '\''{print $1}'\'')/status; echo " "; cat /proc/$(ps | grep wpa_supplicant | grep -v grep | awk '\''{print $1}'\'')/status; echo " "; cat /proc/$(ps | grep uart_test | grep -v grep | awk '\''{print $1}'\'')/status; echo "=========================================="; echo " "'
alias cls='clear'
alias fs='fastboot_server'
alias fc='fastboot_client'
alias bs="bt_start.sh 1"
alias ws="wifi_start.sh 1"
alias st="sys_test.sh 1"
alias pt='/data/prod_test.sh 1'
alias al="cat ${RK96X_CONF_PATH}/ap_list.conf"
alias cwpa='cat /data/cfg/wpa_supplicant.conf'
alias vcwpa='vi /data/cfg/wpa_supplicant.conf'
alias wpa="cat ${RK96X_CONF_PATH}/wpa_supplicant.conf"
alias vwpa="vi ${RK96X_CONF_PATH}/wpa_supplicant.conf"
alias wu="cat ${RK96X_CONF_PATH}/wifi_user.conf"
alias vwu="vi ${RK96X_CONF_PATH}/wifi_user.conf"
alias bu="cat ${RK96X_CONF_PATH}/bt_user.conf"
alias vbu="vi ${RK96X_CONF_PATH}/bt_user.conf"
alias tu="cat ${RK96X_CONF_PATH}/sys_test.conf"
alias vtu="vi ${RK96X_CONF_PATH}/sys_test.conf"
alias wpas="wpa_supplicant -B -i wlan0 -c ${RK96X_CONF_PATH}/wpa_supplicant.conf -d"
alias ud='udhcpc -i wlan0'
alias ap='update_ap'
alias k0='echo "close linux kernel log (default)."; echo 0 > /proc/sys/kernel/printk'
alias k7='echo "open linux kernel log."; echo 7 > /proc/sys/kernel/printk'
alias d0='echo "close wifi device firmware logs."; if [ -z "$( mount | grep debugfs )" ]; then mount -t debugfs none /sys/kernel/debug; fi; cd /sys/kernel/debug/ieee80211/phy0/rk960/; echo 0 > fw_dbg_level'
alias h0='echo "close wifi host logs in the kernel."; if [ -z "$( mount | grep debugfs )" ]; then mount -t debugfs none /sys/kernel/debug; fi; cd /sys/kernel/debug/ieee80211/phy0/rk960/; echo 0 > drv_dbg_level; echo 0 > drv_dbg_flag'
alias v11i='echo "set v1.1 internal LDO power supply (default)."; if [ -z "$( mount | grep debugfs )" ]; then mount -t debugfs none /sys/kernel/debug; fi; cd /sys/kernel/debug/ieee80211/phy0/rk960/; echo "w:0x48100194,0xF000F to mem."; echo w=0x48100194,0xF000F > mem'
## alias v11e='echo "set v1.1 external DCDC power supply."; echo -e -n "\xFF\xFF\x02\x19\x10" > /dev/ttyS5; uart_test notcharge; if [ -z "$( mount | grep debugfs )" ]; then mount -t debugfs none /sys/kernel/debug; fi; cd /sys/kernel/debug/ieee80211/phy0/rk960/; echo "w:0x48100194,0xF000C to mem."; echo w=0x48100194,0xF000C > mem'
alias v11e='echo "set v1.1 external DCDC power supply."; if [ -z "$( mount | grep debugfs )" ]; then mount -t debugfs none /sys/kernel/debug; fi; cd /sys/kernel/debug/ieee80211/phy0/rk960/; echo "w:0x48100194,0xF000C to mem."; echo w=0x48100194,0xF000C > mem'
alias wcs='wpa_cli status'
alias wsp='wpa_cli signal_poll'
alias bd='btmcu_daemon'
alias rs='rkwifi_server'
alias rss='rkwifi_server status'
alias rsl='rkwifi_server list'
alias rsc='rkwifi_server connect'
alias rsd='rkwifi_server disconnect'
alias rskp='kp_demo'
alias rssc='rkwifi_server scan'
alias rssr='rkwifi_server scanr'
