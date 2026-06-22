# core/meson.build
thread_dep = dependency('threads', required : true)
# yaml_dep = dependency('yaml-cpp', required : true)
# boost_dep = dependency('boost', modules: ['json'], required : true)

# racecam_app_dep += [boost_dep, thread_dep, yaml_dep]
# racecam_app_dep += [boost_dep, thread_dep]

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

# enable_libav = get_option('enable_libav')
libav_dep_names = ['libavcodec', 'libavdevice', 'libavformat', 'libavutil', 'libswresample', 'libavfilter']
# libav_dep_names = ['libavdevice', 'libavformat', 'libavutil', 'libswresample', 'libavfilter']
libav_deps = []

# if enable_libav
    foreach name : libav_dep_names
 #       dep = dependency(name, required : false)
        dep = dependency(name, required : true)
 #       if not dep.found()
 #           enable_libav = false
 #           break
 #       endif
        libav_deps += dep
    endforeach
# endif

# if enable_libav
        racecam_app_dep += libav_deps
        cpp_arguments += '-DLIBAV_PRESENT=1'
# endif

inc_install_dir = get_option('includedir') / meson.project_name() / 'core'
