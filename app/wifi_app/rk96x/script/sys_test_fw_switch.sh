#!/bin/sh

cmd=$(realpath "$0")
_DIR=$(dirname "$cmd")
cd "$_DIR" || exit 1

test ! -f "rk96x_env.sh"  || source "rk96x_env.sh"

dev_path=$RK_WIFI_BTMCU_TTYS

# Get current script name (without path and extension)
script_name=$(basename "$0" .sh)

# Get current time in format YYYYMMDD_HHMMSS
current_time=$(date +"%Y%m%d_%H%M%S")

# Check number of iterations
if [ -z "$1" ]; then
    echo "Usage: $0 <number_of_iterations>"
    exit 1
fi

# Get total test iterations
iterations=$1

# Initialize counters
total_runs=0
success_count=0
test_result="0"

# Define log file path with script name and timestamp
test_log_file="/tmp/sys_test/${script_name}_${current_time}.log"

# Ensure log directory exists
mkdir -p /tmp/sys_test

# Get UART device path as global variable
UART_DEV=$RK_WIFI_BTMCU_TTYS

# Function to switch to BT normal firmware
function switch_to_bt_normal() {
    echo "switch to bt normal fw..."

    killall rk_hciattach 2>/dev/null
    sleep 0.2
    killall btmcu_daemon 2>/dev/null
    sleep 0.2

    echo "set bt mcu to maskroom..."
    echo -e -n "\xFF\xFF\x02\x13\x01" > $UART_DEV
    sleep 1

    echo "To normal: hci attach..."
    rk_hciattach -n -s 115200 -m 1 $UART_DEV rockchip 3000000 noflow nosleep 55:66:77:88:99:aa &
  # wait $!

    # Wait for hci0 to appear
    echo "Waiting for hci0 to appear..."
    times=10
    while [ $times -gt 0 ] && [ ! -e "/sys/class/bluetooth/hci0" ]; do
        echo "wait hci0 appear"
        sleep 1
        times=$((times - 1))
    done
    
    if [ -e "/sys/class/bluetooth/hci0" ]; then
        echo "switch to bt normal fw success"
        return 0
    else
        echo "ERROR: switch to bt normal fw failed!"
        return 1
    fi
}

# Function to switch to BT MCU firmware
function switch_to_btmcu() {
    echo "switch to bt-mcu fw..."

    killall rk_hciattach 2>/dev/null
    sleep 0.2

    echo "set bt mcu to maskroom..."
    # Send HCI reset command: 0x01 0x92 0xFC 0x20 followed by 32 bytes of 0x00
    {
        echo -e -n "\x01\x92\xFC\x20"; dd if=/dev/zero bs=32 count=1 2>/dev/null;
    } > $UART_DEV
    sleep 1

    echo "To btmcu: hci attach..."
    rk_hciattach -n -s 115200 -m 2 $UART_DEV rockchip 3000000 noflow nosleep 55:66:77:88:99:aa &
  # wait $!
    sleep 3
    local ret_code=$?

    # Set speed to 115200 to facilitate following switch_to_bt_normal
    stty -F $UART_DEV raw speed 115200
    sleep 0.2

    if [ $ret_code -eq 0 ]; then
        echo "switch to bt-mcu fw success."
        return 0
    else
        echo "ERROR: switch to bt-mcu fw failed!"
        return 1
    fi
}

# Function to display sys resource
function display_sys_resource() {

    echo "Current resource info ---> "
    echo " "

    top -n 1 | tee -a "$test_log_file"
    ps | tee -a "$test_log_file"
    lsof | tee -a "$test_log_file"
    free -h | tee -a "$test_log_file"
    df -h | tee -a "$test_log_file"
    netstat -a | tee -a "$test_log_file"

    echo " "
    sleep 1
}

# Function to reset system state
function fw_switch_reset_sys_state() {

    echo " "
    echo "Reset state: start btmcu_daemon ..."
    echo " "

    killall btmcu_daemon
    sleep 0.2
    btmcu_daemon start $dev_path &

    echo " "

    for i in $(seq 5 -1 1); do
       echo -ne "Will reboot in ${i} seconds ...\r"
       sleep 1
    done

    echo " "

    echo "Wait for system reboot ..."
    echo " "
    echo " "
    sync
}

# Function to release process resource (simplified)
function release_ps_resource() {

    echo "Reset and release resource ... "

    # Kill processes with simplified logic
    for process in tail rkwifibt_app_test rkwifi_server wpa_supplicant dbus-daemon bluetoothd emode_server btmcu_daemon rk_hciattach; do
        if pgrep "$process" > /dev/null 2>&1; then
            echo "Kill $process process"
            killall "$process" 2>/dev/null
            sleep 0.2
        fi
    done

    # remove temp socket
    rm -f /tmp/wpa_ctrl_*

    # sync file system
    sync

#    display_sys_resource
    sleep 1
}

# Function to perform a single test
function perform_test() {
    echo " "
    echo "============================================="
    echo " Begin to perform test $((total_runs))#/$iterations ..." | tee -a "$test_log_file"
    sync
    echo "============================================="

    # reset and release res
    release_ps_resource

    # Test 1: Switch to BT normal firmware
    echo "Test 1: Switching to BT normal firmware" | tee -a "$test_log_file"
    if switch_to_bt_normal; then
        echo "Success: switch to BT normal firmware" | tee -a "$test_log_file"
    else
        echo "Fail: switch to BT normal firmware" | tee -a "$test_log_file"

        display_sys_resource
        test_result="1"
        return 1
    fi
    sync

    echo -ne "\n\n"

    # Test 2: Switch to BT MCU firmware
    echo "Test 2: Switching to BT MCU firmware" | tee -a "$test_log_file"
    if switch_to_btmcu; then
        echo "Success: switch to BT MCU firmware" | tee -a "$test_log_file"
    else
        echo "Fail: switch to BT MCU firmware" | tee -a "$test_log_file"

        display_sys_resource
        test_result="1"
        return 1
    fi
    sync

    # Successfully completed the test
    test_result="0"
    return 0
}

# Run the test multiple times
display_sys_resource

for i in $(seq 1 $iterations); do
    total_runs=$((total_runs + 1))
    perform_test

    echo "------------------------------"
    if [ "$test_result" == "0" ]; then
        success_count=$((success_count + 1))
        echo "Test result: $((total_runs))#/$iterations success" | tee -a "$test_log_file"
    else
        echo "Test result: $((total_runs))#/$iterations failure" | tee -a "$test_log_file"

        exit 1
    fi
    echo "------------------------------"

    # Wait between iterations
    sleep 2
done

# Output final results
echo "---------------------------------"
echo "Test complete. Total: $total_runs/$1, Success: $success_count/$1" | tee -a "$test_log_file"
echo "---------------------------------"

fw_switch_reset_sys_state

# system reset
reboot

exit 0
