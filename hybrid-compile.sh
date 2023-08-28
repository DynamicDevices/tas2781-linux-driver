clear
OUTPUT_DIR1=build_dir
OUTPUT_DIR2=hybrid
function clear_stdin()
(
    old_tty_settings=`stty -g`
    stty -icanon min 0 time 0

    while read none; do :; done

    stty "$old_tty_settings"
)
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- KCONFIG_CONFIG=config_dir/.$OUTPUT_DIR2-config O=$OUTPUT_DIR1/$OUTPUT_DIR2 -j $(expr $(nproc) - 1) 2>&1 | tee $OUTPUT_DIR1/$OUTPUT_DIR2/compile.log
clear_stdin