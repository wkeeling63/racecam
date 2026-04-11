# dts/meson.build
'''
dtc_prog = find_program('kdtc', required: true)
dts_source = files('wpc-racecam.dts')
dtbo_output = 'wpc-racecam.dtbo'

custom_target(
    'compile-dtbo',
    output: dtbo_output,
    input: dts_source,
    command: [dtc_prog, '@INPUT@', '@OUTPUT@' ],
    install: true,
    install_dir: '/boot/firmware/overlays/'
)
'''
dtc_prog = find_program('kdtc', required: true)

# 1. Define the list of DTS files
dts_files = [
  'wpc-racecam.dts',
  'wpc-racecam-v1.dts',
  'wpc-racecam-v2.dts'
]

# 2. Loop through files and create a target for each
foreach dts : dts_files
  # Generate output name: replace .dts/.dtsi with .dtbo
#  dtbo_name = dts.underscorify().split('.')[0] + '.dtbo'
  dtbo_name = dts.split('.')[0] + '.dtbo'
  
  custom_target(
    'compile-' + dtbo_name,
    output: dtbo_name,
    input: dts,
    command: [dtc_prog, '-@', '-I', 'dts', '-O', 'dtb', '-o', '@OUTPUT@', '@INPUT@'],
    install: true,
    install_dir: '/boot/firmware/overlays/'
  )
endforeach

