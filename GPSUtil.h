
#ifndef GPSUTIL_H_
#define GPSUTIL_H_

#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <cairo/cairo.h>
#include "interface/mmal/mmal.h"

#define BAUDRATE B115200            
#define GPSDATA "/dev/ttyUSB1"
#define GPSCNTL "/dev/ttyUSB2"
// TEXTW must be 32 aligned
// #define TEXTW 256
// TEXTH must be 16 aligned
// #define TEXTH 64

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

// add surface width and hieght, text width and hieght, text port and pool_queue 
// remove speed, fd_data, fd_cntl

/* typedef struct GPS_S 
   {
   int fd_data;
   int fd_cntl;
   int speed;
   int active;
   } GPS_T;  */
   

// int open_gps(GPS_T *);
// int close_gps(GPS_T *);
// void read_gps(GPS_T *);
cairo_surface_t* cairo_text(int, int, int);
void *gps_thread(void *);

#endif /* GPSUTIL_H_ */
