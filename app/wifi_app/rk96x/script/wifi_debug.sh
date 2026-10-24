#!/bin/bash

search_base="/"
rk96x_prefix="rk96x"
user_data_dir="/userdata"

check_command() {
    if [ $? -ne 0 ]; then
        echo "error: $1 failed!" >&2
        exit 1
    fi
}

find "$search_base" -type d -name "$rk96x_prefix" | while read -r rk96x_dir; do
    find "$rk96x_dir" -type f | while read -r target_file; do
        filename=$(basename "$target_file")
        user_data_file="$user_data_dir/$filename"

        if [ -f "$user_data_file" ]; then
            echo "mounting $user_data_file -> $target_file"
            mkdir -p "$(dirname "$target_file")" || check_command "mkdir -p $(dirname $target_file)"
            mount --bind "$user_data_file" "$target_file" || check_command "mount --bind $user_data_file $target_file"
        fi
    done
done

user_ko_file="$user_data_dir/rk96x.ko"
if [ -f "$user_ko_file" ]; then
    echo "found $user_ko_file, starting special mount process for ko file"

    found_ko_files=$(find "$search_base" -type f -name "rk96x.ko" ! -path "$user_data_dir/*")
    ko_count=$(echo "$found_ko_files" | wc -l | tr -d ' ')

    if [ "$ko_count" -ne 1 ]; then
        echo "error: expected 1 rk96x.ko file outside $user_data_dir, but found $ko_count" >&2
        exit 1
    fi

    target_ko_file=$(echo "$found_ko_files" | head -n 1)
    echo "mounting $user_ko_file -> $target_ko_file"

    mkdir -p "$(dirname "$target_ko_file")" || check_command "mkdir -p $(dirname $target_ko_file)"
    mount --bind "$user_ko_file" "$target_ko_file" || check_command "mount --bind $user_ko_file $target_ko_file"
fi
