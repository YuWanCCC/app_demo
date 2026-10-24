#!/bin/sh
file_flag_test_arm_aov_system="/userdata/test_arm_aov_system"
file_flag_test_arm_reboot_after_aov="/userdata/test_arm_reboot_after_aov"
file_flag_test_arm_dd_tmp="/userdata/test_arm_dd_tmp"
file_flag_test_arm_venc="/userdata/test_arm_venc"
file_flag_test_arm_vi="/userdata/test_arm_vi"
file_flag_test_arm_vi_reboot_after_aov="/userdata/test_arm_vi_reboot_after_aov"
file_flag_test_arm_vi_dummy_frame="/userdata/test_arm_vi_dummy_frame"
file_flag_test_arm_vi_restart_app="/userdata/test_arm_vi_restart_app"
file_flag_test_arm_vi_multi_frame="/userdata/test_arm_vi_multi_frame"
file_flag_test_arm_vi_multi_frame_dummy="/userdata/test_arm_multi_vi_frame_dummy"
file_flag_test_arm_vi_change_resolution="/userdata/test_arm_vi_change_resolution"
file_flag_test_arm_vi_venc="/userdata/test_arm_vi_venc"
file_flag_test_arm_vi_venc_no_drop_frame="/userdata/test_arm_vi_venc_no_drop_frame"
file_flag_test_arm_vi_wrap_venc="/userdata/test_arm_vi_wrap_venc"
file_flag_test_arm_vi_wrap_venc_no_drop_frame="/userdata/test_arm_vi_wrap_venc_no_drop_frame"
file_flag_test_arm_vi_iva_venc="/userdata/test_arm_vi_iva_venc"
file_flag_test_arm_vi_iva_wrap_venc="/userdata/test_arm_vi_iva_wrap_venc"
file_flag_test_arm_aiisp_iva_venc="/userdata/test_arm_aiisp_iva_venc"
file_flag_test_arm_multi_vi="/userdata/test_arm_multi_vi"
file_flag_test_arm_multi_vi_dummy="/userdata/test_arm_multi_vi_dummy"
file_flag_test_arm_multi_vi_restart_app="/userdata/test_arm_multi_vi_restart_app"
file_flag_test_arm_multi_vi_reboot_after_aov="/userdata/test_arm_multi_vi_reboot_after_aov"
file_flag_test_arm_multi_vi_multi_frame="/userdata/test_arm_multi_vi_multi_frame"
file_flag_test_arm_multi_vi_multi_frame_dummy="/userdata/test_arm_multi_vi_multi_frame_dummy"
file_flag_test_arm_multi_vi_multi_frame_half_online="/userdata/test_arm_multi_vi_multi_frame_half_online"
file_flag_test_arm_multi_vi_multi_frame_full_online="/userdata/test_arm_multi_vi_multi_frame_full_online"
file_flag_test_arm_multi_vi_change_resolution="/userdata/test_arm_multi_vi_change_resolution"
file_flag_test_arm_multi_vi_venc="/userdata/test_arm_multi_vi_venc"
file_flag_test_arm_multi_vi_iva_venc="/userdata/test_arm_multi_vi_iva_venc"
file_flag_test_arm_multi_vi_iva_venc_restart_app="/userdata/test_arm_multi_vi_iva_venc_restart_app"
file_flag_test_arm_multi_vi_nonblend_stitch_venc_online="/userdata/test_arm_multi_vi_nonblend_stitch_venc_online"
file_flag_test_arm_multi_vi_nonblend_stitch_venc_online_soft_wrap="/userdata/test_arm_multi_vi_nonblend_stitch_venc_online_soft_wrap"
file_flag_test_arm_multi_vi_nonblend_stitch_venc_offline="/userdata/test_arm_multi_vi_nonblend_stitch_venc_offline"
file_flag_test_arm_multi_vi_nonblend_stitch_venc_offline_soft_wrap="/userdata/test_arm_multi_vi_nonblend_stitch_venc_offline_soft_wrap"
file_flag_test_arm_multi_vi_nonblend_stitch_venc_half_online="/userdata/test_arm_multi_vi_nonblend_stitch_venc_half_online"
file_flag_test_arm_multi_vi_nonblend_stitch_venc_half_online_soft_wrap="/userdata/test_arm_multi_vi_nonblend_stitch_venc_half_online_soft_wrap"
file_flag_test_npu="/userdata/test_arm_npu"
file_flag_test_arm_soundcard_bind_unbind="/userdata/test_arm_soundcard_bind_unbind"
file_flag_test_arm_sdcard_bind_unbind="/userdata/test_arm_sdcard_bind_unbind"
file_flag_test_arm_usb_bind_unbind="/userdata/test_arm_usb_bind_unbind"
file_flag_test_arm_sdio_bind_unbind="/userdata/test_arm_sdio_bind_unbind"
file_flag_test_arm_emmc_bind_unbind="/userdata/test_arm_emmc_bind_unbind"
file_flag_test_arm_multi_vi_nonblend_stitch_venc_half_online_captureimage="/userdata/test_arm_multi_vi_nonblend_stitch_venc_half_online_captureimage"
file_flag_test_arm_multi_vi_nonblend_stitch_venc_half_online_preview="/userdata/test_arm_multi_vi_nonblend_stitch_venc_half_online_preview"
file_flag_test_arm_vi_iva_wrap_venc_captureimage="/userdata/test_arm_vi_iva_wrap_venc_captureimage"
file_flag_test_arm_vi_iva_wrap_venc_preview="/userdata/test_arm_vi_iva_wrap_venc_preview"
file_flag_test_arm_aoa_capture_audio_100ms="/userdata/test_arm_aoa_capture_audio_100ms"
file_flag_test_arm_aoa_capture_audio_1500ms="/userdata/test_arm_aoa_capture_audio_1500ms"
file_flag_test_arm_aoa_capture_audio_dynamic="/userdata/test_arm_aoa_capture_audio_dynamic"
file_flag_test_arm_aoa_capture_audio_reinit="/userdata/test_arm_aoa_capture_audio_reinit"

g_test_count=10000
g_test_threshold=0.85
sensor_width=$rk_cam_w
sensor_height=$rk_cam_h
sensor2_width=$rk_cam2_w
sensor2_height=$rk_cam2_h

g_platform="RV1106"
g_storage_type="nand"

extract_resolution() {
	file_path=$1
	rk_cam_w_var=$2
	rk_cam_h_var=$3

	if [ ! -f "$file_path" ]; then
		echo "文件不存在: $file_path"
		return 1
	fi

	resolution=$(grep -o "Size:[0-9]*x[0-9]*" "$file_path" | cut -d':' -f2)
	if [ -n "$resolution" ]; then
		rk_cam_w=$(echo "$resolution" | cut -d'x' -f1)
		rk_cam_h=$(echo "$resolution" | cut -d'x' -f2)
		eval export "$rk_cam_w_var=$rk_cam_w"
		eval export "$rk_cam_h_var=$rk_cam_h"
	else
		echo "未从 $file_path 中找到分辨率信息"
		return 1
	fi
}

get_sensor_resolution() {
	# 如果 sensor_width 或 sensor_height 为空，从 /proc/rkisp-vir0 中提取分辨率信息
	if [ -z "$sensor_width" ] || [ -z "$sensor_height" ]; then
		extract_resolution "/proc/rkisp-vir0" "sensor_width" "sensor_height"
	else
		echo "sensor_width=$sensor_width, sensor_height=$sensor_height 已设置"
	fi

	# 如果 sensor2_width 或 sensor2_height 为空，从 /proc/rkisp-vir1 中提取分辨率信息
	if [ -z "$sensor2_width" ] || [ -z "$sensor2_height" ]; then
		extract_resolution "/proc/rkisp-vir1" "sensor2_width" "sensor2_height"
	else
		echo "sensor2_width=$sensor2_width, sensor2_height=$sensor2_height 已设置"
	fi

	# 显示最终的分辨率信息
	echo "最终的分辨率信息:"
	echo "sensor_width=$sensor_width, sensor_height=$sensor_height"
	echo "sensor2_width=$sensor2_width, sensor2_height=$sensor2_height"
}

get_storage_info() {
	STORAGE_TYPE=$(cat /proc/cmdline | grep -o "storagemedia=[^ ]*" | head -1 | cut -d'=' -f2)
	echo "Storage type: $STORAGE_TYPE"

	if [ "$STORAGE_TYPE" = "emmc" ]; then
		g_storage_type="emmc"
	fi
}

get_platform_info() {
	g_platform="RV1106"

	model=$(cat /sys/firmware/devicetree/base/model)
	case "$model" in
	*RV1126B*)
		echo "AOV init in RV1126B Platform"
		io -4 0x20834310 3276
		echo "21d60000.mmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
		g_platform="RV1126B"
		;;
	*RV1126*)
		echo "AOV init in RV1126 Platform"
		io -4 0xff3e0048 9100
		echo "ffc60000.dwmmc" >/sys/bus/platform/drivers/dwmmc_rockchip/unbind
		g_platform="RV1126"
		;;
	*RV1106B*)
		echo "AOV init in RV1106B Platform"
		io -4 0x20514094 3276
		g_platform="RV1106B"
		echo "20d20000.mmc" >/sys/bus/platform/drivers/dwmmc_rockchip/unbind
		;;
	*)
		io -4 0xff300048 3276
		echo "ffaa0000.mmc" >/sys/bus/platform/drivers/dwmmc_rockchip/unbind
		echo "AOV init in RV1106 Platform"
		;;
	esac
}

unbind_emmc_if_needed() {
	get_storage_info
	if [ "$g_storage_type" != "emmc" ]; then
		echo "storage type is $g_storage_type,  not unbind"
		return 0
	fi
	echo "storage type is $g_storage_type, emmc is unbind"
	if mount | grep -q "/userdata"; then
		echo "userdata is mounted, unmount it"
		umount /userdata
		sleep 1
		echo "21470000.mmc" > /sys/bus/platform/drivers/dwmmc_rockchip/unbind
    	sleep 1
	fi
	return 0
}

bind_emmc_if_needed() {
	get_storage_info
	if [ "$g_storage_type" != "emmc" ]; then
		echo "storage type is $g_storage_type,  not bind"
		return 0
	fi
	if mount | grep -q "/userdata"; then
		echo "userdata is mounted, not bind"
		return 0
	fi
	echo "storage type is $g_storage_type, emmc is bind"
	echo "21470000.mmc" > /sys/bus/platform/drivers/dwmmc_rockchip/bind
	sleep 1
	mount /dev/block/by-name/userdata userdata/
	sleep 1
	return 0
}

