#!/bin/bash

LOG_FILE=/data/auto_test_log.txt
APP_BIN="/oem/usr/bin/cvr_remote_cfg_tool"
TEST_RESULT=/data/auto_test_result.txt
MAX_TEST_COUNT=9999999
ERROR_LOG=/data/auto_test_error.log
DEBUG_PREFIX="[CVR_DEBUG]"

# Create a unique identifier for the current test
TEST_ID=$(date '+%Y%m%d%H%M%S')

# Check if APP_BIN exists
if [ ! -f "$APP_BIN" ]; then
	echo "${DEBUG_PREFIX} Error: $APP_BIN does not exist!"
	exit 1
fi

# Capture more signals to ensure clean exit
trap 'onCtrlC' INT SIGTERM SIGINT SIGQUIT SIGTRAP SIGABRT SIGHUP SIGSEGV SIGTSTP

function onCtrlC () {
	echo "${DEBUG_PREFIX} Ctrl+C is captured"
	echo "${DEBUG_PREFIX} Exiting..."
	log "cvr_auto_test's pid = $$"
	# Output test statistics
	printTestStats
	kill -9 $$
	exit
}

function rand(){
	min=$1

	max=$(($2- $min + 1))
	num=$RANDOM
	echo $(($num % $max + $min))
}

function log(){
	content="${DEBUG_PREFIX} [AUTO_TEST-$TEST_ID] $(date '+%Y-%m-%d %H:%M:%S') $@"
	echo $content >> $LOG_FILE && echo -e "\033[41;33m"  ${content}  "\033[0m"
}

function error_log(){
	content="${DEBUG_PREFIX} [ERROR-$TEST_ID] $(date '+%Y-%m-%d %H:%M:%S') $@"
	echo $content >> $ERROR_LOG && echo -e "\033[41;31m"  ${content}  "\033[0m"
}

# Check command execution status
function check_result() {
	if [ $? -ne 0 ]; then
		error_log "Command execution failed: $1"
		return 1
	fi
	return 0
}

# Monitor memory usage
function check_memory() {
	mem_free=$(free | grep "Mem:" | awk '{print $4}')
	mem_total=$(free | grep "Mem:" | awk '{print $2}')
	mem_used_percent=$((100 - ($mem_free * 100) / $mem_total))

	if [ $mem_used_percent -gt 90 ]; then
		error_log "Memory usage too high: ${mem_used_percent}%, trying to free memory..."
		sync
		echo 3 > /proc/sys/vm/drop_caches
		return 1
	fi
	return 0
}

# Print test statistics
function printTestStats() {
	log "============ TEST STATISTICS ============"
	log "Resolution setting count: $resolution_sum"
	log "Record type setting count: $record_sum"
	log "Codec type setting count: $codec_sum"
	log "Alarm trigger count: $alarm_sum"
	log "UI switch count: $ui_sum"
	log "Take photo count: $take_photo_sum"
	log "Reset record count: $reset_record_sum"
	log "AOV mode switch count: $aov_sum"
	log "Total test count: $test_count"
	log "========================================"

	# Save test results
	echo "${DEBUG_PREFIX} Test ID: $TEST_ID" > $TEST_RESULT
	echo "${DEBUG_PREFIX} Test start time: $start_time" >> $TEST_RESULT
	echo "${DEBUG_PREFIX} Test end time: $(date '+%Y-%m-%d %H:%M:%S')" >> $TEST_RESULT
	echo "${DEBUG_PREFIX} Resolution setting count: $resolution_sum" >> $TEST_RESULT
	echo "${DEBUG_PREFIX} Record type setting count: $record_sum" >> $TEST_RESULT
	echo "${DEBUG_PREFIX} Codec type setting count: $codec_sum" >> $TEST_RESULT
	echo "${DEBUG_PREFIX} Alarm trigger count: $alarm_sum" >> $TEST_RESULT
	echo "${DEBUG_PREFIX} UI switch count: $ui_sum" >> $TEST_RESULT
	echo "${DEBUG_PREFIX} Take photo count: $take_photo_sum" >> $TEST_RESULT
	echo "${DEBUG_PREFIX} Reset record count: $reset_record_sum" >> $TEST_RESULT
	echo "${DEBUG_PREFIX} AOV mode switch count: $aov_sum" >> $TEST_RESULT
	echo "${DEBUG_PREFIX} Total test count: $test_count" >> $TEST_RESULT
}

