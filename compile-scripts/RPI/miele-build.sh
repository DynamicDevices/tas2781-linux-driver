#!/bin/sh
NEWDIR=tas2563-miele
KERNEL_COMPILE_CONFIG_FILE=$NEWDIR.config
clear
function clear_stdin()
(
    old_tty_settings=`stty -g`
    stty -icanon min 0 time 0

    while read none; do :; done

    stty "$old_tty_settings"
)

OUTPUT_DIR1=build_dir

if [ ! -d $OUTPUT_DIR1 ];then
    mkdir $OUTPUT_DIR1
fi
if [ ! -d $OUTPUT_DIR1/$NEWDIR ];then
	pushd $OUTPUT_DIR1
    mkdir $NEWDIR
    popd
fi
echo Please check the log, if error, then press CTRL+C
clear_stdin
read -n1 -p "else press any key to continue..."
#make mrproper
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- KCONFIG_CONFIG=config_dir/$KERNEL_COMPILE_CONFIG_FILE Image dtbs modules O=$OUTPUT_DIR1/$NEWDIR -j $(expr $(nproc) - 1) 2>&1 | tee $OUTPUT_DIR1/$NEWDIR/compile.log
cp arch/arm64/boot/dts/overlays/README $OUTPUT_DIR1/$NEWDIR/arch/arm64/boot/dts/overlays
echo Please check the log, compiling is completed!
clear_stdin