check_test_started() {
	local FILE_PATH=$1
	local NEED_UNBIND_EMMC=1
	if [ -n "$2" ]; then
		NEED_UNBIND_EMMC=$2
	fi

	if [ -f "$FILE_PATH" ]; then
		if grep -q "AOVStartTest" "$FILE_PATH"; then
			echo "Test has started, please check for abnormal restarts !!!"
			rm -rf $FILE_PATH
			touch "$FILE_PATH"_failed
			echo "Test has started, please check for abnormal restarts" >"$FILE_PATH"_failed
			sync
			reboot -f
		else
			echo "File does not contain AOVStartTest, adding it..."
			echo "AOVStartTest" >>"$FILE_PATH"
		fi
	else
		echo "File does not exist, continuing..."
	fi

	if [ "$NEED_UNBIND_EMMC" -eq 1 ]; then
		unbind_emmc_if_needed
	fi
}

set_test_failed_and_reboot() {
	local FILE_PATH=$1
	local NEED_UNBIND_EMMC=1
	if [ -n "$2" ]; then
		NEED_UNBIND_EMMC=$2
	fi

	if [ "$NEED_UNBIND_EMMC" -eq 1 ]; then
		bind_emmc_if_needed
	fi

	if [ -f "$FILE_PATH" ]; then
		touch "$FILE_PATH"_failed
		echo "Test has failed, please check for abnormal restarts" >"$FILE_PATH"_failed
		sync
		reboot -f
	fi
}

set_test_done_and_reboot() {
	local FILE_PATH=$1
	local NEED_BIND_EMMC=1
	if [ -n "$2" ]; then
		NEED_BIND_EMMC=$2
	fi

	if [ "$NEED_BIND_EMMC" -eq 1 ]; then
		bind_emmc_if_needed
	fi

	if [ -f "$FILE_PATH" ]; then
		rm -rf $FILE_PATH
		sync
		reboot -f
	fi
}

check_command_exists() {
	local cmd=$1
	local FILE_PATH=$2

	if ! which "$cmd" >/dev/null 2>&1; then
		echo "Error: Command '$cmd' not found."
		rm -rf $FILE_PATH
		sync
		reboot -f
		return 1
	fi
	return 0
}

## 休眠唤醒10万次后，reboot重启机器
test_arm_aov_system() {
	if [ ! -e "$file_flag_test_arm_aov_system" ]; then
		return
	fi

	get_platform_info

	check_test_started $file_flag_test_arm_aov_system

	test_count=$((g_test_count * 10))
	while [ $test_count -gt 0 ]; do
		echo mem >/sys/power/state
		sleep 0.01
		test_count=$((test_count - 1))
	done

	set_test_done_and_reboot $file_flag_test_arm_aov_system
}

## 休眠唤醒10次后, reboot 重启机器
test_arm_reboot_after_aov() {
	if [ ! -e "$file_flag_test_arm_reboot_after_aov" ]; then
		return
	fi

	get_platform_info

	if [ ! -e "$file_flag_test_arm_reboot_after_aov" ]; then
		echo "$file_flag_test_arm_reboot_after_aov not found, example: echo 10000 $file_flag_test_arm_reboot_after_aov"
		return
	fi

	reboot_count=$(cat "$file_flag_test_arm_reboot_after_aov")
	counter=0
	while [ $counter -lt 10 ]; do
		echo mem >/sys/power/state
		counter=$((counter + 1))
	done

	if [ "$reboot_count" -gt 0 ]; then
		reboot_count=$((reboot_count - 1))
		echo "$reboot_count" >"$file_flag_test_arm_reboot_after_aov"
		sync
		reboot -f
	fi
	set_test_done_and_reboot $file_flag_test_arm_reboot_after_aov
}

## 休眠唤醒g_test_count次后, 并检查/tmp/test.bin 内容
test_arm_dd_tmp() {
	if [ ! -e "$file_flag_test_arm_dd_tmp" ]; then
		return
	fi

	get_platform_info

	check_test_started $file_flag_test_arm_dd_tmp

	mem_available=$(awk '/MemAvailable/ {print $2}' /proc/meminfo)
	mem_available_mb=$((mem_available / 1024))
	dd_count=10
	if [ "$mem_available_mb" -lt 15 ]; then
		dd_count=5
	fi

	dd if=/dev/zero of=/tmp/test.bin bs=1M count=$dd_count
	test_md5_result=$(md5sum /tmp/test.bin | awk '{print $1}')

	counter=0
	while [ $counter -lt "$g_test_count" ]; do
		md5_result=$(md5sum /tmp/test.bin | awk '{print $1}')
		echo "md5sum result = $test_md5_result, $md5_result"
		if [ "$md5_result"x != "$test_md5_result"x ]; then
			echo "/tmp/test_0.bin is error, break test"
			set_test_failed_and_reboot "$file_flag_test_arm_dd_tmp"
			break
		fi
		md5sum /tmp/test.bin
		dd if=/dev/zero of=/tmp/test.bin bs=1M count=$dd_count
		echo mem >/sys/power/state
		counter=$((counter + 1))
	done
	set_test_done_and_reboot $file_flag_test_arm_dd_tmp
}

##  MCU 空跑，MCU 唤醒后马上进入休眠，且后台rk_mpi_venc_test 在编码，拷机35万次 ##
test_arm_venc() {
	if [ ! -e "$file_flag_test_arm_venc" ]; then
		return
	fi

	get_platform_info

	check_test_started $file_flag_test_arm_venc

	rk_mpi_venc_test -w 1920 -h 1080 -C 8 -s 200 -o /tmp/
	if [ ! -f /tmp/test_0.bin ]; then
		echo "rk_mpi_venc_test -w 1920 -h 1080 -C 8 -s 200 -o /tmp/ start faild"
		# touch "$file_flag_test_arm_venc"_failed
		set_test_failed_and_reboot "$file_flag_test_arm_venc"
	fi
	venc0_md5_result=$(md5sum /tmp/test_0.bin | awk '{print $1}')
	counter=0
	venc_run_counter=0
	while [ $counter -lt "$g_test_count" ]; do
		ps | grep rk_mpi_venc_test | grep -v grep
		if [ $? -ne 0 ]; then
			echo "rk_mpi_venc_test not exit, restart"
			md5_result=$(md5sum /tmp/test_0.bin | awk '{print $1}')
			echo "md5sum result = $venc0_md5_result, $md5_result"
			if [ "$md5_result"x != "$venc0_md5_result"x ]; then
				echo "/tmp/test_0.bin is error, break test"
				set_test_failed_and_reboot "$file_flag_test_arm_venc"
				break
			fi
			sleep 1
			rk_mpi_venc_test -w 1920 -h 1080 -C 8 -s 200 -o /tmp/ &
			venc_run_counter=$((venc_run_counter + 1))
		fi
		echo "LoopCount = $counter, vencRunCounter = $venc_run_counter"
		sleep 1
		echo mem >/sys/power/state
		counter=$((counter + 1))
	done
	while [ true ]; do
		ps | grep rk_mpi_venc_test | grep -v grep
		if [ $? -ne 0 ]; then
			break
		fi
		sleep 10
		echo "slee one"
	done
	set_test_done_and_reboot $file_flag_test_arm_venc
}

## vi 单帧压测 ##
test_arm_vi() {
	if [ ! -e "$file_flag_test_arm_vi" ]; then
		return
	fi

	get_platform_info

	check_test_started $file_flag_test_arm_vi

	check_command_exists sample_aov_vi $file_flag_test_arm_vi

	sample_aov_vi -w $sensor_width -h $sensor_height -a /etc/iqfiles/ --aov_loop_count $g_test_count --suspend_time 100
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		set_test_failed_and_reboot "$file_flag_test_arm_vi"
	fi
	set_test_done_and_reboot $file_flag_test_arm_vi
}

## 休眠唤醒10次后, reboot 重启机器
test_arm_vi_reboot_after_aov() {
	if [ ! -e "$file_flag_test_arm_vi_reboot_after_aov" ]; then
		return
	fi

	get_platform_info

	check_command_exists sample_aov_vi $file_flag_test_arm_vi_reboot_after_aov

	sample_aov_vi -w $sensor_width -h $sensor_height -a /etc/iqfiles/ --aov_loop_count 10 --suspend_time 100 --enable_dummy_frame 1

	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		set_test_failed_and_reboot "$file_flag_test_arm_vi_reboot_after_aov"
		echo 0 >"$file_flag_test_arm_reboot_after_aov"
	fi

	reboot_count=$(cat "$file_flag_test_arm_vi_reboot_after_aov")

	if [ "$reboot_count" -gt 0 ]; then
		reboot_count=$((reboot_count - 1))
		echo "$reboot_count" >"$file_flag_test_arm_vi_reboot_after_aov"
		sync
		reboot -f
	fi
	set_test_done_and_reboot $file_flag_test_arm_vi_reboot_after_aov
}

## vi 单帧-手动出帧压测 ##
test_arm_vi_dummy_frame() {
	if [ ! -e "$file_flag_test_arm_vi_dummy_frame" ]; then
		return
	fi

	get_platform_info

	check_test_started $file_flag_test_arm_vi_dummy_frame

	check_command_exists sample_aov_vi $file_flag_test_arm_vi_dummy_frame

	sample_aov_vi -w $sensor_width -h $sensor_height -a /etc/iqfiles/ --aov_loop_count $g_test_count --suspend_time 100 --enable_dummy_frame 1
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		set_test_failed_and_reboot "$file_flag_test_arm_vi_dummy_frame"
	fi

	set_test_done_and_reboot "$file_flag_test_arm_vi_dummy_frame"
}

## vi 进程重启测试 ##
test_arm_vi_restart_app() {
	if [ ! -e "$file_flag_test_arm_vi_restart_app" ]; then
		return
	fi

	get_platform_info

	check_test_started $file_flag_test_arm_vi_restart_app

	check_command_exists sample_aov_vi $file_flag_test_arm_vi_restart_app

	counter=0
	while [ "$counter" -lt $(($g_test_count / 10)) ]; do
		sample_aov_vi -w $sensor_width -h $sensor_height -a /etc/iqfiles/ --aov_loop_count 10 --suspend_time 100
		if [ $? -eq 0 ]; then
			echo "Process exited successfully."
		else
			echo "Process exited with an error."
			set_test_failed_and_reboot "$file_flag_test_arm_vi_restart_app"
			break
		fi
		counter=$((counter + 1))
	done

	set_test_done_and_reboot $file_flag_test_arm_vi_restart_app

}

## vi 单多帧来回切换 ##
test_arm_vi_multi_frame() {
	if [ ! -e "$file_flag_test_arm_vi_multi_frame" ]; then
		return
	fi

	get_platform_info

	check_test_started $file_flag_test_arm_vi_multi_frame

	check_command_exists sample_aov_vi $file_flag_test_arm_vi_multi_frame

	sample_aov_vi -w $sensor_width -h $sensor_height -a /etc/iqfiles/ --aov_loop_count $g_test_count --suspend_time 100 --vi_frame_mode 1
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		set_test_failed_and_reboot "$file_flag_test_arm_vi_multi_frame"
	fi
	set_test_done_and_reboot $file_flag_test_arm_vi_multi_frame
}