resolution=0
record_type=0
codec_type=0
ui_type=0
randomcommand=0

resolution_sum=0
record_sum=0
codec_sum=0
alarm_sum=0;
ui_sum=0
take_photo_sum=0
reset_record_sum=0
aov_sum=0
test_count=0
error_count=0
start_time=$(date '+%Y-%m-%d %H:%M:%S')

# Display tool help information
$APP_BIN -h

function normal() {
	randomcommand=$(rand 0 4);
	if [[ 0 == $randomcommand ]]
	then
		resolution=$(rand 0 6);
		$APP_BIN -r $resolution
		check_result "Failed to set resolution: $resolution"
		resolution_sum=`expr $resolution_sum + 1`;
		log "cvr_remote_cfg_tool -r $resolution [count: $resolution_sum]"
	elif [[ 1 == $randomcommand ]]
	then
		record_type=$(rand 0 1);
		$APP_BIN -t $record_type
		check_result "Failed to set record type: $record_type"
		record_sum=`expr $record_sum + 1`;
		log "cvr_remote_cfg_tool -t $record_type [count: $record_sum]"
	elif [[ 2 == $randomcommand ]]
	then
		codec_type=$(rand 0 1);
		$APP_BIN -c $codec_type
		check_result "Failed to set codec type: $codec_type"
		codec_sum=`expr $codec_sum + 1`;
		log "cvr_remote_cfg_tool -c $codec_type [count: $codec_sum]"
	elif [[ 3 == $randomcommand ]]
	then
		$APP_BIN -a
		check_result "Failed to trigger alarm"
		alarm_sum=`expr $alarm_sum + 1`;
		log "cvr_remote_cfg_tool -a [count: $alarm_sum]"
	elif [[ 4 == $randomcommand ]]
	then
		$APP_BIN -d
		check_result "Failed to reset recording"
		reset_record_sum=`expr $reset_record_sum + 1`;
		log "cvr_remote_cfg_tool -d [count: $reset_record_sum]"
	fi

	# Check memory usage every 10 iterations
	if [ $((test_count % 10)) -eq 0 ]; then
		check_memory
	fi

	sleep 80
}

function ui() {
	ui_type=$((!$ui_type))
	$APP_BIN -u $ui_type
	check_result "Failed to switch UI: $ui_type"
	ui_sum=`expr $ui_sum + 1`;
	log "cvr_remote_cfg_tool -u $ui_type [count: $ui_sum]"

	sleep 20
}

function take_photo() {
	$APP_BIN -p
	check_result "Failed to take photo"
	take_photo_sum=`expr $take_photo_sum + 1`;
	log "cvr_remote_cfg_tool -p [count: $take_photo_sum]"

	sleep 10
}

function aov()
{
	$APP_BIN -A
	check_result "Failed to switch AOV mode"
	aov_sum=`expr $aov_sum + 1`;
	log "cvr_remote_cfg_tool -A [count: $aov_sum]"

	sleep 300
}

# Main loop
while true;
do
	test_count=$((test_count + 1))
	log "Starting test #$test_count"

	# Exit if maximum test count is reached
	if [ $test_count -gt $MAX_TEST_COUNT ]; then
		log "Reached maximum test count $MAX_TEST_COUNT, test finished"
		printTestStats
		exit 0
	fi

	if [ "$1" == "ui" ]
	then
		ui
	elif [ "$1" == "aov" ]
	then
		aov
	elif [ "$1" == "photo" ]
	then
		take_photo
	else
		normal
	fi

done