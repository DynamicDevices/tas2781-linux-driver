clear
OUTPUT_DIR1=build_dir
OUTPUT_DIR2=hybrid-ko
function clear_stdin()
(
    old_tty_settings=`stty -g`
    stty -icanon min 0 time 0

    while read none; do :; done

    stty "$old_tty_settings"
)
make W=1 ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- KCONFIG_CONFIG=config_dir/.hybrid-config O=$OUTPUT_DIR1/$OUTPUT_DIR2 CONFIG_SND_SOC_INTEGRATED_TASDEVICE=m modules -j $(expr $(nproc) - 1) 2>&1 | tee $OUTPUT_DIR1/$OUTPUT_DIR2/compile-tasdevice.log
clear_stdin