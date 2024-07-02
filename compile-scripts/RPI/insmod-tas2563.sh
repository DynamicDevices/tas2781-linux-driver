#!/bin/bash
TMP_FOLDER=$(pwd)
modprobe regmap-i2c
modprobe crc8
pushd /sys/kernel/config/device-tree/overlays/
mkdir -p integrated-tasdevice
cp $TMP_FOLDER/integrated-tasdevice-soundcard.dtbo ./integrated-tasdevice/dtbo
popd
insmod snd-soc-integrated-tasdevice.ko
