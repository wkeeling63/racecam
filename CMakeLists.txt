# racecam

cmake_minimum_required(VERSION 3.13)

project(racecam)

# SET(COMPILE_DEFINITIONS -Werror -pthread)
# set(CMAKE_C_FLAGS "-pthread")

# Set --no-as-needed to stop the linker discarding mmal_vc_client
# as it can't see that the constructor registers a load of functionality
# with the MMAL core.

SET(CMAKE_EXE_LINKER_FLAGS "-Wl,-L/opt/vc/lib,--no-as-needed")

include_directories(/opt/vc/include)
# include_directories(/usr/include/alsa)
# include_directories(/alsa-utils/include)
include_directories(/usr/include/gtk-2.0)
include_directories(/usr/lib/arm-linux-gnueabihf/gtk-2.0/include)
include_directories(/usr/lib/arm-linux-gnueabihf/glib-2.0/include)
include_directories(/usr/include/cairo)
include_directories(/usr/include/pango-1.0)
include_directories(/usr/include/atk-1.0) 
include_directories(/usr/include/gdk-pixbuf-2.0)
include_directories(/usr/include/glib-2.0)
 
set (RACECAM_SOURCES
   raspiCamUtilities.c
   racecamCommon.c
   GPSUtil.c
   racecamQueue.c
   racecamLogger.c)
   
set (DUMPUSB_SOURCES testhttps.c)
   
add_executable(racecam ${RACECAM_SOURCES} racecam.c)

add_executable(racecamcli ${RACECAM_SOURCES} racecamcli.c)

add_executable(testhttps ${DUMPUSB_SOURCES} testhttps.c)

set (MMAL_LIBS mmal_core mmal_util mmal_vc_client)
set (GTK_LIBS gtk-x11-2.0 gdk-x11-2.0 pangocairo-1.0 atk-1.0 cairo gdk_pixbuf-2.0 gio-2.0 pangoft2-1.0 pango-1.0 gobject-2.0 glib-2.0 fontconfig freetype)

# target_link_libraries(PiPIPflv pthread asound avformat avcodec avutil swresample ${MMAL_LIBS} vcos bcm_host bcm2835 cairo)
# target_link_libraries(racecam ${GTK_LIBS} ${MMAL_LIBS} pthread asound avformat avcodec avutil swresample vcos bcm_host bcm2835 cairo)
target_link_libraries(racecam ${GTK_LIBS} ${MMAL_LIBS} pthread asound avformat avcodec avutil swresample vcos bcm_host cairo)

target_link_libraries(racecamcli ${MMAL_LIBS} pthread asound avformat avcodec avutil swresample vcos bcm_host bcm2835 cairo)

target_link_libraries(testhttps ssl crypto)

install(TARGETS racecam RUNTIME DESTINATION bin)

install(TARGETS racecamcli RUNTIME DESTINATION bin)

install(TARGETS testhttps RUNTIME DESTINATION bin)
