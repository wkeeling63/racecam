
#ifndef GPSUTIL_H_
#define GPSUTIL_H_

#include <fcntl.h>
// #include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <cairo/cairo.h>
#include "interface/mmal/mmal.h"
          
#define GPSDATA "/dev/ttyUSB1"
#define GPSCNTL "/dev/ttyUSB2"

enum gps_enum {
   DONE,
   WAITING,
   SENDING};
   
typedef struct GPS_S 
   {
   int active;
   MMAL_RECT_T text;
   int text_size;
   MMAL_PORT_T *t_port;
   MMAL_QUEUE_T *t_queue;
   } GPS_T;

void *gps_thread(void *);

#endif /* GPSUTIL_H_ */
