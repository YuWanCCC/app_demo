#!/bin/sh
cmd=$(realpath "$0")
_DIR=$(dirname "$cmd")
cd "$_DIR" || exit 1

test ! -f "rk96x_env.sh"  || source "rk96x_env.sh"

# parameters check
if [ $# -ne 1 ]; then
  echo "Usage: $0 <AP count>"
  exit 1
fi

# param
ap_count=$1

# check wpa_supplicant file's path
if [ -f "${RK96X_CONF_PATH}/wpa_supplicant.conf" ]; then
  wpa_supplicant_conf="/data/cfg/wpa_supplicant.conf"
else
  echo "Error: wpa_supplicant.conf not found in /data/cfg/ or /data/"
  exit 1
fi

# get current script name (exclude path and extension name)
script_name=$(basename "$0" .sh)

# output config file path (debug)
echo "Using wpa_supplicant configuration file: $wpa_supplicant_conf"

# get current time , format: YYYYMMDD_HHMMSS
current_time=$(date +"%Y%m%d_%H%M%S")

# define log file path, include script path and time
log_file="/tmp/sys_test/${script_name}_${current_time}.log"

# check log dir exist
mkdir -p /tmp/sys_test

# save runtime configuration to file
echo "save runtime configuration to file brfore test"
wpa_cli -i wlan0 save_config
sleep 1
sync

# init AP and PSK list (split array by newline char)
ap_list=""
psk_list=""

# get AP SSID and PSK from wpa_supplicant.conf
while IFS= read -r line; do
  if echo "$line" | grep -q '^\s*ssid='; then
    ssid=$(echo "$line" | cut -d'"' -f2)
    ap_list="${ap_list}${ssid}"$'\n'
  elif echo "$line" | grep -q '^\s*psk='; then
    psk=$(echo "$line" | cut -d'"' -f2)
    [ "$psk" = "NONE" ] && psk=""
    psk_list="${psk_list}${psk}"$'\n'
  fi
done < "$wpa_supplicant_conf"

echo ">>>> ap_list content start >>>>"
echo "$ap_list"
echo "<<<< ap_list content end <<<<"

# counter nums of AP saved, delete blank firstly
saved_ap_count=$(printf "%s\n" "$ap_list" | sed '/^$/d' | wc -l)

# check nums of AP
if [ $saved_ap_count -lt $ap_count ]; then
  echo "Currently saved APs:" | tee -a "$log_file"
  printf "%s\n" "$ap_list" | sed '/^$/d'
  echo "Need to connect $(($ap_count - $saved_ap_count)) more APs. Please use the rsc command to connect APs and try again." | tee -a "$log_file"
  exit 1
fi

# Convert the AP list and PSK list to read by row
ap_array=$(printf "%s\n" "$ap_list" | sed '/^$/d')
psk_array=$(printf "%s\n" "$psk_list" | sed '/^$/d')

# statistical variable
total_switches=0
successful_switches=0
failed_switches=0
max_failures=5
fail_record=""
blacklist=""

# Generate a random number ranging from 1 to $1 (inclusive)
generate_random_index() {
  max=$1

  if [ -r /dev/urandom ]; then
    raw_hex=$(head -c 2 /dev/urandom | hexdump -e '"%04x"')
    rand_num=$((0x$raw_hex))
  else
    rand_num=$(date +%s)
  fi

  rand_index=$((rand_num % max + 1))
  echo $rand_index
}

reset_wifi() {
  echo "Resetting wlan0 interface and wpa_supplicant..." | tee -a "$log_file"

  # kill legacy wpa_supplicant
  killall wpa_supplicant
  sleep 1

	# reset wlan0
	ifconfig wlan0 down
	sleep 1
	ifconfig wlan0 up
	sleep 1
	
	# restart wpa_supplicant
	wpa_supplicant -B -i wlan0 -c "$wpa_supplicant_conf"
	sleep 2
	
	# restart dhclient(if needed)
	# udhcpc -i wlan0 > /dev/null 2>&1 &
	
	echo "wlan0 and wpa_supplicant restarted." | tee -a "$log_file"
}

trap 'echo; \
      echo "Total switches: $total_switches"; \
      echo "Successful: $successful_switches"; \
      echo "Failed: $failed_switches"; \
      if [ $total_switches -gt 0 ]; then \
        success_rate=$(awk "BEGIN {printf \"%.2f\", ($successful_switches / $total_switches) * 100}"); \
        echo "Success rate: $success_rate%" | tee -a "$log_file"; \
      fi; \
      exit 0' INT

last_index=-1

while true; do
  attempts=0
  max_attempts=$saved_ap_count
  while true; do
    index=$(generate_random_index $saved_ap_count)
    selected_ap=$(echo "$ap_array" | sed -n "${index}p")
    echo "$blacklist" | grep -Fxq "$selected_ap"
    if [ $? -eq 0 ]; then
      attempts=$((attempts + 1))
      if [ $attempts -ge $max_attempts ]; then
        echo "All APs are blacklisted. Exiting..." | tee -a "$log_file"
        exit 1
      fi
      continue
    fi
    if [ "$index" -ne "$last_index" ]; then
      break
    fi
  done

  last_index=$index
  selected_psk=$(echo "$psk_array" | sed -n "${index}p")

  echo "\n--- Switching to AP: $selected_ap ---" | tee -a "$log_file"

  if [ -z "$selected_psk" ]; then
    rkwifi_server connect "$selected_ap"
  else
    rkwifi_server connect "$selected_ap" "$selected_psk"
  fi

  sleep 5

  max_retries=10
  retry_interval=2
  wpa_state="DISCONNECTED"
  connected_ssid=""
  for i in $(seq 1 $max_retries); do
    wpa_state=$(wpa_cli -i wlan0 status | grep 'wpa_state=' | cut -d'=' -f2)
    connected_ssid=$(wpa_cli -i wlan0 status | grep '^ssid=' | cut -d'=' -f2)
    echo "Check $i: wpa_state=$wpa_state, ssid=$connected_ssid" | tee -a "$log_file"
    if [ "$wpa_state" = "COMPLETED" ] && [ "$connected_ssid" = "$selected_ap" ]; then
      break
    fi
    sleep $retry_interval
  done

  if [ "$wpa_state" = "COMPLETED" ] && [ "$connected_ssid" = "$selected_ap" ]; then
    echo "Successfully connected to $selected_ap" | tee -a "$log_file"
    successful_switches=$((successful_switches + 1))
    fail_record=$(echo "$fail_record" | grep -v "^$selected_ap:")
  else
    echo "Failed to connect to $selected_ap" | tee -a "$log_file"
    failed_switches=$((failed_switches + 1))
    current_fail=$(echo "$fail_record" | grep "^$selected_ap:" | cut -d':' -f2)
    [ -z "$current_fail" ] && current_fail=0
    new_fail=$((current_fail + 1))
    fail_record=$(echo "$fail_record" | grep -v "^$selected_ap:")
    fail_record=$(printf "%s\n%s" "$fail_record" "$selected_ap:$new_fail")

    if [ $new_fail -ge $max_failures ]; then
      echo "$selected_ap reached $new_fail failures, added to blacklist." | tee -a "$log_file"
      blacklist=$(printf "%s\n%s" "$blacklist" "$selected_ap")
    fi

    rkwifi_server cancel
    sleep 2
    wpa_state=$(wpa_cli -i wlan0 status | grep 'wpa_state=' | cut -d'=' -f2)
    if [ "$wpa_state" = "INACTIVE" ] || [ "$wpa_state" = "SCANNING" ]; then
      echo "wpa_state abnormal ($wpa_state), resetting wlan0 and wpa_supplicant..." | tee -a "$log_file"
      reset_wifi
    fi
  fi

  total_switches=$((total_switches + 1))
  echo "Total switches: $total_switches, Successful: $successful_switches, Failed: $failed_switches" | tee -a "$log_file"
  sync
  sleep 10
done
#
