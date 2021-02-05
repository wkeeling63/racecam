# racecam

cmake_minimum_required(VERSION 3.13)

project(racecam)

# SET(COMPILE_DEFINITIONS -Werror -pthread)
# set(CMAKE_C_FLAGS "-pthread")

# Set --no-as-needed to stop the linker discarding mmal_vc_client
# as it can't see that the constructor registers a load of functionality
# with the MMAL core.
# SET( CMAKE_EXE_LINKER_FLAGS "-Wl,--no-as-needed" )
SET(CMAKE_EXE_LINKER_FLAGS "-Wl,-L/opt/vc/lib,--no-as-needed")

# include_directories(/opt/vc/include)
# include_directories(/usr/include/alsa)
# include_directories(/alsa-utils/include)
include_directories(/usr/include/gtk-2.0)
include_directories(/usr/lib/arm-linux-gnueabihf/gtk-2.0/include)
include_directories(/usr/lib/arm-linux-gnueabihf/glib-2.0/include)
# include_directories(/usr/include/gio-unix-2.0)
include_directories(/usr/include/cairo)
include_directories(/usr/include/pango-1.0)
include_directories(/usr/include/atk-1.0)
# include_directories(/usr/include/pixman-1) 
include_directories(/usr/include/gdk-pixbuf-2.0)
# include_directories(/usr/include/libmount)
# include_directories(/usr/include/blkid)
# include_directories(/usr/include/harfbuzz)
# include_directories(/usr/include/fribidi)
include_directories(/usr/include/glib-2.0)
# include_directories(/usr/include/uuid)
# include_directories(/usr/include/freetype2)
# include_directories(/usr/include/libpng16)

# Find the commit hash of the build and pass to the compiler
#execute_process(
#  COMMAND git log -1 --abbrev=12 --format=%h
#  OUTPUT_VARIABLE GIT_COMMIT_ID
#  OUTPUT_STRIP_TRAILING_WHITESPACE
#)
#add_definitions("-DGIT_COMMIT_ID=\"${GIT_COMMIT_ID}\"")

# Determine if we are tainted
#execute_process(
#  COMMAND bash "-c" "git ls-files -m | wc -l"
#  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
#  OUTPUT_VARIABLE GIT_TAINTED
#  OUTPUT_STRIP_TRAILING_WHITESPACE
#)
#add_definitions("-DGIT_TAINTED=${GIT_TAINTED}")
 
# set (GUI_SOURCES
#   othermodules.c)
   
# add_executable(racecam ${PIPCAM_SOURCES} PiPIPflv.c)
add_executable(racecam racecam.c)

# set (MMAL_LIBS mmal_core mmal_util mmal_vc_client)
set (GTK_LIBS gtk-x11-2.0 gdk-x11-2.0 pangocairo-1.0 atk-1.0 cairo gdk_pixbuf-2.0 gio-2.0 pangoft2-1.0 pango-1.0 gobject-2.0 glib-2.0 fontconfig freetype)

#target_link_libraries(PiPIPflv pthread asound avformat avcodec avutil swresample ${MMAL_LIBS} vcos bcm_host bcm2835 cairo)
target_link_libraries(racecam ${GTK_LIBS})

install(TARGETS racecam RUNTIME DESTINATION bin)
