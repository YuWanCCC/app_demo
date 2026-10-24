#!/bin/bash
if [ ! -f "patched" ]; then
patch -p1 < 0001-nginx-http-flv-live-remove-openssl.patch
fi
if [ "$?" = "0" ]; then
	touch "patched"
fi