## vi 单多帧来回切换-手动出帧压测 ##
test_arm_vi_multi_frame_dummy() {
	if [ ! -e "$file_flag_test_arm_vi_multi_frame_dummy" ]; then
		return
	fi

	get_platform_info

	check_test_started $file_flag_test_arm_vi_multi_frame_dummy

	check_command_exists sample_aov_vi $file_flag_test_arm_vi_multi_frame_dummy

	sample_aov_vi -w $sensor_width -h $sensor_height -a /etc/iqfiles/ --aov_loop_count $g_test_count --suspend_time 100 --vi_frame_mode 1 --enable_dummy_frame 1
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		set_test_failed_and_reboot "$file_flag_test_arm_vi_multi_frame_dummy"
	fi
	set_test_done_and_reboot $file_flag_test_arm_vi_multi_frame_dummy
}

## vi 切换sensor大小分辨率，需要sensor驱动支持 ##
test_arm_vi_change_resolution() {
	if [ ! -e "$file_flag_test_arm_vi_change_resolution" ]; then
		return
	fi

	get_platform_info

	check_test_started $file_flag_test_arm_vi_change_resolution

	check_command_exists sample_aov_vi $file_flag_test_arm_vi_change_resolution

	bin_width=960
	bin_height=540
	bin_fps=120
	if [ "$sensor_width" -eq 1920 ]; then
		bin_width=960
		bin_height=540
		bin_fps=120
	elif [ "$sensor_width" -eq 2688 ]; then
		bin_width=1344
		bin_height=760
		bin_fps=120
	fi

	sample_aov_vi -w $sensor_width -h $sensor_height -a /etc/iqfiles/ --aov_loop_count $g_test_count --suspend_time 100 --vi_frame_mode 1 --enable_dummy_frame 1 --enable_change_resolution 1 --bin_width $bin_width --bin_height $bin_height --bin_fps $bin_fps
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		set_test_failed_and_reboot "$file_flag_test_arm_vi_change_resolution"
	fi
	set_test_done_and_reboot $file_flag_test_arm_vi_change_resolution
}

## vi-venc 压测 ##
test_arm_vi_venc() {
	if [ ! -e "$file_flag_test_arm_vi_venc" ]; then
		return
	fi

	get_platform_info

	check_test_started $file_flag_test_arm_vi_venc

	check_command_exists sample_aov_vi_venc $file_flag_test_arm_vi_venc

	sample_aov_vi_venc -w $sensor_width -h $sensor_height -a /etc/iqfiles/ --aov_loop_count $g_test_count --suspend_time 100 --enable_save_sdcard 0
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		set_test_failed_and_reboot "$file_flag_test_arm_vi_venc"
	fi

	set_test_done_and_reboot $file_flag_test_arm_vi_venc
}

## vi-venc 压测, 不允许编码器丢帧 ##
test_arm_vi_venc_no_drop_frame() {
	if [ ! -e "$file_flag_test_arm_vi_venc_no_drop_frame" ]; then
		return
	fi

	get_platform_info

	check_test_started $file_flag_test_arm_vi_venc_no_drop_frame

	check_command_exists sample_aov_vi_venc $file_flag_test_arm_vi_venc_no_drop_frame

	sample_aov_debug_drop_frame=true sample_aov_vi_venc -w $sensor_width -h $sensor_height -a /etc/iqfiles/ --aov_loop_count $g_test_count --suspend_time 100 --enable_save_sdcard 0
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		set_test_failed_and_reboot "$file_flag_test_arm_vi_venc_no_drop_frame"
	fi

	set_test_done_and_reboot "$file_flag_test_arm_vi_venc_no_drop_frame"
}

## vi-venc 卷绕压测 ##
test_arm_vi_wrap_venc() {
	if [ ! -e "$file_flag_test_arm_vi_wrap_venc" ]; then
		return
	fi

	get_platform_info

	check_test_started $file_flag_test_arm_vi_wrap_venc

	check_command_exists sample_aov_vi_venc $file_flag_test_arm_vi_wrap_venc
	if [ "$sensor_width" -eq 2688 ] || [ "$sensor_width" -eq 3840 ]; then
		sample_aov_vi_venc -w $sensor_width -h $sensor_height -a /etc/iqfiles/ --aov_loop_count $g_test_count --suspend_time 100 --wrap 1 --wrap_lines 0  --enable_save_sdcard 0
	else
		sample_aov_vi_venc -w $sensor_width -h $sensor_height -a /etc/iqfiles/ --aov_loop_count $g_test_count --suspend_time 100 --wrap 1 --enable_save_sdcard 0
	fi
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		set_test_failed_and_reboot "$file_flag_test_arm_vi_wrap_venc"
	fi

	set_test_done_and_reboot "$file_flag_test_arm_vi_wrap_venc"
}

## vi-venc 卷绕压测, 不允许编码器丢帧##
test_arm_vi_wrap_venc_no_drop_frame() {
	if [ ! -e "$file_flag_test_arm_vi_wrap_venc_no_drop_frame" ]; then
		return
	fi

	get_platform_info

	check_test_started $file_flag_test_arm_vi_wrap_venc_no_drop_frame

	check_command_exists sample_aov_vi_venc $file_flag_test_arm_vi_wrap_venc_no_drop_frame

	sample_aov_debug_drop_frame=true sample_aov_vi_venc -w $sensor_width -h $sensor_height -a /etc/iqfiles/ --aov_loop_count $g_test_count --suspend_time 100 --wrap 1 --wrap_lines 0 --enable_save_sdcard 0
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		set_test_failed_and_reboot "$file_flag_test_arm_vi_wrap_venc_no_drop_frame"
	fi

	set_test_done_and_reboot "$file_flag_test_arm_vi_wrap_venc_no_drop_frame"
}

## vi-iva-venc 压测 ##
test_arm_vi_iva_venc() {
	if [ ! -e "$file_flag_test_arm_vi_iva_venc" ]; then
		return
	fi

	check_test_started $file_flag_test_arm_vi_iva_venc

	check_command_exists sample_aov_vi_iva_venc $file_flag_test_arm_vi_iva_venc

	export test_aov_resume_suspend=1
	sample_aov_vi_iva_venc --vi_buff_cnt 1 -w $sensor_width -h $sensor_height -a /etc/iqfiles/ --aov_loop_count $g_test_count --suspend_time 100 --enable_save_sdcard 0 --enable_ivs 0 --enable_iva 0
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		set_test_failed_and_reboot "$file_flag_test_arm_vi_iva_venc"
	fi

	set_test_done_and_reboot "$file_flag_test_arm_vi_iva_venc"
}

## vi-iva-wrap-venc 压测 ##
test_arm_vi_iva_wrap_venc() {
	if [ ! -e "$file_flag_test_arm_vi_iva_wrap_venc" ]; then
		return
	fi

	check_test_started $file_flag_test_arm_vi_iva_wrap_venc

	check_command_exists sample_aov_vi_iva_venc $file_flag_test_arm_vi_iva_wrap_venc

	export test_aov_resume_suspend=1
	if [ "$sensor_width" -eq 2688 ] || [ "$sensor_width" -eq 3840 ]; then
		echo "The sensor is sc450ai, wrap buffer must be one."
		sample_aov_vi_iva_venc -w $sensor_width -h $sensor_height -a /etc/iqfiles/ --aov_loop_count $g_test_count --suspend_time 100 --wrap 1 --wrap_lines 0 --enable_save_sdcard 0 --enable_ivs 0 --enable_iva 0
	else
		sample_aov_vi_iva_venc -w $sensor_width -h $sensor_height -a /etc/iqfiles/ --aov_loop_count $g_test_count --suspend_time 100 --wrap 1 --enable_save_sdcard 0 --enable_ivs 0 --enable_iva 0
	fi

	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		set_test_failed_and_reboot "$file_flag_test_arm_vi_iva_wrap_venc"
	fi

	set_test_done_and_reboot "$file_flag_test_arm_vi_iva_wrap_venc"
}

## vi-iva-wrap-venc 预览 ##
test_arm_vi_iva_wrap_venc_preview() {
	if [ ! -e "$file_flag_test_arm_vi_iva_wrap_venc_preview" ]; then
		return
	fi

	check_test_started $file_flag_test_arm_vi_iva_wrap_venc_preview

	check_command_exists sample_aov_vi_iva_venc $file_flag_test_arm_vi_iva_wrap_venc_preview

	preview_loop_count=$((g_test_count * 9))

	if [ "$sensor_width" -eq 2688 ] || [ "$sensor_width" -eq 3840 ]; then
		echo "The sensor is sc450ai, wrap buffer must be one."
		sample_aov_vi_iva_venc -w $sensor_width -h $sensor_height -a /etc/iqfiles/ --aov_loop_count $preview_loop_count --suspend_time 1000 --wrap 1 --wrap_lines 0 --enable_save_sdcard 1
	else
		sample_aov_vi_iva_venc -w $sensor_width -h $sensor_height -a /etc/iqfiles/ --aov_loop_count $preview_loop_count --suspend_time 1000 --wrap 1 --enable_save_sdcard 1
	fi

	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		set_test_failed_and_reboot "$file_flag_test_arm_vi_iva_wrap_venc_preview"
	fi

	set_test_done_and_reboot "$file_flag_test_arm_vi_iva_wrap_venc_preview"
}

## vi-iva-wrap-venc 抓图 ##
test_arm_vi_iva_wrap_venc_captureimage() {
	if [ ! -e "$file_flag_test_arm_vi_iva_wrap_venc_captureimage" ]; then
		return
	fi

	check_test_started $file_flag_test_arm_vi_iva_wrap_venc_captureimage

	check_command_exists sample_aov_vi_iva_venc $file_flag_test_arm_vi_iva_wrap_venc_captureimage

	if [ "$sensor_width" -eq 2688 ] || [ "$sensor_width" -eq 3840 ]; then
		echo "The sensor is sc450ai, wrap buffer must be one."
		sample_aov_vi_iva_venc -w $sensor_width -h $sensor_height -a /etc/iqfiles/ --aov_loop_count 60 --enable_save_mp4 1 --enable_bypass 0 -r 1 --wrap_lines 0 --enable_ivs 0 --enable_iva 0
	else
		sample_aov_vi_iva_venc -w $sensor_width -h $sensor_height -a /etc/iqfiles/ --aov_loop_count 60 --enable_save_mp4 1 --enable_bypass 0 -r 1 --enable_ivs 0 --enable_iva 0
	fi

	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		set_test_failed_and_reboot "$file_flag_test_arm_vi_iva_wrap_venc_captureimage"
	fi

	set_test_done_and_reboot "$file_flag_test_arm_vi_iva_wrap_venc_captureimage"
}

