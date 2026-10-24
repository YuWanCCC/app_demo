#!/bin/sh

# find_best_channel.sh - Find the best WiFi channel with minimal interference
# Based on the find_best_channel function from rk_wifi_softap.c
#
# Usage: find_best_channel.sh [hostapd_config_file]
# Parameters:
#   hostapd_config_file: Path to hostapd configuration file (optional, default: /data/hostapd.conf)
#
# This script will:
# 1. Scan for WiFi networks to analyze channel usage
# 2. Calculate the best channel with minimal interference
# 3. Update the hostapd configuration file with the best channel

# Global variables
WPA_STARTED_BY_SCRIPT=0
BEST_CHANNEL=6  # Default channel
MAX_WAIT_TIMES=10

# Function to log info messages
log_info() {
    echo "[INFO] $1"
}

# Function to log error messages
log_error() {
    echo "[ERROR] $1" >&2
}

# Function to check if wpa_supplicant is running
check_wpa_supplicant() {
    if ps | grep -v grep | grep wpa_supplicant > /dev/null 2>&1; then
        log_info "wpa_supplicant is already running"
        return 0
    else
        log_info "wpa_supplicant is not running"
        return 1
    fi
}

# Function to start wpa_supplicant
start_wpa_supplicant() {
    log_info "Starting wpa_supplicant..."
    
    # Check if wpa_supplicant config exists
    if [ ! -f "/etc/wpa_supplicant/wpa_supplicant.conf" ] && [ ! -f "/etc/wpa_supplicant.conf" ]; then
        log_error "wpa_supplicant configuration file not found"
        return 1
    fi
    
    # Use default wireless interface for embedded system
    WIRELESS_INTERFACE="wlan0"
    
    # Verify the interface exists
    if [ ! -d "/sys/class/net/$WIRELESS_INTERFACE" ]; then
        log_error "Wireless interface $WIRELESS_INTERFACE not found"
        return 1
    fi
    
    log_info "Using wireless interface: $WIRELESS_INTERFACE"
    
    # Start wpa_supplicant
    if [ -f "/etc/wpa_supplicant/wpa_supplicant.conf" ]; then
        wpa_supplicant -B -i "$WIRELESS_INTERFACE" -c /etc/wpa_supplicant/wpa_supplicant.conf
    else
        wpa_supplicant -B -i "$WIRELESS_INTERFACE" -c /etc/wpa_supplicant.conf
    fi
    
    if [ $? -eq 0 ]; then
        log_info "wpa_supplicant started successfully"
        WPA_STARTED_BY_SCRIPT=1
        # Wait a moment for wpa_supplicant to initialize
        sleep 2
        return 0
    else
        log_error "Failed to start wpa_supplicant"
        return 1
    fi
}

# Function to stop wpa_supplicant if started by this script
cleanup_wpa_supplicant() {
    if [ "$WPA_STARTED_BY_SCRIPT" -eq 1 ]; then
        log_info "Stopping wpa_supplicant (started by this script)..."
        pkill wpa_supplicant
        if [ $? -eq 0 ]; then
            log_info "wpa_supplicant stopped successfully"
        else
            log_error "Failed to stop wpa_supplicant"
        fi
    fi
}

# Function to execute scan command
execute_scan() {
    log_info "Scanning for best channel..."
    
    # Execute wpa_cli scan command
    if ! wpa_cli scan > /dev/null 2>&1; then
        log_error "Scan command failed"
        return 1
    fi
    
    return 0
}

# Function to wait for scan completion
wait_scan_completion() {
    log_info "Waiting for scan to complete..."
    wait_count=0
    scan_completed=0
    
    while [ "$wait_count" -lt "$MAX_WAIT_TIMES" ] && [ "$scan_completed" -eq 0 ]; do
        sleep 0.5
        wait_count=$((wait_count + 1))
        
        # Check scan status
        status_output=$(wpa_cli status 2>/dev/null)
        if [ $? -eq 0 ]; then
            if ! echo "$status_output" | grep -q "wpa_state=SCANNING"; then
                scan_completed=1
            fi
        fi
    done
    
    wpa_cli status | grep "wpa_state="
    #wpa_cli scan_result

    if [ "$scan_completed" -eq 0 ]; then
        log_info "Scan may not have completed within $((MAX_WAIT_TIMES / 2)) seconds, proceeding anyway"
    else
        seconds=$((wait_count / 2))
        tenths=$(((wait_count % 2) * 5))
        log_info "Scan completed successfully in $seconds.$tenths seconds"
    fi
    
    return 0
}

