#!/bin/sh

post_chk()
{
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

	ifconfig eth0 up
    udhcpc -i eth0 &

	# avoid MIPI screen not displaying when startup
	export rt_vo_disable_vop=0

	echo userspace > /sys/devices/system/cpu/cpufreq/policy0/scaling_governor
	echo 1608000 > /sys/devices/system/cpu/cpufreq/policy0/scaling_setspeed

	yoloworld_demo &
}

ulimit -c unlimited
echo "/data/core-%p-%e" > /proc/sys/kernel/core_pattern

post_chk &
