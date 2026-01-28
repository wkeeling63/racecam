dtc_prog = find_program('dtc', required: true)
dts_source = files('wpc-racecam.dts')
dtbo_output = 'wpc-racecam.dtbo'
# Create a custom target to build the DTBO from the DTS
# This command compiles the .dts file to a .dtbo binary blob
# -@ is used for overlays, -O dtb specifies the output format
custom_target(
    'compile-dtbo',
    output: dtbo_output,
    input: dts_source,
    command: [dtc_prog, '-@', '-O', 'dtb', '-o', '@OUTPUT@', '@INPUT@'],
    install: true,
    # Specify the installation directory on the target Raspberry Pi
    # This path assumes you are running 'meson install' with the RPi's /boot mounted or as root
    install_dir: '/boot/overlays/'
)
