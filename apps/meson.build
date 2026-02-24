# apps/meson.build
racecam_config = executable('racecam-config', files('racecam_config.cpp'),
                        include_directories : include_directories('..'),
#                        dependencies: [libcamera_dep, src_dep, libav_deps, boost_dep],
                        dependencies: [libcamera_dep, libav_deps, boost_dep],
                        link_with : rcamcfg_lib, 
                        install : true)

racecam = executable('racecam', files('racecam.cpp'),
                        include_directories : include_directories('..'),
#                        dependencies: [libcamera_dep, src_dep, libav_deps, boost_dep],
                        dependencies: [libcamera_dep, libav_deps, boost_dep],
                        link_with : rcam_lib,
                        install : true)
                        
racecamd = executable('racecamd', files('racecamd.cpp'),
                        include_directories : include_directories('..'),
#                        dependencies: [libcamera_dep, src_dep, libav_deps, boost_dep],
                        dependencies: [libcamera_dep, libav_deps, boost_dep],
                        link_with : rcam_lib,
                        install : true)
