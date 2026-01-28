project(
  'racecam',
  'c',
  'cpp',
meson_version : '>= 0.64.0',
version : '1.0.0',
default_options : ['werror=true', 'warning_level=3', 'cpp_std=c++17', 'c_std=c11', 'buildtype=release'],
license : 'BSD-2-Clause'
)

cpp_arguments = ['-pedantic', '-Wno-unused-parameter', '-faligned-new']

# Needed for file sizes > 32-bits.
cpp_arguments += '-D_FILE_OFFSET_BITS=64' 

cxx = meson.get_compiler('cpp')
cpu = host_machine.cpu()
neon = get_option('neon_flags')

if cxx.get_id() == 'gcc'
    cpp_arguments += '-Wno-psabi'
endif

srcheader = custom_target('generate_src_header',
        output : 'racecamsrc.hpp',
        command : ['sh', meson.project_source_root() / 'scripts/sourcelocation.sh', meson.project_source_root(), '@OUTPUT@']
  #      install_dir: include_directories('.')
    )

if cpu == 'aarch64' or neon == 'arm64'
    cpp_arguments += '-ftree-vectorize'
elif neon == 'armv8-neon'
    cpp_arguments += ['-mfpu=neon-fp-armv8', '-ftree-vectorize']
endif

libcamera_dep = dependency('libcamera', required : true)
boost_dep = dependency('boost', modules: ['json'], required : true)
curlpp_dep = dependency(['curlpp', 'curl'], required : true)
src_dep = declare_dependency(sources : [srcheader])



summary({
            'location' : libcamera_dep.get_variable('libdir'),
            'version' : libcamera_dep.version()
        },
       section : 'libcamera')

# racecam_app_src = []
racecam_app_dep = [libcamera_dep, boost_dep, curlpp_dep]
rcamcfg_src = []
rcam_src = []

subdir('core')

add_project_arguments(cpp_arguments, language : 'cpp')

# racecam_app = library(
#    'racecam_app',
#    racecam_app_src,
#    soversion : meson.project_version(),
#    include_directories : include_directories('.'),
#    install : true,
#    name_prefix : '',
#    dependencies : racecam_app_dep,
# )

 rcamcfg_lib = library(
    'librcamcfg',
    rcamcfg_src,
    soversion : meson.project_version(),
    include_directories : include_directories('.'),
    install : true,
    name_prefix : '',
    dependencies : racecam_app_dep,
)

rcam_lib = library(
    'librcam',
    rcam_src,
    soversion : meson.project_version(),
    include_directories : include_directories('.'),
    install : true,
    name_prefix : '',
    dependencies : [racecam_app_dep, boost_dep],
)

# Install a symlink to the old library name for legacy purposes.
#install_symlink('lib_racecam.so',
#                install_dir: get_option('libdir'),
#                pointing_to: 'racecam_app.so')

subdir('apps')
subdir('dts')

summary({
#            'libav encoder' : enable_libav,
        },
        bool_yn : true, section : 'Build configuration')
