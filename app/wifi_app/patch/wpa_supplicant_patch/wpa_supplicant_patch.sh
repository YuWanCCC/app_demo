#!/bin/bash

if [ ! -f "wpa_supplicant_patched_done" ]; then
patch -p1 < 001-wpa_supplicant.patch
touch wpa_supplicant_patched_done
else
echo "wpa_supplicant: patched done. skip"
fi