## vi-aiisp-iva-venc 压测 ##
test_arm_aiisp_iva_venc() {
	if [ ! -e "$file_flag_test_arm_aiisp_iva_venc" ]; then
		return
	fi

	check_test_started $file_flag_test_arm_aiisp_iva_venc

	check_command_exists sample_aov_aiisp_iva_venc $file_flag_test_arm_aiisp_iva_venc

	get_platform_info

	if [ "$g_platform" == "RV1106B" ] || [ "$g_platform" == "RV1126B" ]; then
		echo "$g_platform Skip aiisp test!"
		set_test_done_and_reboot "$file_flag_test_arm_aiisp_iva_venc"
	fi

	sample_aov_aiisp_iva_venc -w $sensor_width -h $sensor_height -a /etc/iqfiles/ --aov_loop_count $g_test_count --suspend_time 100 --enable_save_sdcard 0
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		set_test_failed_and_reboot "$file_flag_test_arm_aiisp_iva_venc"
	fi

	set_test_done_and_reboot "$file_flag_test_arm_aiisp_iva_venc"

}

## multi_vi 单帧压测 ##
test_arm_multi_vi() {
	if [ ! -e "$file_flag_test_arm_multi_vi" ]; then
		return
	fi

	get_platform_info

	check_test_started $file_flag_test_arm_multi_vi

	check_command_exists sample_aov_multi_vi $file_flag_test_arm_multi_vi

	sample_aov_multi_vi -s 0 -w $sensor_width -h $sensor_height -s 1 -w $sensor2_width -h $sensor2_height -a /etc/iqfiles/ --aov_loop_count $g_test_count --suspend_time 100
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		set_test_failed_and_reboot "$file_flag_test_arm_multi_vi"
	fi

	set_test_done_and_reboot "$file_flag_test_arm_multi_vi"
}

## multi_vi 单帧,手动出一帧压测 ##
test_arm_multi_vi_dummy() {
	if [ ! -e "$file_flag_test_arm_multi_vi_dummy" ]; then
		return
	fi

	get_platform_info

	check_test_started $file_flag_test_arm_multi_vi_dummy

	check_command_exists sample_aov_multi_vi $file_flag_test_arm_multi_vi_dummy

	sample_aov_multi_vi -s 0 -w $sensor_width -h $sensor_height -s 1 -w $sensor2_width -h $sensor2_height -a /etc/iqfiles/ --aov_loop_count $g_test_count --suspend_time 100 --enable_dummy_frame 1
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		set_test_failed_and_reboot "$file_flag_test_arm_multi_vi_dummy"
	fi

	set_test_done_and_reboot "$file_flag_test_arm_multi_vi_dummy"
}

## multi_vi 单帧压测 ##
test_arm_multi_vi_restart_app() {
	if [ ! -e "$file_flag_test_arm_multi_vi_restart_app" ]; then
		return
	fi

	get_platform_info

	check_test_started $file_flag_test_arm_multi_vi_restart_app

	check_command_exists sample_aov_multi_vi $file_flag_test_arm_multi_vi_restart_app

	counter=0
	while [ "$counter" -lt $(($g_test_count / 10)) ]; do
		sample_aov_multi_vi -s 0 -w $sensor_width -h $sensor_height -s 1 -w $sensor2_width -h $sensor2_height -a /etc/iqfiles/ --aov_loop_count 10 --suspend_time 100
		if [ $? -eq 0 ]; then
			echo "Process exited successfully."
		else
			echo "Process exited with an error."
			set_test_failed_and_reboot "$file_flag_test_arm_multi_vi_restart_app"
			break
		fi
		counter=$((counter + 1))
	done

	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		set_test_failed_and_reboot "$file_flag_test_arm_multi_vi_restart_app"
	fi

	set_test_done_and_reboot "$file_flag_test_arm_multi_vi_restart_app"
}

## 休眠唤醒10次后, reboot 重启机器
test_arm_multi_vi_reboot_after_aov() {
	if [ ! -e "$file_flag_test_arm_multi_vi_reboot_after_aov" ]; then
		return
	fi

	get_platform_info

	check_command_exists sample_aov_multi_vi $file_flag_test_arm_multi_vi_reboot_after_aov 0

	sample_aov_multi_vi -s 0 -w $sensor_width -h $sensor_height -s 1 -w $sensor2_width -h $sensor2_height -a /etc/iqfiles/ --aov_loop_count 10 --suspend_time 100

	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		set_test_failed_and_reboot "$file_flag_test_arm_multi_vi_reboot_after_aov"
		echo 0 >"$file_flag_test_arm_reboot_after_aov"
	fi

	reboot_count=$(cat "$file_flag_test_arm_multi_vi_reboot_after_aov")

	if [ "$reboot_count" -gt 0 ]; then
		reboot_count=$((reboot_count - 1))
		echo "$reboot_count" >"$file_flag_test_arm_multi_vi_reboot_after_aov"
		sync
		reboot -f
	fi

	set_test_done_and_reboot "$file_flag_test_arm_multi_vi_reboot_after_aov"
}

## multi_vi 单多帧来回切换 ##
test_arm_multi_vi_multi_frame() {
	if [ ! -e "$file_flag_test_arm_multi_vi_multi_frame" ]; then
		return
	fi

	get_platform_info

	check_test_started $file_flag_test_arm_multi_vi_multi_frame

	check_command_exists sample_aov_multi_vi $file_flag_test_arm_multi_vi_multi_frame

	sample_aov_multi_vi -w -s 0 -w $sensor_width -h $sensor_height -s 1 -w $sensor2_width -h $sensor2_height -a /etc/iqfiles/ --aov_loop_count $g_test_count --suspend_time 100 --vi_frame_mode 1
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		set_test_failed_and_reboot "$file_flag_test_arm_multi_vi_multi_frame"
	fi

	set_test_done_and_reboot "$file_flag_test_arm_multi_vi_multi_frame"
}

## multi_vi 单多帧来回切换 ##
test_arm_multi_vi_multi_frame_dummy() {
	if [ ! -e "$file_flag_test_arm_multi_vi_multi_frame_dummy" ]; then
		return
	fi

	get_platform_info

	check_test_started $file_flag_test_arm_multi_vi_multi_frame_dummy

	check_command_exists sample_aov_multi_vi $file_flag_test_arm_multi_vi_multi_frame_dummy

	sample_aov_multi_vi -w -s 0 -w $sensor_width -h $sensor_height -s 1 -w $sensor2_width -h $sensor2_height -a /etc/iqfiles/ --aov_loop_count $g_test_count --suspend_time 100 --vi_frame_mode 1 --enable_dummy_frame 1
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		set_test_failed_and_reboot "$file_flag_test_arm_multi_vi_multi_frame_dummy"
	fi

	set_test_done_and_reboot "$file_flag_test_arm_multi_vi_multi_frame_dummy"
}

## multi_vi 单多帧来回切换，半直通 ##
test_arm_multi_vi_multi_frame_half_online() {
	if [ ! -e "$file_flag_test_arm_multi_vi_multi_frame_half_online" ]; then
		return
	fi

	get_platform_info

	check_test_started $file_flag_test_arm_multi_vi_multi_frame_half_online

	check_command_exists sample_aov_multi_vi $file_flag_test_arm_multi_vi_multi_frame_half_online

	sample_aov_multi_vi -s 0 -w $sensor_width -h $sensor_height -s 1 -w $sensor2_width -h $sensor2_height -a /etc/iqfiles/ --aov_loop_count $g_test_count --suspend_time 100 --vi_frame_mode 1 --enable_dummy_frame 1 --isp_mode 1
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		set_test_failed_and_reboot "$file_flag_test_arm_multi_vi_multi_frame_half_online"
	fi

	set_test_done_and_reboot "$file_flag_test_arm_multi_vi_multi_frame_half_online"
}

## multi_vi 单多帧来回切换，全直通 ##
test_arm_multi_vi_multi_frame_full_online() {
	if [ ! -e "$file_flag_test_arm_multi_vi_multi_frame_full_online" ]; then
		return
	fi

	get_platform_info

	check_test_started $file_flag_test_arm_multi_vi_multi_frame_full_online

	check_command_exists sample_aov_multi_vi $file_flag_test_arm_multi_vi_multi_frame_full_online

	sample_aov_multi_vi -s 0 -w $sensor_width -h $sensor_height -s 1 -w $sensor2_width -h $sensor2_height -a /etc/iqfiles/ --aov_loop_count $g_test_count --suspend_time 100 --vi_frame_mode 1 --enable_dummy_frame 1 --isp_mode 2
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		set_test_failed_and_reboot "$file_flag_test_arm_multi_vi_multi_frame_full_online"
	fi

	set_test_done_and_reboot "$file_flag_test_arm_multi_vi_multi_frame_full_online"
}

## multi_vi 大小分辨率切换，需要sensor驱动支持 ##
test_arm_multi_vi_change_resolution() {
	if [ ! -e "$file_flag_test_arm_multi_vi_change_resolution" ]; then
		return
	fi

	get_platform_info

	check_test_started $file_flag_test_arm_multi_vi_change_resolution

	check_command_exists sample_aov_multi_vi $file_flag_test_arm_multi_vi_change_resolution

	sample_aov_multi_vi -s 0 -w $sensor_width -h $sensor_height -s 1 -w $sensor2_width -h $sensor2_height -a /etc/iqfiles/ --aov_loop_count $g_test_count --suspend_time 100 --vi_frame_mode 1 --enable_dummy_frame 1 --enable_change_resolution 1
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		set_test_failed_and_reboot "$file_flag_test_arm_multi_vi_change_resolution"
	fi

	set_test_done_and_reboot "$file_flag_test_arm_multi_vi_change_resolution"
}

## multi_vi-venc 压测 ##
test_arm_multi_vi_venc() {
	if [ ! -e "$file_flag_test_arm_multi_vi_venc" ]; then
		return
	fi

	get_platform_info

	check_test_started $file_flag_test_arm_multi_vi_venc

	check_command_exists sample_aov_multi_vi_venc $file_flag_test_arm_multi_vi_venc

	sample_aov_multi_vi_venc -v 1 -s 0 -w $sensor_width -h $sensor_height -s 1 -w $sensor2_width -h $sensor2_height -a /etc/iqfiles/ --aov_loop_count $g_test_count --suspend_time 100 --enable_save_sdcard 0 -r 1
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		set_test_failed_and_reboot "$file_flag_test_arm_multi_vi_venc"
	fi

	set_test_done_and_reboot "$file_flag_test_arm_multi_vi_venc"
}

## multi_vi-iva-venc 压测 ##
test_arm_multi_vi_iva_venc() {
	if [ ! -e "$file_flag_test_arm_multi_vi_iva_venc" ]; then
		return
	fi

	get_platform_info

	check_test_started $file_flag_test_arm_multi_vi_iva_venc

	check_command_exists sample_aov_multi_vi_iva_venc $file_flag_test_arm_multi_vi_iva_venc

	export test_aov_resume_suspend=1
	sample_aov_multi_vi_iva_venc --vi_buff_cnt 1 --cif_buff_cnt 1 --cache_buff_size 2 -s 0 -w $sensor_width -h $sensor_height -s 1 -w $sensor2_width -h $sensor2_height -a /etc/iqfiles/ --aov_loop_count $g_test_count --suspend_time 100 --enable_save_sdcard 0 -r 1
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		set_test_failed_and_reboot "$file_flag_test_arm_multi_vi_iva_venc"
	fi

	set_test_done_and_reboot $file_flag_test_arm_multi_vi_iva_venc
}

