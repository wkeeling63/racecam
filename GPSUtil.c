#include "GPSUtil.h" 
#include "racecamLogger.h"
#include "racecamCommon.h"
 
/* int get_gps()
{
   log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
   int index[20], i, c=0, speed=-2;
   char buf[256];
   FILE *gps_file;
   
   gps_file=fopen("/home/pi/gps.data", "r");
   if (gps_file)
      {
      size_t size=0;
      size=fread(&buf, 1, sizeof(buf)-1, gps_file);
      buf[size]='\0';
     
      index[0] = c;
      for (i=0;i<size;i++)
         {
          if (buf[i]==',')
            {
            buf[i]='\0';
            c++;
            index[c]=i+1;
            }
         }
      if (buf[index[2]]=='A')
         {
         float fspd=0;
         sscanf(buf+index[7], "%f", &fspd);
         speed = fspd*1.15078;   
         }
      else
         {
         speed = -1;
         }
      fclose(gps_file);
      }
    return speed;
} */
 int parse_gps(char *msg)
{
   log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
   
   log_status("full message %d %s", strlen(msg), msg);
   
   int index[20], i, c=0, size=strlen(msg);
   if (strncmp(msg,"$GPRMC",6))
      {
      return -2;
      }
   else
      {
      index[0]='\0';
      for (i=0;i<size;i++)
         {
          if (msg[i]==',')
            {
            msg[i]='\0';
            c++;
            index[c]=i+1;
            }
         }
      if (msg[index[2]]=='A')
         {
         float fspd=0;
         sscanf(msg+index[7], "%f", &fspd);
         return fspd*1.15078;   
         }
      else
         {
         return -1;
         }
      }
} 

int read_gps(int fd)
{
   log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);

   static int o=0, speed=-2;
   static char msg[256];
   
   int cnt=0, i;
   char buf[256];
   
   cnt = read(fd,buf,255);

   for (i=0; i<cnt; i++)
      {
      if ((buf[i] == '\r') || (buf[i] == '\n'))
         {
 /*        if (buf[i] == '\r')
            log_status("CR");
         else
            log_status("LF"); */
         if (o>6)
            {
            msg[o] = '\0';
            speed=parse_gps(msg);
            }
         o=0;
         }
      else
         {
         msg[o]=buf[i];
         o++;
         }
      }
//   log_status("after read %d %d %d", cnt, i, o);
   return speed;
} 

