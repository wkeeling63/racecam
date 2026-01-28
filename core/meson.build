# boost_dep = dependency('boost', modules : ['program_options'], required : true)
thread_dep = dependency('threads', required : true)
yaml_dep = dependency('yaml-cpp', required : true)
#json_dep = dependency('boost_json', required : true, version: '>=1.87')
# json1_87_dep = declare_dependency(include_directories : /usr/local/include/json, link_with : libboost_json)
boost_dep = dependency('boost', modules: ['json'], required : true)

#rpi2cam_app_dep += [boost_dep, thread_dep]

#link_flags = boost_dep.get_link_args()
# message('boost dep var in core: ', boost_dep.get_variable('libdir'))
racecam_app_dep += [boost_dep, thread_dep, yaml_dep]
# racecam_app_dep += [thread_dep, yaml_dep]

#rcamshared_src += files([
#    'rcamshared.cpp'
#])

rcamcfg_src += files([
    'rcamshared.cpp',
    'rcamcfg.cpp',
    'jsonio.cpp',
    'youtube.cpp',
])

rcam_src += files([
    'rcamshared.cpp',
    'rcam.cpp',
    'jsonio.cpp',
    'youtube.cpp',
    'dma_heaps.cpp',
])

#racecam_app_src += files([
#    'rcam_app.cpp',
#    'youtube.cpp',
##    'buffer_sync.cpp',
#    'dma_heaps.cpp',
#    'jsonio.cpp',
##    'rcamshared.cpp',
##    'rpi2cam_app.cpp',
##    'rpi2cam_encoder.cpp',
##    'options.cpp',
#])

#core_headers = files([
#    'rcam_app.hpp',
#    'youtube.hpp',
#    'buffer_sync.hpp',
#    'completed_request.hpp',
#    'dma_heaps.hpp',
#    'jsonio.hpp',
#    'rcamshared.hpp',
#    'rpi2cam_app.hpp',
#    'rpi2cam_encoder.hpp',
#    'logging.hpp',
#    'options.hpp',
#    'stream_info.hpp',
#    'version.hpp',
#    'stream_enum.hpp',
#])

enable_libav = get_option('enable_libav')
libav_dep_names = ['libavcodec', 'libavdevice', 'libavformat', 'libavutil', 'libswresample', 'libavfilter']
libav_deps = []

if enable_libav
    foreach name : libav_dep_names
        dep = dependency(name, required : false)
        if not dep.found()
            enable_libav = false
            break
        endif
        libav_deps += dep
    endforeach
endif

if enable_libav
        racecam_app_dep += libav_deps
#        message('libav is enabled')
        cpp_arguments += '-DLIBAV_PRESENT=1'
endif

# Generate a version string.
#version_cmd = [meson.project_source_root() / 'utils' / 'version.py']

# Check if a version.gen file is present.
# This would have been generated from the meson dist command.
#fs = import('fs')
#dist_version_file = meson.project_source_root() / 'version.gen'
#if fs.is_file(dist_version_file)
#    version_cmd += fs.read(dist_version_file)
#endif

#version_cpp = vcs_tag(command : version_cmd,
#                      replace_string: '@VER@',
#                      input : 'version.cpp.in',
#                      output : 'version.cpp',
#                      fallback : meson.project_version())

#rpi2cam_app_src += version_cpp

#install_headers(core_headers, subdir: meson.project_name() / 'core')

# Install symlinks to the old header directories and names for legacy purposes.
#install_symlink('libcamera-apps',
#                install_dir: get_option('includedir'),
#                pointing_to: 'rpicam-apps')

inc_install_dir = get_option('includedir') / meson.project_name() / 'core'
# install_symlink('libcamera_app.h',
#                install_dir: inc_install_dir,
#                pointing_to: 'rpicam_app.hpp')

# install_symlink('libcamera_encoder.h',
#                install_dir: inc_install_dir,
#                pointing_to: 'rpicam_encoder.hpp')