## multi_vi-iva-venc 测试进程重启
test_arm_multi_vi_iva_venc_restart_app() {
	if [ ! -e "$file_flag_test_arm_multi_vi_iva_venc_restart_app" ]; then
		return
	fi

	get_platform_info

	check_test_started $file_flag_test_arm_multi_vi_iva_venc_restart_app

	check_command_exists sample_aov_multi_vi_iva_venc $file_flag_test_arm_multi_vi_iva_venc_restart_app

	counter=0
	while [ "$counter" -lt $(($g_test_count / 10)) ]; do
		sample_aov_multi_vi_iva_venc --vi_buff_cnt 1 --cif_buff_cnt 1 --cache_buff_size 2 -s 0 -w $sensor_width -h $sensor_height -s 1 -w $sensor2_width -h $sensor2_height -a /etc/iqfiles/ --aov_loop_count 10 --suspend_time 100 --enable_save_sdcard 0 -r 1
		if [ $? -eq 0 ]; then
			echo "Process exited successfully."
		else
			echo "Process exited with an error."
			set_test_failed_and_reboot "$file_flag_test_arm_multi_vi_iva_venc_restart_app"
			break
		fi
		counter=$((counter + 1))
	done

	set_test_done_and_reboot $file_flag_test_arm_multi_vi_iva_venc_restart_app
}

## multi_vi-nonblend-venc 全离线压测 ##
test_arm_multi_vi_nonblend_stitch_venc_offline() {
	if [ ! -e "$file_flag_test_arm_multi_vi_nonblend_stitch_venc_offline" ]; then
		return
	fi

	get_platform_info

	check_test_started $file_flag_test_arm_multi_vi_nonblend_stitch_venc_offline

	check_command_exists sample_aov_multi_vi_nonblend_stitch_venc $file_flag_test_arm_multi_vi_nonblend_stitch_venc_offline

	export test_aov_resume_suspend=1
	sample_aov_multi_vi_nonblend_stitch_venc -w $sensor_width -h $sensor_height -a /etc/iqfiles/ --aov_loop_count $g_test_count --suspend_time 100 --isp_mode 0 --enable_ivs 0 --enable_iva 0 --enable_save_sdcard 0
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		set_test_failed_and_reboot "$file_flag_test_arm_multi_vi_nonblend_stitch_venc_offline"
	fi

	set_test_done_and_reboot $file_flag_test_arm_multi_vi_nonblend_stitch_venc_offline
}
## multi_vi-nonblend-venc 全离线压测soft卷绕编码 ##
test_arm_multi_vi_nonblend_stitch_venc_offline_soft_wrap() {
	if [ ! -e "$file_flag_test_arm_multi_vi_nonblend_stitch_venc_offline_soft_wrap" ]; then
		return
	fi

	get_platform_info

	check_test_started $file_flag_test_arm_multi_vi_nonblend_stitch_venc_offline_soft_wrap

	check_command_exists sample_aov_multi_vi_nonblend_stitch_venc $file_flag_test_arm_multi_vi_nonblend_stitch_venc_offline_soft_wrap

	export test_aov_resume_suspend=1
	sample_aov_multi_vi_nonblend_stitch_venc -w $sensor_width -h $sensor_height -a /etc/iqfiles/ --aov_loop_count $g_test_count --suspend_time 100 --isp_mode 0 --enable_ivs 0 --enable_iva 0 --enable_save_sdcard 0 -r 1 --wrap_lines 0
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		set_test_failed_and_reboot "$file_flag_test_arm_multi_vi_nonblend_stitch_venc_offline_soft_wrap"
	fi

	set_test_done_and_reboot $file_flag_test_arm_multi_vi_nonblend_stitch_venc_offline_soft_wrap
}
## multi_vi-nonblend-venc 半直通压测 ##
test_arm_multi_vi_nonblend_stitch_venc_half_online() {
	if [ ! -e "$file_flag_test_arm_multi_vi_nonblend_stitch_venc_half_online" ]; then
		return
	fi

	get_platform_info

	check_test_started $file_flag_test_arm_multi_vi_nonblend_stitch_venc_half_online

	check_command_exists sample_aov_multi_vi_nonblend_stitch_venc $file_flag_test_arm_multi_vi_nonblend_stitch_venc_half_online

	export test_aov_resume_suspend=1
	sample_aov_multi_vi_nonblend_stitch_venc -w $sensor_width -h $sensor_height -a /etc/iqfiles/ --aov_loop_count $g_test_count --suspend_time 100 --isp_mode 1 --enable_ivs 0 --enable_iva 0 --enable_save_sdcard 0
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		set_test_failed_and_reboot "$file_flag_test_arm_multi_vi_nonblend_stitch_venc_half_online"
	fi

	set_test_done_and_reboot $file_flag_test_arm_multi_vi_nonblend_stitch_venc_half_online
}
## multi_vi-nonblend-venc 半直通压测soft卷绕编码 ##
test_arm_multi_vi_nonblend_stitch_venc_half_online_soft_wrap() {
	if [ ! -e "$file_flag_test_arm_multi_vi_nonblend_stitch_venc_half_online_soft_wrap" ]; then
		return
	fi

	get_platform_info

	check_test_started $file_flag_test_arm_multi_vi_nonblend_stitch_venc_half_online_soft_wrap

	check_command_exists sample_aov_multi_vi_nonblend_stitch_venc $file_flag_test_arm_multi_vi_nonblend_stitch_venc_half_online_soft_wrap

	export test_aov_resume_suspend=1
	sample_aov_multi_vi_nonblend_stitch_venc -w $sensor_width -h $sensor_height -a /etc/iqfiles/ --aov_loop_count $g_test_count --suspend_time 100 --isp_mode 1 --enable_ivs 0 --enable_iva 0 --enable_save_sdcard 0 -r 1 --wrap_lines 0
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		set_test_failed_and_reboot "$file_flag_test_arm_multi_vi_nonblend_stitch_venc_half_online_soft_wrap"
	fi

	set_test_done_and_reboot $file_flag_test_arm_multi_vi_nonblend_stitch_venc_half_online_soft_wrap
}
## multi_vi-nonblend-venc 全直通压测 ##
test_arm_multi_vi_nonblend_stitch_venc_online() {
	if [ ! -e "$file_flag_test_arm_multi_vi_nonblend_stitch_venc_online" ]; then
		return
	fi

	get_platform_info

	check_test_started $file_flag_test_arm_multi_vi_nonblend_stitch_venc_online

	check_command_exists sample_aov_multi_vi_nonblend_stitch_venc $file_flag_test_arm_multi_vi_nonblend_stitch_venc_online

	export test_aov_resume_suspend=1
	sample_aov_multi_vi_nonblend_stitch_venc -w $sensor_width -h $sensor_height -a /etc/iqfiles/ --aov_loop_count $g_test_count --suspend_time 100 --isp_mode 2 --enable_ivs 0 --enable_iva 0 --enable_save_sdcard 0
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		set_test_failed_and_reboot "$file_flag_test_arm_multi_vi_nonblend_stitch_venc_online"
	fi

	set_test_done_and_reboot $file_flag_test_arm_multi_vi_nonblend_stitch_venc_online
}

## multi_vi-nonblend-venc 全直通压测soft卷绕编码 ##
test_arm_multi_vi_nonblend_stitch_venc_online_soft_wrap() {
	if [ ! -e "$file_flag_test_arm_multi_vi_nonblend_stitch_venc_online_soft_wrap" ]; then
		return
	fi

	get_platform_info

	check_test_started $file_flag_test_arm_multi_vi_nonblend_stitch_venc_online_soft_wrap

	check_command_exists sample_aov_multi_vi_nonblend_stitch_venc $file_flag_test_arm_multi_vi_nonblend_stitch_venc_online_soft_wrap

	export test_aov_resume_suspend=1
	sample_aov_multi_vi_nonblend_stitch_venc -w $sensor_width -h $sensor_height -a /etc/iqfiles/ --aov_loop_count $g_test_count --suspend_time 100 --isp_mode 2 --enable_ivs 0 --enable_iva 0 --enable_save_sdcard 0 -r 1 --wrap_lines 0
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		set_test_failed_and_reboot "$file_flag_test_arm_multi_vi_nonblend_stitch_venc_online_soft_wrap"
	fi

	set_test_done_and_reboot $file_flag_test_arm_multi_vi_nonblend_stitch_venc_online_soft_wrap
}

## multi_vi-nonblend-venc 半直通压测-预览 ##
test_arm_multi_vi_nonblend_stitch_venc_half_online_preview() {
	if [ ! -e "$file_flag_test_arm_multi_vi_nonblend_stitch_venc_half_online_preview" ]; then
		return
	fi

	get_platform_info

	check_test_started $file_flag_test_arm_multi_vi_nonblend_stitch_venc_half_online_preview

	check_command_exists sample_aov_multi_vi_nonblend_stitch_venc $file_flag_test_arm_multi_vi_nonblend_stitch_venc_half_online_preview

	preview_loop_count=$((g_test_count * 9))
	sample_aov_multi_vi_nonblend_stitch_venc -w $sensor_width -h $sensor_height -a /etc/iqfiles/ --aov_loop_count $preview_loop_count --suspend_time 1000 --isp_mode 1 --enable_save_mp4 1 -r 1 --wrap_lines 0
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		set_test_failed_and_reboot "$file_flag_test_arm_multi_vi_nonblend_stitch_venc_half_online_preview"
	fi

	set_test_done_and_reboot $file_flag_test_arm_multi_vi_nonblend_stitch_venc_half_online_preview
}

## multi_vi-nonblend-venc 半直通压测-抓图 ##
test_arm_multi_vi_nonblend_stitch_venc_half_online_captureimage() {
	if [ ! -e "$file_flag_test_arm_multi_vi_nonblend_stitch_venc_half_online_captureimage" ]; then
		return
	fi

	get_platform_info

	check_test_started $file_flag_test_arm_multi_vi_nonblend_stitch_venc_half_online_captureimage

	check_command_exists sample_aov_multi_vi_nonblend_stitch_venc $file_flag_test_arm_multi_vi_nonblend_stitch_venc_half_online_captureimage

	preview_loop_count=$((g_test_count * 9))

	sample_aov_multi_vi_nonblend_stitch_venc -w $sensor_width -h $sensor_height -a /etc/iqfiles/ --aov_loop_count 60 --suspend_time 1000 --isp_mode 1 --enable_save_mp4 1 -r 1 --wrap_lines 0
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		set_test_failed_and_reboot "$file_flag_test_arm_multi_vi_nonblend_stitch_venc_half_online_captureimage"
	fi

	set_test_done_and_reboot $file_flag_test_arm_multi_vi_nonblend_stitch_venc_half_online_captureimage
}