# Function to get and parse scan results
parse_scan_results() {
    # Get scan results
    scan_results=$(wpa_cli scan_results 2>/dev/null)
    if [ $? -ne 0 ]; then
        log_error "Failed to get scan results"
        return 1
    fi
    
    # Debug: Print scan results (including header for reference)
    log_info "Complete scan results:"
    echo "$scan_results"
    
    # Validate scan results content
    if [ ${#scan_results} -lt 30 ]; then
        log_error "Scan results appear empty or incomplete, using default channel"
        return 1
    fi
    
    # Initialize channel load array (using temporary files for array simulation)
    TEMP_DIR="/tmp/channel_load_$$"
    mkdir -p "$TEMP_DIR"
    
    # Initialize all channels to 0
    i=1
    while [ "$i" -le 13 ]; do
        echo "0" > "$TEMP_DIR/channel_$i"
        i=$((i + 1))
    done
    
    # Skip header lines and process each result
    # Use grep to skip lines that contain header patterns
    echo "$scan_results" | grep -v "Selected interface" | grep -v "bssid.*frequency.*signal" | while IFS= read -r line; do
        if [ -n "$line" ]; then
            # Parse line: bssid frequency signal flags ssid
            bssid=$(echo "$line" | awk '{print $1}')
            frequency=$(echo "$line" | awk '{print $2}')
            signal=$(echo "$line" | awk '{print $3}')
            
            # Convert frequency to channel number
            if [ "$frequency" -ge 2412 ] && [ "$frequency" -le 2472 ]; then
                channel=$(((frequency - 2407) / 5))
                if [ "$channel" -ge 1 ] && [ "$channel" -le 13 ]; then
                    # Calculate load based on signal strength
                    load=$((signal / 10 + 10))
                    
                    # Ensure load is positive
                    if [ "$load" -lt 1 ]; then
                        load=1
                    fi
                    
                    # Update load for this channel
                    current_load=$(cat "$TEMP_DIR/channel_$channel")
                    new_load=$((current_load + load))
                    echo "$new_load" > "$TEMP_DIR/channel_$channel"
                    
                    # Calculate adjacent channel loads
                    load_adj1=$((load * 3 / 4))
                    load_adj2=$((load / 2))
                    load_adj3=$((load / 4))
                    
                    # Add load to adjacent channels
                    if [ "$channel" -gt 1 ]; then
                        adj_ch=$((channel - 1))
                        current_load=$(cat "$TEMP_DIR/channel_$adj_ch")
                        new_load=$((current_load + load_adj1))
                        echo "$new_load" > "$TEMP_DIR/channel_$adj_ch"
                        
                        if [ "$channel" -gt 2 ]; then
                            adj_ch=$((channel - 2))
                            current_load=$(cat "$TEMP_DIR/channel_$adj_ch")
                            new_load=$((current_load + load_adj2))
                            echo "$new_load" > "$TEMP_DIR/channel_$adj_ch"
                            
                            if [ "$channel" -gt 3 ]; then
                                adj_ch=$((channel - 3))
                                current_load=$(cat "$TEMP_DIR/channel_$adj_ch")
                                new_load=$((current_load + load_adj3))
                                echo "$new_load" > "$TEMP_DIR/channel_$adj_ch"
                            fi
                        fi
                    fi
                    
                    if [ "$channel" -lt 13 ]; then
                        adj_ch=$((channel + 1))
                        current_load=$(cat "$TEMP_DIR/channel_$adj_ch")
                        new_load=$((current_load + load_adj1))
                        echo "$new_load" > "$TEMP_DIR/channel_$adj_ch"
                        
                        if [ "$channel" -lt 12 ]; then
                            adj_ch=$((channel + 2))
                            current_load=$(cat "$TEMP_DIR/channel_$adj_ch")
                            new_load=$((current_load + load_adj2))
                            echo "$new_load" > "$TEMP_DIR/channel_$adj_ch"
                            
                            if [ "$channel" -lt 11 ]; then
                                adj_ch=$((channel + 3))
                                current_load=$(cat "$TEMP_DIR/channel_$adj_ch")
                                new_load=$((current_load + load_adj3))
                                echo "$new_load" > "$TEMP_DIR/channel_$adj_ch"
                            fi
                        fi
                    fi
                    
                    log_info "Found network on channel $channel (frequency $frequency MHz, signal $signal dBm, load $load)"
                fi
            fi
        fi
    done
    
    # Find channel with minimum load
    min_load=999999
    max_load=0
    best_channel=6
    
    i=1
    while [ "$i" -le 13 ]; do
        current_load=$(cat "$TEMP_DIR/channel_$i")
        
        if [ "$current_load" -gt "$max_load" ]; then
            max_load=$current_load
        fi
        
        # Only consider channels 1-11 for best channel selection
        if [ "$i" -le 11 ] && [ "$current_load" -lt "$min_load" ]; then
            min_load=$current_load
            best_channel=$i
        fi
        
        i=$((i + 1))
    done
    
    # Print channel load visualization
    echo ""
    log_info "Channel Load Visualization:"
    
    i=1
    while [ "$i" -le 13 ]; do
        current_load=$(cat "$TEMP_DIR/channel_$i")
        
        # Calculate bar count
        if [ "$max_load" -le 50 ]; then
            bar_count=$current_load
        else
            if [ "$max_load" -gt 0 ]; then
                bar_count=$((current_load * 50 / max_load))
            else
                bar_count=0
            fi
        fi
        
        # Ensure we don't exceed 50 bars
        if [ "$bar_count" -gt 50 ]; then
            bar_count=50
        fi
        
        # Create visual bar
        visual=""
        j=0
        while [ "$j" -lt "$bar_count" ]; do
            visual="${visual}|"
            j=$((j + 1))
        done
        
        # Determine status
        if [ "$i" -eq "$best_channel" ]; then
            status="(BEST)"
        elif [ "$i" -gt 11 ]; then
            status="(FORBIDDEN)"
        else
            status=""
        fi
        
        printf "CH %2d [%3d]: %s %s\n" "$i" "$current_load" "$visual" "$status"
        
        i=$((i + 1))
    done
    
    echo ""
    log_info "Selected best channel: $best_channel with load $min_load (max load: $max_load)"
    
    # Store result for main function
    echo "$best_channel" > "$TEMP_DIR/best_channel"
    
    return 0
}

# Function to cleanup temporary files
cleanup_temp_files() {
    if [ -d "$TEMP_DIR" ]; then
        rm -rf "$TEMP_DIR"
    fi
}

# Signal handler for cleanup
signal_handler() {
    log_info "Received signal, cleaning up..."
    cleanup_temp_files
    cleanup_wpa_supplicant
    exit 1
}



# Function to update hostapd configuration
update_hostapd_config() {
    local config_file="$1"
    local channel="$2"
    
    if [ ! -f "$config_file" ]; then
        log_error "hostapd configuration file not found: $config_file"
        return 1
    fi
    
    log_info "Updating hostapd configuration: $config_file"
    log_info "Setting channel to: $channel"
    
    # Create backup
    cp "$config_file" "${config_file}.bak.$(date +%Y%m%d_%H%M%S)"
    
    # Update channel in config file
    if grep -q "^channel=" "$config_file"; then
        # Replace existing channel line
        sed -i "s/^channel=.*/channel=$channel/" "$config_file"
    else
        # Add channel line if not exists
        echo "channel=$channel" >> "$config_file"
    fi
    
    log_info "hostapd configuration updated successfully"
    return 0
}

# Main function
main() {
    # Use default hostapd config file if not specified
    if [ "$#" -eq 0 ]; then
        hostapd_config="/data/hostapd.conf"
        log_info "Using default hostapd config: $hostapd_config"
    elif [ -z "$1" ]; then
        hostapd_config="/data/hostapd.conf"
        log_info "Using default hostapd config: $hostapd_config"
    else
        hostapd_config="$1"
        log_info "Using specified config file: $hostapd_config"
    fi
    
    # Set up signal handlers
    trap signal_handler INT TERM
    
    log_info "Starting WiFi channel analysis..."
    
    # Check if wpa_supplicant is running, start if needed
    if ! check_wpa_supplicant; then
        if ! start_wpa_supplicant; then
            log_error "Cannot proceed without wpa_supplicant"
            exit 1
        fi
    fi
    
    # Check if wpa_cli is available
    if ! command -v wpa_cli > /dev/null 2>&1; then
        log_error "wpa_cli command not found"
        cleanup_wpa_supplicant
        exit 1
    fi
    
    # Disconnect network if connected
    wpa_cli -i wlan0 disconnect
    
    # Execute scan
    if ! execute_scan; then
        log_error "Scan execution failed, using default channel $BEST_CHANNEL"
        
        # Still try to update hostapd configuration with default channel
        if update_hostapd_config "$hostapd_config" "$BEST_CHANNEL"; then
            log_info "Successfully configured default channel $BEST_CHANNEL in $hostapd_config"
        else
            log_error "Failed to update hostapd configuration with default channel"
        fi
        
        cleanup_wpa_supplicant
        echo "$BEST_CHANNEL"
        exit 0
    fi
    
    # Wait for scan completion
    wait_scan_completion
    
    # Parse scan results and find best channel
    if parse_scan_results; then
        if [ -f "$TEMP_DIR/best_channel" ]; then
            BEST_CHANNEL=$(cat "$TEMP_DIR/best_channel")
        fi
    else
        log_error "Failed to parse scan results, using default channel $BEST_CHANNEL"
    fi
    
    # Update hostapd configuration with the best channel
    if update_hostapd_config "$hostapd_config" "$BEST_CHANNEL"; then
        log_info "Successfully configured channel $BEST_CHANNEL in $hostapd_config"
        cat $hostapd_config
        echo ""
    else
        log_error "Failed to update hostapd configuration, but continuing..."
    fi
    
    # Cleanup
    cleanup_temp_files
    cleanup_wpa_supplicant
    
    # Return
    exit 0
}

# Run main function
if [ "$#" -eq 0 ]; then
    main
else
    main "$@"
fi