void send_text(int speed, int max_width, GPS_T *gps)
{
   log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
   log_status("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
   
   char buffer[8];
   
   if (speed == -2)
      sprintf(buffer, "No Data");
   else
      if (speed == -1)
         sprintf(buffer, "No GPS");
      else
         sprintf(buffer, "%3d MPH", speed); 
   
   log_status("speed in send_text %d %s", speed, buffer);
         
   MMAL_BUFFER_HEADER_T *buffer_header=NULL;

   if ((buffer_header = mmal_queue_get(gps->t_queue)) != NULL)
      {
/*      if (speed < 0)
         {
         buffer_header->length=buffer_header->alloc_size=0;
         buffer_header->user_data=NULL;
         } 
      else */
//         {
//         char buffer[8];
//         sprintf(buffer, "%3d MPH", speed); 
         cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, VCOS_ALIGN_UP(gps->text.width,32), VCOS_ALIGN_UP(gps->text.height,16));
         cairo_status_t cairo_status = cairo_surface_status (surface);
         if (cairo_status) 
            {
            log_error("surface status %s %d", cairo_status_to_string (cairo_status), cairo_status);
            }
         cairo_t *cr =  cairo_create(surface);
         cairo_rectangle(cr, 0, 0, gps->text.width, gps->text.height);
         cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
         cairo_fill(cr);
         cairo_set_source_rgb(cr, 0.75, 0.75, 0.75);
         cairo_select_font_face(cr, "cairo:serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
         cairo_set_font_size(cr, gps->text_size);
         cairo_text_extents_t extents;
         cairo_text_extents(cr, buffer, &extents);
         cairo_move_to(cr, gps->text.x+(max_width-extents.x_advance), gps->text.y);
         cairo_show_text(cr, buffer);
         cairo_destroy(cr);
         buffer_header->data=cairo_image_surface_get_data(surface);
         buffer_header->user_data = surface;
         buffer_header->length=buffer_header->alloc_size=
         cairo_image_surface_get_height(surface)*cairo_image_surface_get_stride(surface);
//         } 
      buffer_header->cmd=buffer_header->offset=0;

      int status=mmal_port_send_buffer(gps->t_port, buffer_header);
      if (status) log_error("buffer send of text overlay failed %s", mmal_status_to_string(status));
      }
   else
      {
      log_error("no buffer header returned for text overlay");
      }

}

/* void *port_messages(void *argp)
{
   log_status("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
   int *cntl_fd = (int *)argp;
   char rbuf[256], msg[256];
   int cnt, i=0, o=0;
   while (*cntl_fd)
      {
      cnt = read(*cntl_fd,rbuf,255);
      for (i=0; i<cnt; i++)
         {
         if ((rbuf[i] == '\r') || (rbuf[i] == '\n'))
            {
            msg[o] = '\0';
            if (o) log_status("GPS control port message %s", msg);
            o=0;
            }
         else
            {
            msg[o]=rbuf[i];
            o++;
            }
         }
      }
   log_status("done message thread...");
 } */

void *gps_thread(void *argp)
{
   log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__); 
   
   log_status("Starting GPS thread...");
//   vcos_sleep(60000);
//   log_status("Starting GPS thread.wait done");

   GPS_T *gps = (GPS_T *)argp;
   int speed = -2, last_speed = -3; 
   
   int fd;
   fd = open(GPSCNTL, O_RDWR | O_NOCTTY ); 
   if (fd <0) 
      {
      log_error("Open of GPS data failed! RC=%d\r\n", fd);
      gps->active = ERROR;
      goto error;
      }
      
   struct termios options;
   
   tcgetattr(fd,&options);
   
   options.c_iflag &= ~(IXON | IXOFF | IXANY | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
   options.c_iflag |= IGNBRK;
   options.c_oflag = 0; 
   options.c_lflag = 0;
   options.c_cflag &= ~(PARENB | CSTOPB | CSIZE);
   options.c_cflag |= CLOCAL | HUPCL | CREAD | CS8 | B115200;
   options.c_cc[VMIN]     = 0; 
   options.c_cc[VTIME]    = 10;
   
   cfsetspeed(&options, B115200);

   tcflush(fd, TCIOFLUSH);
   tcsetattr(fd,TCSANOW,&options); 
   
   char cmd[] = "AT+QGPS=1\r";
   size_t wstat = write(fd, cmd, sizeof(cmd));
   if (wstat < 0) log_error("Write GPS init commands error:%s\r\n", strerror(wstat));
   tcdrain(fd);
   
   int status =  close(fd);
   if (status) log_error("Close of GPS control failed! RC=%d\r\n", status);
   
   fd = open(GPSDATA, O_RDONLY | O_NOCTTY );
   if (fd <0) 
      {
      log_error("Open of GPS control failed! RC=%d\r\n", fd);
      gps->active = ERROR;
      goto error;
      }
      
   tcgetattr(fd,&options); 

   options.c_iflag &= ~(IXON | IXOFF | IXANY | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
   options.c_iflag |= IGNBRK;
   options.c_oflag = 0; 
   options.c_lflag = 0;
   options.c_cflag &= ~(PARENB | CSTOPB | CSIZE);
   options.c_cflag |= CLOCAL | HUPCL | CREAD | CS8 | B115200;
   options.c_cc[VMIN]     = 0; 
   options.c_cc[VTIME]    = 10;
   
   cfsetspeed(&options, B115200);      

   tcflush(fd, TCIOFLUSH);
   tcsetattr(fd,TCSANOW,&options);
   
   
   cairo_surface_t *temp_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, VCOS_ALIGN_UP(gps->text.width,32), VCOS_ALIGN_UP(gps->text.height,16));
   cairo_t *temp_context =  cairo_create(temp_surface);
   cairo_rectangle(temp_context, 0, 0, gps->text.width, gps->text.height);
   cairo_select_font_face(temp_context, "cairo:serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
   cairo_set_font_size(temp_context, gps->text_size);
   cairo_text_extents_t extents;
   cairo_text_extents(temp_context, "888 MPH", &extents);

   int max_width=extents.x_advance;
   int max_above_o=(double)extents.y_bearing*-1;
   int max_below_o=(double)extents.height-((double)extents.y_bearing*-1);  // why 51-40=10???
   cairo_destroy(temp_context);
   cairo_surface_destroy(temp_surface);

   if (gps->text.x > (gps->text.width-max_width)) gps->text.x = gps->text.width-max_width; 
   if (gps->text.x < 0) gps->text.x = 0; 
   if (gps->text.y > (gps->text.height-max_below_o)) gps->text.y = gps->text.height-max_below_o; 
   if (gps->text.y < max_above_o) gps->text.y = max_above_o; 
   
//   send_text(speed, max_width, gps);

   while (gps->active > 0) 
      { 
//      log_status("gps active %d", gps->active);
//      vcos_sleep(100);
//      if (gps->active == WAITING) last_speed = -3;
      speed = read_gps(fd);
//      speed = get_gps();
      if (gps->active == SENDING) 
         {
         if (speed != last_speed)
            {
            send_text(speed, max_width, gps);
            vcos_sleep(50);  //wait needed due to 2 threads MMAL release of buffer and create in this thread
            last_speed = speed;
            }
         }
      else
         {
         last_speed = -1;
         }
//      vcos_sleep(1000);
      }
   
/*   msg_fd = 0;
   pthread_join(msg_tid, NULL);  */

error:   
   status =  close(fd);
   if (status) log_error("Close of GPS data failed! RC=%d\r\n", status);

   log_status("Ending GPS thread");
}