## 测试npu ##
test_arm_npu() {
	if [ ! -e "$file_flag_test_npu" ]; then
		return
	fi

	get_platform_info

	check_test_started $file_flag_test_npu

	check_command_exists sample_aov_npu_test $file_flag_test_npu

	sample_aov_npu_test -w 720 -h 480 -l $g_test_count --suspend_time 100
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		set_test_failed_and_reboot "$file_flag_test_npu"
	fi

	set_test_done_and_reboot $file_flag_test_npu
}

## 测试SD卡休眠唤醒流程 ##
test_arm_sdcard_bind_unbind() {
	if [ ! -e "$file_flag_test_arm_sdcard_bind_unbind" ]; then
		return
	fi

	check_test_started $file_flag_test_arm_sdcard_bind_unbind

	check_command_exists sample_aov_bind_test $file_flag_test_arm_sdcard_bind_unbind

	sample_aov_bind_test -s 1 -l $g_test_count
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		set_test_failed_and_reboot "$file_flag_test_arm_sdcard_bind_unbind"
	fi

	set_test_done_and_reboot $file_flag_test_arm_sdcard_bind_unbind
}

## 测试声卡休眠唤醒流程 ##
test_arm_soundcard_bind_unbind() {
	if [ ! -e "$file_flag_test_arm_soundcard_bind_unbind" ]; then
		return
	fi

	check_test_started $file_flag_test_arm_soundcard_bind_unbind

	check_command_exists sample_aov_bind_test $file_flag_test_arm_soundcard_bind_unbind

	sample_aov_bind_test -a 1 -l $g_test_count
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		set_test_failed_and_reboot "$file_flag_test_arm_soundcard_bind_unbind"
	fi

	set_test_done_and_reboot $file_flag_test_arm_soundcard_bind_unbind
}

## 测试usb host休眠唤醒流程 ##
test_arm_usb_bind_unbind() {
	if [ ! -e "$file_flag_test_arm_usb_bind_unbind" ]; then
		return
	fi

	check_test_started $file_flag_test_arm_usb_bind_unbind

	check_command_exists sample_aov_bind_test $file_flag_test_arm_usb_bind_unbind

	sample_aov_bind_test -u 1 -l $g_test_count
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		set_test_failed_and_reboot "$file_flag_test_arm_usb_bind_unbind"
	fi

	set_test_done_and_reboot $file_flag_test_arm_usb_bind_unbind
}

## 测试sdio 休眠唤醒流程 ##
test_arm_sdio_bind_unbind() {
	if [ ! -e "$file_flag_test_arm_sdio_bind_unbind" ]; then
		return
	fi

	check_test_started $file_flag_test_arm_sdio_bind_unbind

	check_command_exists sample_aov_bind_test $file_flag_test_arm_sdio_bind_unbind

	sample_aov_bind_test -i 1 -l $g_test_count
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		set_test_failed_and_reboot "$file_flag_test_arm_sdio_bind_unbind"
	fi

	set_test_done_and_reboot $file_flag_test_arm_sdio_bind_unbind
}

## 测试emmc 休眠唤醒流程 ##
test_arm_emmc_bind_unbind() {

	if [ ! -e "$file_flag_test_arm_emmc_bind_unbind" ]; then
		return
	fi

	get_storage_info

	if [ "$g_storage_type" != "emmc" ]; then
		echo "Storage type is not emmc, skip emmc bind unbind test!"
		set_test_done_and_reboot $file_flag_test_arm_emmc_bind_unbind 0
	fi

	check_test_started $file_flag_test_arm_emmc_bind_unbind

	check_command_exists sample_aov_bind_test $file_flag_test_arm_emmc_bind_unbind

	sample_aov_bind_test -m 1 -l $g_test_count
	if [ $? -eq 0 ]; then
		echo "Process exited successfully."
	else
		echo "Process exited with an error."
		set_test_failed_and_reboot "$file_flag_test_arm_emmc_bind_unbind"
	fi

	set_test_done_and_reboot $file_flag_test_arm_emmc_bind_unbind
}

## 测试arm aoa音频采集 ##
test_arm_aoa_capture_audio_dynamic() {
	if [ ! -e "$file_flag_test_arm_aoa_capture_audio_dynamic" ]; then
		return
	fi

	get_platform_info

	if [ "$g_platform" != "RV1126B" ]; then
		echo "$g_platform Skip aoa capture audio test!"
		set_test_done_and_reboot $file_flag_test_arm_aoa_capture_audio_dynamic
	fi

	check_test_started $file_flag_test_arm_aoa_capture_audio_dynamic
	check_command_exists sample_aov_audio_test $file_flag_test_arm_aoa_capture_audio_dynamic

	touch /tmp/audio_check.log

	dd if=/dev/urandom of=/tmp/random_8m.bin bs=1M count=8
	md5_before=$(md5sum /tmp/random_8m.bin | awk '{print $1}')

	sample_aov_audio_test -m 0 -o /tmp/my_aor.pcm -t 1 -l $g_test_count
	if [ $? -ne 0 ]; then
		echo "capture audio capture test failed."
		fail_msg="capture audio capture test failed.\n"
	fi

	md5_after=$(md5sum /tmp/random_8m.bin | awk '{print $1}')

	if [ "$md5_after" != "$md5_before" ]; then
		echo "Audio data has changed during capture."
		fail_msg="$fail_msg Audio data has changed during capture.\n"
	fi

	echo -e $fail_msg >> /tmp/audio_check.log

	echo "21d60000.mmc" >/sys/bus/platform/drivers/dwmmc_rockchip/bind
	sleep 1
	mount /dev/mmcblk1p1 /mnt/sdcard/
	sleep 1

	mkdir -p /mnt/sdcard/aoa_test

	i=1
	while [ -e "/mnt/sdcard/aoa_test/aoa_pcm$i.pcm" ]; do
		i=$((i+1))
	done
	cp /tmp/my_aor.pcm /mnt/sdcard/aoa_test/aoa_pcm$i.pcm

	i=1
	while [ -e "/mnt/sdcard/aoa_test/aoa_log$i.txt" ]; do
		i=$((i+1))
	done
	cp /tmp/audio_check.log /mnt/sdcard/aoa_test/aoa_log$i.txt

	if [ -n "$fail_msg" ]; then
		echo -e "System instability detected during audio capture test:\n$fail_msg"
		set_test_failed_and_reboot "$file_flag_test_arm_aoa_capture_audio_dynamic"
	else
		echo "Audio capture test completed successfully - system stable."
	fi

	set_test_done_and_reboot $file_flag_test_arm_aoa_capture_audio_dynamic
}

test_arm_aoa_capture_audio_reinit() {
	if [ ! -e "$file_flag_test_arm_aoa_capture_audio_reinit" ]; then
		return
	fi

	get_platform_info

	if [ "$g_platform" != "RV1126B" ]; then
		echo "$g_platform Skip aoa capture audio test!"
		set_test_done_and_reboot $file_flag_test_arm_aoa_capture_audio_reinit
	fi

	check_test_started $file_flag_test_arm_aoa_capture_audio_reinit
	check_command_exists sample_aov_audio_test $file_flag_test_arm_aoa_capture_audio_reinit

	touch /tmp/audio_check.log

	dd if=/dev/urandom of=/tmp/random_8m.bin bs=1M count=8
	md5_before=$(md5sum /tmp/random_8m.bin | awk '{print $1}')

	sample_aov_audio_test -m 0 -o /tmp/my_aor.pcm -t 3 -s 100 -l $g_test_count
	if [ $? -ne 0 ]; then
		echo "capture audio capture test failed."
		fail_msg="capture audio capture test failed.\n"
	fi

	md5_after=$(md5sum /tmp/random_8m.bin | awk '{print $1}')

	if [ "$md5_after" != "$md5_before" ]; then
		echo "Audio data has changed during capture."
		fail_msg="$fail_msg Audio data has changed during capture.\n"
	fi

	echo -e $fail_msg >> /tmp/audio_check.log

	echo "21d60000.mmc" >/sys/bus/platform/drivers/dwmmc_rockchip/bind
	sleep 1
	mount /dev/mmcblk1p1 /mnt/sdcard/
	sleep 1

	mkdir -p /mnt/sdcard/aoa_test

	i=1
	while [ -e "/mnt/sdcard/aoa_test/aoa_pcm$i.pcm" ]; do
		i=$((i+1))
	done
	cp /tmp/my_aor.pcm /mnt/sdcard/aoa_test/aoa_pcm$i.pcm

	i=1
	while [ -e "/mnt/sdcard/aoa_test/aoa_log$i.txt" ]; do
		i=$((i+1))
	done
	cp /tmp/audio_check.log /mnt/sdcard/aoa_test/aoa_log$i.txt

	if [ -n "$fail_msg" ]; then
		echo -e "System instability detected during audio capture test:\n$fail_msg"
		set_test_failed_and_reboot "$file_flag_test_arm_aoa_capture_audio_reinit"
	else
		echo "Audio capture test completed successfully - system stable."
	fi

	set_test_done_and_reboot $file_flag_test_arm_aoa_capture_audio_reinit
}

## 测试arm aoa音频采集 ##
test_arm_aoa_capture_audio_100ms() {
	if [ ! -e "$file_flag_test_arm_aoa_capture_audio_100ms" ]; then
		return
	fi

	get_platform_info

	if [ "$g_platform" != "RV1126B" ]; then
		echo "$g_platform Skip aoa capture audio test!"
		set_test_done_and_reboot $file_flag_test_arm_aoa_capture_audio_100ms 0
	fi

	check_test_started $file_flag_test_arm_aoa_capture_audio_100ms
	check_command_exists sample_aov_audio_test $file_flag_test_arm_aoa_capture_audio_100ms

	touch /tmp/audio_check.log

	dd if=/dev/urandom of=/tmp/random_8m.bin bs=1M count=8
	md5_before=$(md5sum /tmp/random_8m.bin | awk '{print $1}')

	sample_aov_audio_test -m 0 -o /tmp/my_aor.pcm -t 2 -s 100 -l $g_test_count
	if [ $? -ne 0 ]; then
		echo "capture audio capture test failed."
		fail_msg="capture audio capture test failed.\n"
	fi

	aor_check_integrity /tmp/my_aor.pcm $g_test_threshold
	if [ $? -ne 0 ]; then
		echo "check audio capture test failed."
		fail_msg="$fail_msg check audio capture test failed.\n"
	fi

	md5_after=$(md5sum /tmp/random_8m.bin | awk '{print $1}')

	if [ "$md5_after" != "$md5_before" ]; then
		echo "Audio data has changed during capture."
		fail_msg="$fail_msg Audio data has changed during capture.\n"
	fi

	echo -e $fail_msg >> /tmp/audio_check.log

	echo "21d60000.mmc" >/sys/bus/platform/drivers/dwmmc_rockchip/bind
	sleep 1
	mount /dev/mmcblk1p1 /mnt/sdcard/
	sleep 1

	mkdir -p /mnt/sdcard/aoa_test

	i=1
	while [ -e "/mnt/sdcard/aoa_test/aoa_pcm$i.pcm" ]; do
		i=$((i+1))
	done
	cp /tmp/my_aor.pcm /mnt/sdcard/aoa_test/aoa_pcm$i.pcm

	i=1
	while [ -e "/mnt/sdcard/aoa_test/aoa_log$i.txt" ]; do
		i=$((i+1))
	done
	cp /tmp/audio_check.log /mnt/sdcard/aoa_test/aoa_log$i.txt

	if [ -n "$fail_msg" ]; then
		echo -e "System instability detected during audio capture test:\n$fail_msg"
		set_test_failed_and_reboot "$file_flag_test_arm_aoa_capture_audio_100ms"
	else
		echo "Audio capture test completed successfully - system stable."
	fi

	set_test_done_and_reboot $file_flag_test_arm_aoa_capture_audio_100ms
}

