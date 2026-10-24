#!/bin/sh
PROGRAM="projector_demo"
check_process() {
    ps_output="$(ps | grep $PROGRAM | grep -v "grep")"

        if [ -z "$ps_output" ]; then
				echo "projector_demo is not running."
                return 1
        else
                return 0
        fi
}

while :;
do
        if check_process; then
                sleep 2
        else
                projector_demo  -d /dev/video1 -w 3840 -h 2160 -p NV12  --stream-mmap=3 --enable-encode=1 --display-type=0 --display-w=3840 --display-h=2160  --dst-ip=192.168.1.112  --enSlice=0 --dump-stream=0 --bitrate=10 &
                sleep 0.5
        fi
done
