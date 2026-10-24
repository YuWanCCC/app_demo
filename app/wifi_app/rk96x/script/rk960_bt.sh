#!/bin/sh

export RK96X_TARGET_PATH=/oem/usr/ko/rk96x
cd /oem/usr/ko
insmod ecc.ko
insmod ecdh_generic.ko
insmod bluetooth.ko
insmod hci_uart.ko

killall rk_hciattach

echo 0 > /sys/class/rfkill/rfkill0/state
sleep 0.6
echo 1 > /sys/class/rfkill/rfkill0/state

rk_hciattach -n -s 115200 -m 1  /dev/ttyS2 rockchip 3000000 flow nosleep 55:66:77:88:99:aa &

