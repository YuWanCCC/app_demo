#!/bin/sh

rcS()
{
	for i in /oem/usr/etc/init.d/S??* ;do

		# Ignore dangling symlinks (if any).
		[ ! -f "$i" ] && continue

		case "$i" in
			*.sh)
				# Source shell script for speed.
				(
					trap - INT QUIT TSTP
					set start
					. $i
				)
				;;
			*)
				# No sh extension, so fork subprocess.
				$i start
				;;
		esac
	done
}

check_linker()
{
	[ ! -L "$2" ] && ln -sf $1 $2
}

open_usb_mass_storage() {
	block_device=`cat /proc/mounts | grep /mnt | awk '{print $1}'`

	if [ "$block_device" == "" ]; then
		echo "block device does not exist."
	else    
		/etc/init.d/S50usbdevice stop   #TODO: close adb

		echo usb_ums_en > /tmp/.usb_config
		echo "ums_block=$block_device" >> /tmp/.usb_config
		echo "ums_block_size=8" >> /tmp/.usb_config
		echo "ums_block_type=fat" >> /tmp/.usb_config

		/etc/init.d/S50usbdevice restart   #TODO: start ums
	fi
}

set_memory_policy()
{
	#To modify swap content, you need to open the kernel configuration:CONFIG_ZRAM=y CONFIG_BLK_DEV=y
	#echo 150M > /sys/block/zram0/disksize
	#echo 75M > /sys/block/zram0/mem_limit
	#mkswap /dev/zram0
	#swapon /dev/zram0
	#echo 60 > /proc/sys/vm/swappiness
	#echo 100 > /proc/sys/vm/watermark_scale_factor

	mem_total=$(grep MemTotal /proc/meminfo | awk '{print $2}')
	min_free_kbytes=$((mem_total / 20))
	echo $min_free_kbytes > /proc/sys/vm/min_free_kbytes
	echo "Set min_free_kbytes to: $min_free_kbytes"
}

network_init()
{
	IFACE=$(ifconfig -a | grep -Eo '^[a-zA-Z0-9]+' | grep -E '^(eth0|end0|en[a-z0-9]*)' | head -n 1)

	if [ -z "$IFACE" ]; then
		echo "No valid ethernet interface found!"
		return 1
	fi
	echo "Using interface: $IFACE"

	ethaddr1=`ifconfig -a $IFACE | grep "HWaddr" | awk '{print $NF}'`

	if [ -f /data/ethaddr.txt ] && [ -s /data/ethaddr.txt ]; then
		ethaddr2=`cat /data/ethaddr.txt`
		if [ "$ethaddr1" == "$ethaddr2" ]; then
			echo "eth HWaddr cfg ok"
		else
			ifconfig $IFACE down
			ifconfig $IFACE hw ether $ethaddr2
		fi
	else
		echo $ethaddr1 > /data/ethaddr.txt
	fi
	ifconfig $IFACE up && udhcpc -i $IFACE > /dev/null 2>&1
	ifconfig lo up
	ifconfig lo 127.0.0.1
}

post_chk()
{
	echo userspace > /sys/devices/system/cpu/cpufreq/policy0/scaling_governor
	echo 1512000 > /sys/devices/system/cpu/cpufreq/policy0/scaling_setspeed

	echo user_space > /sys/class/thermal/thermal_zone0/policy
	echo 0 >  /sys/class/thermal/thermal_zone0/cdev0/cur_state
	echo 0 >  /sys/class/thermal/thermal_zone0/cdev1/cur_state

	# audio init
	asound_sh_dir=/oem/usr/bin
	if [ -f "$asound_sh_dir/asound.sh" ];then
		sh $asound_sh_dir/asound.sh
	fi

	#TODO: ensure /userdata mount done
	cnt=0
	while [ $cnt -lt 30 ];
	do
		cnt=$(( cnt + 1 ))
		if mount | grep -w userdata; then
			break
		fi
		sleep .1
	done

	# if ko exist, install ko first
	default_ko_dir=/ko
	if [ -f "/oem/usr/ko/insmod_ko.sh" ];then
		default_ko_dir=/oem/usr/ko
	fi
	if [ -f "$default_ko_dir/insmod_ko.sh" ];then
		cd $default_ko_dir && sh insmod_ko.sh && cd -
	fi

	#open_usb_mass_storage
	set_memory_policy

	cvr_app &

	network_init &

	cvr_ntp_tool &

	echo 3 > /proc/sys/vm/dirty_background_ratio
	echo 23 > /proc/sys/vm/dirty_ratio
	echo 1500 > /proc/sys/vm/dirty_expire_centisecs
}

rcS

ulimit -c unlimited
echo "/data/core-%p-%e" > /proc/sys/kernel/core_pattern
# echo 0 > /sys/devices/platform/rkcif-mipi-lvds/is_use_dummybuf

echo 1 > /proc/sys/vm/overcommit_memory

post_chk &