## 测试arm aoa音频采集 ##
test_arm_aoa_capture_audio_1500ms() {
	if [ ! -e "$file_flag_test_arm_aoa_capture_audio_1500ms" ]; then
		return
	fi

	get_platform_info

	if [ "$g_platform" != "RV1126B" ]; then
		echo "$g_platform Skip aoa capture audio test!"
		set_test_done_and_reboot $file_flag_test_arm_aoa_capture_audio_1500ms 0
	fi

	check_test_started $file_flag_test_arm_aoa_capture_audio_1500ms
	check_command_exists sample_aov_audio_test $file_flag_test_arm_aoa_capture_audio_1500ms

	touch /tmp/audio_check.log

	dd if=/dev/urandom of=/tmp/random_8m.bin bs=1M count=8
	md5_before=$(md5sum /tmp/random_8m.bin | awk '{print $1}')

	sample_aov_audio_test -m 0 -o /tmp/my_aor.pcm -t 2 -s 1500 -l $g_test_count
	if [ $? -ne 0 ]; then
		echo "capture audio capture test failed."
		fail_msg="capture audio capture test failed.\n"
	fi

	aor_check_integrity /tmp/my_aor.pcm $g_test_threshold
	if [ $? -ne 0 ]; then
		echo "check audio capture test failed."
		fail_msg="$fail_msg check audio capture test failed.\n"
	fi

	md5_after=$(md5sum /tmp/random_8m.bin | awk '{print $1}')

	if [ "$md5_after" != "$md5_before" ]; then
		echo "Audio data has changed during capture."
		fail_msg="$fail_msg Audio data has changed during capture.\n"
	fi

	echo -e $fail_msg >> /tmp/audio_check.log

	echo "21d60000.mmc" >/sys/bus/platform/drivers/dwmmc_rockchip/bind
	sleep 1
	mount /dev/mmcblk1p1 /mnt/sdcard/
	sleep 1

	mkdir -p /mnt/sdcard/aoa_test

	i=1
	while [ -e "/mnt/sdcard/aoa_test/aoa_pcm$i.pcm" ]; do
		i=$((i+1))
	done
	cp /tmp/my_aor.pcm /mnt/sdcard/aoa_test/aoa_pcm$i.pcm

	i=1
	while [ -e "/mnt/sdcard/aoa_test/aoa_log$i.txt" ]; do
		i=$((i+1))
	done
	cp /tmp/audio_check.log /mnt/sdcard/aoa_test/aoa_log$i.txt

	if [ -n "$fail_msg" ]; then
		echo -e "System instability detected during audio capture test:\n$fail_msg"
		set_test_failed_and_reboot "$file_flag_test_arm_aoa_capture_audio_1500ms"
	else
		echo "Audio capture test completed successfully - system stable."
	fi

	set_test_done_and_reboot $file_flag_test_arm_aoa_capture_audio_1500ms
}


touch_test_files() {
	for file in "$@"; do
		touch "$file"
	done
}

# 定义单摄像头测试的函数
single_cam_test() {
	make_meta --update --meta_path /dev/block/by-name/meta --cmdline NoAuto=1
	rm -rf /userdata/rkipc.ini
	cp /oem/usr/bin/resume_suspend_test.sh /userdata/auto_test.sh
	sed -i '1a set -x' /userdata/auto_test.sh
	chmod a+x /userdata/auto_test.sh
	touch /userdata/auto_userdata_test
	rm -rf /userdata/test_arm_*

	touch_test_files \
		"$file_flag_test_arm_reboot_mcu_run_sleep10ms" \
		"$file_flag_test_arm_aov_system" \
		"$file_flag_test_arm_reboot_after_aov" \
		"$file_flag_test_arm_dd_tmp" \
		"$file_flag_test_arm_venc" \
		"$file_flag_test_arm_vi" \
		"$file_flag_test_arm_vi_reboot_after_aov" \
		"$file_flag_test_arm_vi_dummy_frame" \
		"$file_flag_test_arm_vi_restart_app" \
		"$file_flag_test_arm_vi_multi_frame" \
		"$file_flag_test_arm_vi_multi_frame_dummy" \
		"$file_flag_test_arm_vi_change_resolution" \
		"$file_flag_test_arm_vi_venc" \
		"$file_flag_test_arm_vi_venc_no_drop_frame" \
		"$file_flag_test_arm_vi_wrap_venc" \
		"$file_flag_test_arm_vi_wrap_venc_no_drop_frame" \
		"$file_flag_test_arm_vi_iva_venc" \
		"$file_flag_test_arm_vi_iva_wrap_venc" \
		"$file_flag_test_arm_aiisp_iva_venc" \
		"$file_flag_test_npu" \
		"$file_flag_test_sdcard_bind_unbind" \
		"$file_flag_test_arm_soundcard_bind_unbind" \
		"$file_flag_test_arm_usb_bind_unbind" \
		"$file_flag_test_arm_sdio_bind_unbind" \
		"$file_flag_test_arm_emmc_bind_unbind"

	if [ -n "$2" ]; then
		g_test_count="$2"
		sed -i "s/g_test_count=10000/g_test_count=$g_test_count/g" /userdata/auto_test.sh
	fi

	# for aotu test
	if [ -n "$3" ]; then
		touch_test_files \
			"$file_flag_test_arm_vi_iva_wrap_venc_captureimage" \
			"$file_flag_test_arm_vi_iva_wrap_venc_preview"
	fi

	echo "wakeup test: g_test_count = $g_test_count"
	echo $((g_test_count / 10)) >"$file_flag_test_arm_reboot_after_aov"
	echo $((g_test_count / 10)) >"$file_flag_test_arm_vi_reboot_after_aov"
	sync
	reboot -f
}

# 定义单摄像头图像测试的函数
single_cam_image_test() {
	aov_loop_count="${2:-60}"
	boot_frame="${3:-60}"
	mirror_flip="${4:-0}"

	make_meta --update --meta_path /dev/block/by-name/meta --cmdline NoAuto=1

	rm -rf /userdata/auto_test.sh

	run_cmd="sample_aov_vi_iva_venc -w $sensor_width -h $sensor_height -a /etc/iqfiles/ --enable_save_mp4 1 --enable_bypass 0 -r 1 --wrap_lines 0 --enable_ivs 0 --enable_iva 0 --aov_loop_count $aov_loop_count --boot_frame $boot_frame --camera_mirror_flip $mirror_flip --enable_draw_2a 1"
	echo "$run_cmd" >/userdata/auto_test.sh

	touch /userdata/auto_userdata_test
	chmod a+x /userdata/auto_test.sh
	sync
	reboot -f
}

# 定义单摄像头图像预览测试
single_cam_preview_test() {
	aov_loop_count="${2:--1}"
	mirror_flip="${3:-0}"
	encode="${4:-h265vbr}"
	make_meta --update --meta_path /dev/block/by-name/meta --cmdline NoAuto=1

	rm -rf /userdata/auto_test.sh

	if [ "$sensor_width" -eq 2688 ] || [ "$sensor_width" -eq 3840 ]; then
		echo "The sensor is sc450ai, wrap buffer must be one."
		run_cmd="sample_aov_vi_iva_venc -w $sensor_width -h $sensor_height -a /etc/iqfiles/ --enable_save_mp4 1 --enable_bypass 1 -r 1 --wrap_lines 0  --aov_loop_count $aov_loop_count --camera_mirror_flip $mirror_flip --enable_draw_2a 1 -e $encode"
	else
		run_cmd="sample_aov_vi_iva_venc -w $sensor_width -h $sensor_height -a /etc/iqfiles/ --enable_save_mp4 1 --enable_bypass 1 -r 1 --aov_loop_count $aov_loop_count --camera_mirror_flip $mirror_flip --enable_draw_2a 1 -e $encode"
	fi

	if [ "$g_platform" != "RV1126B" ]; then
		echo "export AOA_DEBUG=1">/userdata/auto_test.sh
	fi

	echo "$run_cmd" >> /userdata/auto_test.sh

	touch /userdata/auto_userdata_test
	chmod a+x /userdata/auto_test.sh
	sync
	reboot -f
}

