# dts/meson.build
dtc_prog = find_program('kdtc', required: true)
dts_source = files('wpc-racecam-overlay.dts')
dtbo_output = 'wpc-racecam.dtbo'

custom_target(
    'compile-dtbo',
    output: dtbo_output,
    input: dts_source,
    command: [dtc_prog, '@INPUT@', '@OUTPUT@' ],
    install: true,
    install_dir: '/boot/overlays/'
)
