#!/bin/sh
NEWDIR=tas2563-miele
KERNEL_COMPILE_CONFIG_FILE=$NEWDIR.config
export KERNEL=kernel8

mkdir -p mnt
mkdir -p mnt/fat32
mkdir -p mnt/ext4
mount /dev/sdb1 mnt/fat32
mount /dev/sdb2 mnt/ext4

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
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- KCONFIG_CONFIG=config_dir/$KERNEL_COMPILE_CONFIG_FILE O=$OUTPUT_DIR1/$NEWDIR INSTALL_MOD_PATH=mnt/ext4 modules_install -j $(expr $(nproc) - 1) 2>&1 | tee $OUTPUT_DIR1/$NEWDIR/compile.log
cp mnt/fat32/$KERNEL.img mnt/fat32/$KERNEL-backup.img
cp $OUTPUT_DIR1/$NEWDIR/arch/arm64/boot/Image mnt/fat32/$KERNEL.img
cp $OUTPUT_DIR1/$NEWDIR/arch/arm64/boot/dts/broadcom/*.dtb mnt/fat32/
cp $OUTPUT_DIR1/$NEWDIR/arch/arm64/boot/dts/overlays/*.dtb* mnt/fat32/overlays/
cp $OUTPUT_DIR1/$NEWDIR/arch/arm64/boot/dts/overlays/README mnt/fat32/overlays/
cp firmware/*.bin mnt/ext4/lib/firmware
echo Please check the overlays, if error, then press CTRL+C
clear_stdin
read -n1 -p "else press any key to continue..."
umount mnt/fat32
umount mnt/ext4
echo Please check the log, compiling is completed!
clear_stdin
