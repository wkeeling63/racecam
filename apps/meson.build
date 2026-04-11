# apps/meson.build
racecam_config = executable('racecam-config', files('racecam_config.cpp') + rcamcfg_src,
                        include_directories : include_directories('..'),
#                        dependencies: [libcamera_dep, src_dep, libav_deps, boost_dep],
                        dependencies: [libcamera_dep, libav_deps, boost_dep, curlpp_dep],
#                        link_with : rcamcfg_lib, 
                        install : true)

racecam = executable('racecam', files('racecam.cpp') + rcam_src,
                        include_directories : include_directories('..'),
#                        dependencies: [libcamera_dep, src_dep, libav_deps, boost_dep],
                        dependencies: [libcamera_dep, libav_deps, boost_dep, curlpp_dep],  
#                        link_with : rcam_lib,
                        install : true)
                        
racecamd = executable('racecamd', files('racecamd.cpp')  + rcam_src,
                        include_directories : include_directories('..'),
#                        dependencies: [libcamera_dep, src_dep, libav_deps, boost_dep],
                        dependencies: [libcamera_dep, libav_deps, boost_dep, curlpp_dep],
#                        link_with : rcam_lib,
                        install : true)
                        
#rcam_control_map = executable('rcammakemap', files('racecam_control_map.cpp'),
#                        include_directories : include_directories('..'),
#                        dependencies: [yaml_dep],
#                        install : true)