# 定义多摄像头测试的函数
multi_cam_test() {
	make_meta --update --meta_path /dev/block/by-name/meta --cmdline NoAuto=1
	rm -rf /userdata/rkipc.ini
	cp /oem/usr/bin/resume_suspend_test.sh /userdata/auto_test.sh
	sed -i '1a set -x' /userdata/auto_test.sh
	chmod a+x /userdata/auto_test.sh
	touch /userdata/auto_userdata_test
	rm -rf /userdata/test_arm_*

	touch_test_files \
		"$file_flag_test_arm_reboot_mcu_run_sleep10ms" \
		"$file_flag_test_arm_aov_system" \
		"$file_flag_test_arm_reboot_after_aov" \
		"$file_flag_test_arm_dd_tmp" \
		"$file_flag_test_arm_venc" \
		"$file_flag_test_arm_multi_vi" \
		"$file_flag_test_arm_multi_vi_dummy" \
		"$file_flag_test_arm_multi_vi_reboot_after_aov" \
		"$file_flag_test_arm_multi_vi_multi_frame" \
		"$file_flag_test_arm_multi_vi_multi_frame_dummy" \
		"$file_flag_test_arm_multi_vi_multi_frame_half_online" \
		"$file_flag_test_arm_multi_vi_multi_frame_full_online" \
		"$file_flag_test_arm_multi_vi_change_resolution" \
		"$file_flag_test_arm_multi_vi_venc" \
		"$file_flag_test_arm_multi_vi_iva_venc" \
		"$file_flag_test_arm_multi_vi_nonblend_stitch_venc_online" \
		"$file_flag_test_arm_multi_vi_nonblend_stitch_venc_online_soft_wrap" \
		"$file_flag_test_arm_multi_vi_nonblend_stitch_venc_offline" \
		"$file_flag_test_arm_multi_vi_nonblend_stitch_venc_offline_soft_wrap" \
		"$file_flag_test_arm_multi_vi_nonblend_stitch_venc_half_online" \
		"$file_flag_test_arm_multi_vi_nonblend_stitch_venc_half_online_soft_wrap" \
		"$file_flag_test_npu" \
		"$file_flag_test_sdcard_bind_unbind" \
		"$file_flag_test_arm_soundcard_bind_unbind" \
		"$file_flag_test_arm_usb_bind_unbind" \
		"$file_flag_test_arm_sdio_bind_unbind" \
		"$file_flag_test_arm_emmc_bind_unbind"
	if [ -n "$2" ]; then
		g_test_count="$2"
		sed -i "s/g_test_count=10000/g_test_count=$g_test_count/g" /userdata/auto_test.sh
	fi

	# for aotu test
	if [ -n "$3" ]; then
		touch_test_files \
			"$file_flag_test_arm_multi_vi_nonblend_stitch_venc_half_online_captureimage" \
			"$file_flag_test_arm_multi_vi_nonblend_stitch_venc_half_online_preview"
	fi

	echo "wakeup test: g_test_count = $g_test_count"
	echo $((g_test_count / 10)) >"$file_flag_test_arm_reboot_after_aov"
	echo $((g_test_count / 10)) >"$file_flag_test_arm_multi_vi_reboot_after_aov"
	sync
	reboot -f
}

# 定义双摄像头图像测试的函数
multi_cam_image_test() {
	aov_loop_count="${2:-60}"
	boot_frame="${3:-60}"
	mirror_flip="${4:-0}"

	make_meta --update --meta_path /dev/block/by-name/meta --cmdline NoAuto=1

	rm -rf /userdata/auto_test.sh
	run_cmd="sample_aov_multi_vi_nonblend_stitch_venc -w $sensor_width -h $sensor_height -a /etc/iqfiles/ --isp_mode 1 --enable_save_mp4 1 --enable_sub_stream 0  --enable_iva 0 --aov_loop_count $aov_loop_count --boot_frame $boot_frame --camera_mirror_flip $mirror_flip --enable_draw_2a 1 -r 1 --wrap_lines 0"
	echo "$run_cmd" >/userdata/auto_test.sh

	touch /userdata/auto_userdata_test
	chmod a+x /userdata/auto_test.sh
	sync
	reboot -f
}

# 定义双摄像头图像预览测试
multi_cam_preview_test() {
	aov_loop_count="${2:--1}"
	mirror_flip="${3:-0}"
	encode="${4:-h265vbr}"
	make_meta --update --meta_path /dev/block/by-name/meta --cmdline NoAuto=1

	rm -rf /userdata/auto_test.sh
	run_cmd="sample_aov_multi_vi_nonblend_stitch_venc -w $sensor_width -h $sensor_height -a /etc/iqfiles/ --isp_mode 1 --enable_save_mp4 1  --aov_loop_count $aov_loop_count --camera_mirror_flip $mirror_flip --enable_draw_2a 1 -e $encode -r 1 --wrap_lines 0"
	echo "$run_cmd" >/userdata/auto_test.sh

	touch /userdata/auto_userdata_test
	chmod a+x /userdata/auto_test.sh
	sync
	reboot -f
}
# 定义AOA音频测试
aoa_audio_capture_test(){
	cp /oem/usr/bin/resume_suspend_test.sh /userdata/auto_test.sh
	chmod a+x /userdata/auto_test.sh
	touch /userdata/auto_userdata_test
	rm -rf /userdata/test_arm_*

	if [ -n "$2" ]; then
		input="$2"
		i=0
		len=$(echo "$input" | wc -c)
		len=$((len - 1)) # wc -c 会多算一个换行符
		while [ $i -lt $len ]; do
			num=$(echo "$input" | cut -c $((i+1)))
			case $num in
				"1")
					touch "$file_flag_test_arm_aoa_capture_audio_100ms"
					;;
				"2")
					touch "$file_flag_test_arm_aoa_capture_audio_1500ms"
					;;
				"3")
					touch "$file_flag_test_arm_aoa_capture_audio_dynamic"
					;;
				"4")
					touch "$file_flag_test_arm_aoa_capture_audio_reinit"
					;;
				*)
					echo "Invalid parameters: $num"
					;;
			esac
			i=$((i+1))
		done
	else 
		echo "Missing parameters <test_type>:"
		echo "1: test_arm_aoa_capture_audio_100ms"
		echo "2: test_arm_aoa_capture_audio_1500ms"
		echo "3: test_arm_aoa_capture_audio_dynamic"
		echo "4: test_arm_aoa_capture_audio_reinit"
		exit 0
	fi

	if [ -n "$3" ]; then
		g_test_count="$3"
		sed -i "s/g_test_count=10000/g_test_count=$g_test_count/g" /userdata/auto_test.sh
	fi

	if [ -n "$4" ]; then
		g_test_threshold="$4"
		sed -i "s/g_test_threshold=0.85/g_test_threshold=$g_test_threshold/g" /userdata/auto_test.sh
	fi

	echo "wakeup test: g_test_count = $g_test_count"
	sync
	reboot -f
}

# 显示帮助信息的函数
show_help() {
	echo "Usage: $0 [TestType] [OptionalParams]"
	echo "TestType:"
	echo "  SingleCamTest [TestCount]     - Run single camera test with optional TestCount"
	echo "  SingleCamImageTest [aov_loop_count] [BootFrame] [MirrorFlip] - Run single camera image test with optional aov_loop_count, BootFrame, and MirrorFlip"
	echo "      MirrorFlip options:"
	echo "          0 - No rotation (0°)"
	echo "          1 - Rotate 90°"
	echo "          2 - Rotate 180°"
	echo "          3 - Rotate 270°"
	echo "  SingleCamPreviewTest [aov_loop_count] [mirror_flip]"
	echo "    aov_loop_count  - (Optional) Number of iterations for the loop count. Default is -1."
	echo "    mirror_flip     - (Optional) Mirror flip setting for the camera. Default is 0."
	echo "                       0 - No rotation (0°)"
	echo "                       1 - Rotate 90°"
	echo "                       2 - Rotate 180°"
	echo "                       3 - Rotate 270°"
	echo "  MultiCamTest [TestCount]      - Run multi camera test with optional TestCount"
	echo "  MultiCamImageTest [aov_loop_count] [BootFrame] [mirror_flip] - Run single camera image test with optional aov_loop_count, BootFrame"
	echo "  MultiCamPreviewTest [aov_loop_count] [mirror_flip]"
	echo "    aov_loop_count  - (Optional) Number of iterations for the loop count. Default is -1."
	echo "  AoaAudioCaptureTest  <test_type> [g_test_count] [threshold]"
	echo "    test_type      - (Required)   The tests to run"
	echo "                       1: test_arm_aoa_capture_audio_100ms"
	echo "                       2: test_arm_aoa_capture_audio_1500ms"
	echo "                       3: test_arm_aoa_capture_audio_dynamic"
	echo "                       4: test_arm_aoa_capture_audio_reinit"
	echo "    g_test_count    - (Optional)  Number of loop count. Default is 10000"
	echo "    threshold       - (Optional)  The threshold for the test. Default is 0.85"
	echo "  help                         - Show this help message"
}

# 获取sensor 分辨率
get_sensor_resolution

case "$1"x in
"SingleCamTest"x)
	single_cam_test "$@"
	;;
"SingleCamImageTest"x)
	single_cam_image_test "$@"
	;;
"SingleCamPreviewTest"x)
	single_cam_preview_test "$@"
	;;
"MultiCamTest"x)
	multi_cam_test "$@"
	;;
"MultiCamImageTest"x)
	multi_cam_image_test "$@"
	;;
"MultiCamPreviewTest"x)
	multi_cam_preview_test "$@"
	;;
"AoaAudioCaptureTest"x)
	aoa_audio_capture_test "$@"
	;;
"helpx")
	show_help
	exit
	;;
*)
	echo "Unknown test type: $1"
	;;
esac

sleep 5

export debug_rockit=1
test_arm_aov_system
test_arm_reboot_after_aov
test_arm_dd_tmp
test_arm_venc
test_arm_npu
test_arm_vi
test_arm_vi_reboot_after_aov
test_arm_vi_dummy_frame
test_arm_vi_multi_frame
test_arm_vi_multi_frame_dummy
test_arm_vi_change_resolution
test_arm_vi_restart_app
test_arm_vi_venc
test_arm_vi_venc_no_drop_frame
test_arm_vi_wrap_venc
test_arm_vi_wrap_venc_no_drop_frame
test_arm_vi_iva_venc
test_arm_vi_iva_wrap_venc
test_arm_aiisp_iva_venc
test_arm_multi_vi
test_arm_multi_vi_dummy
test_arm_multi_vi_restart_app
test_arm_multi_vi_reboot_after_aov
test_arm_multi_vi_multi_frame
test_arm_multi_vi_multi_frame_dummy
test_arm_multi_vi_multi_frame_half_online
test_arm_multi_vi_multi_frame_full_online
test_arm_multi_vi_change_resolution
test_arm_multi_vi_venc
test_arm_multi_vi_iva_venc
test_arm_multi_vi_iva_venc_restart_app
# Wait to test this item until the no-flow issue is resolved
if ! which "make_meta" >/dev/null 2>&1; then
	test_arm_multi_vi_nonblend_stitch_venc_online
	#test_arm_multi_vi_nonblend_stitch_venc_online_soft_wrap
fi
test_arm_multi_vi_nonblend_stitch_venc_offline
#test_arm_multi_vi_nonblend_stitch_venc_offline_soft_wrap
test_arm_multi_vi_nonblend_stitch_venc_half_online
test_arm_multi_vi_nonblend_stitch_venc_half_online_soft_wrap
test_arm_soundcard_bind_unbind
test_arm_sdcard_bind_unbind
test_arm_usb_bind_unbind
test_arm_sdio_bind_unbind
test_arm_emmc_bind_unbind
test_arm_multi_vi_nonblend_stitch_venc_half_online_captureimage
test_arm_multi_vi_nonblend_stitch_venc_half_online_preview
test_arm_vi_iva_wrap_venc_captureimage
test_arm_vi_iva_wrap_venc_preview
test_arm_aoa_capture_audio_100ms
test_arm_aoa_capture_audio_1500ms
test_arm_aoa_capture_audio_dynamic
test_arm_aoa_capture_audio_reinit

rm -rf /userdata/auto_userdata_test
if ls /userdata/*_failed; then
	echo " ===== AOV Testing completed, failed: ====="
else
	echo " ===== AOV Testing completed, ok ====="
fi
