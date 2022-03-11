#include "GPSUtil.h" 
#include "racecamLogger.h"
#include "racecamCommon.h"
 
int open_gps(int *fd_data, int *fd_cntl)
{
   log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
   log_status("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
// open readonly GPS data return port   
   *fd_data = open(GPSDATA, O_RDONLY | O_NOCTTY );
   if (*fd_data <0) 
      {
      log_error("Open of GPS data failed! RC=%d", *fd_data);
      return -1;
      }
      
   struct termios options_data, options_cntl;
   
   tcgetattr(*fd_data,&options_data);
   
/*   options_data.c_iflag &= ~(IXON | IXOFF | IXANY | IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
   options_data.c_iflag |= IGNPAR | ICRNL;
   
   options_data.c_oflag &= (OPOST | ONLCR); 
   
   options_data.c_lflag &= ~(ECHO | ECHOE | ECHONL | ISIG | ICANON);
   
   options_data.c_cflag &= ~(PARENB | CSTOPB | CSIZE);
   options_data.c_cflag |= CS8 | CLOCAL | CREAD | CRTSCTS; */
   
   options_data.c_iflag &= ~(IXON | IXOFF | IXANY | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
   options_data.c_iflag |= IGNBRK;
   
   options_data.c_oflag = 0; 
   
   options_data.c_lflag = 0;
   
   options_data.c_cflag &= ~(PARENB | CSTOPB | CSIZE);
   options_data.c_cflag |= CLOCAL | HUPCL | CREAD | CS8 | B115200;
//   options_data.c_cc[VEOF]     = 4;     // Ctrl-d  
   options_data.c_cc[VMIN]     = 0; 
   options_data.c_cc[VTIME]    = 5;
   
   cfsetspeed(&options_data, B115200);
   log_status("iflag %u oflag %u cflag %u lflag %u cline %d\r\n", options_data.c_iflag, options_data.c_oflag, options_data.c_cflag, options_data.c_lflag, options_data.c_line);
   log_status("ispeed %u ospeed %u\r\n", options_data.c_ispeed, options_data.c_ospeed);
    
   tcflush(*fd_data, TCIFLUSH);
   tcsetattr(*fd_data,TCSANOW,&options_data); 
 
// open read/write GPS control port      
   *fd_cntl = open(GPSCNTL, O_RDWR | O_NOCTTY ); 
   if (*fd_cntl <0) 
      {
      log_error("Open of GPS control failed! RC=%d", *fd_cntl);
      return -1;
      }
      
   tcgetattr(*fd_data,&options_cntl); 
//   memset(&options, 0, sizeof(options));
//   options_cntl.c_cflag = BAUDRATE | CRTSCTS | CS8 | CLOCAL | CREAD;
//   options_cntl.c_iflag = IGNPAR | ICRNL;
//   options_cntl.c_lflag = ICANON;
//   options_cntl.c_cc[VEOF]     = 4;     // Ctrl-d 
//   options_cntl.c_cc[VMIN]     = 1; 
   options_data.c_iflag &= ~(IXON | IXOFF | IXANY | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
   options_data.c_iflag |= IGNBRK;
   
   options_data.c_oflag = 0; 
   
   options_data.c_lflag = 0;
   
   options_data.c_cflag &= ~(PARENB | CSTOPB | CSIZE);
   options_data.c_cflag |= CLOCAL | HUPCL | CREAD | CS8 | B115200;
//   options_cntl.c_lflag |= ICANON;
//   options_cntl.c_cc[VEOF]     = 4;     // Ctrl-d  
   options_cntl.c_cc[VMIN]     = 0; 
   options_cntl.c_cc[VTIME]    = 10;
   
   cfsetspeed(&options_data, B115200);      

   tcflush(*fd_cntl, TCIFLUSH);
   tcsetattr(*fd_cntl,TCSANOW,&options_cntl); 
   
//   char cmd[] = "AT+QGPSCFG=\"gpsnmeatype\",2\rAT+QGPSCFG=\"outport\",\"usbnmea\"\rAT+QGPS=1\rAT+QGPS?\r";
   char cmd[] = "AT+QGPS=1\rAT+QGPS?\r";
   size_t status = write(*fd_cntl, cmd, sizeof(cmd));
   if (status < 0) log_error("Write GPS init commands error:%s", strerror(errno)); 
   
/*   status = write(*fd_cntl, "AT+QGPS=1\r\n", 11);
   if (status < 0) log_error("Write AT+QGPS=1 error:%s", strerror(errno));
   
   status = write(*fd_cntl, "AT+QGPS?\r\n", 10);
   if (status < 0) log_error("Write AT+QGPS? error:%s", strerror(errno)); */
   
   return 0;
}

void close_gps(int *fd_data, int *fd_cntl)
{
   log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
   log_status("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
   
   char cmd[] = "AT+QGPSEND\r";
   write(*fd_cntl, cmd, sizeof(cmd));
   
   int status =  close(*fd_cntl);
   if (status) log_error("Close of GPS control failed! RC=%d", status);

   status = close(*fd_data);
   if (status) log_error("Close of GPS data failed! RC=%d", status);
}

int parse_gps(char *msg)
{
   log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
   
   log_status("full message %s", msg);
   
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

int read_gps(int *fd_data)
{
   log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);

   static int o=0;
   static char msg[256];
   
   int cnt=0, i, speed=-3;
   char buf[256];
   
   cnt = read(*fd_data,buf,255);

   for (i=0; i<cnt; i++)
      {
      if ((buf[i] == '\r') || (buf[i] == '\n'))
         {
         msg[o] = '\0';
         speed=parse_gps(buf);
         o=0;
         }
      else
         {
         msg[o]=buf[i];
         o++;
         }
      }
   return speed;
}

void send_text(int speed, int max_width, GPS_T *gps)
{
   log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);

   MMAL_BUFFER_HEADER_T *buffer_header=NULL;

   if ((buffer_header = mmal_queue_get(gps->t_queue)) != NULL)
      {
      if (speed < 0)
         {
         buffer_header->length=buffer_header->alloc_size=0;
         buffer_header->user_data=NULL;
         } 
      else
         {
         char buffer[8];
         sprintf(buffer, "%3d MPH", speed); 
         cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, VCOS_ALIGN_UP(gps->text.width,32), VCOS_ALIGN_UP(gps->text.height,16));
         cairo_status_t status = cairo_surface_status (surface);
         if (status) 
            {
            log_error("surface status %s %d", cairo_status_to_string (status), status);
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
         } 
      buffer_header->cmd=buffer_header->offset=0;

      int status=mmal_port_send_buffer(gps->t_port, buffer_header);
      if (status) log_error("buffer send of text overlay failed %s", mmal_status_to_string(status));
      }
   else
      {
      log_error("no buffer header returned for text overlay");
      }

}

void *port_messages(void *argp)
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
 }

void *gps_thread(void *argp)
{
   log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__); 
   
   log_status("Starting GPS thread...");
   vcos_sleep(3000);

   GPS_T *gps = (GPS_T *)argp;
   int fd_data, fd_cntl;
   int speed = -1, last_speed = -1; 
   
   if (open_gps(&fd_data, &fd_cntl)) return NULL;
   
   pthread_t msg_tid;
   int msg_fd = fd_cntl;
   pthread_create(&msg_tid, NULL, port_messages, (void *)&msg_fd); 
   
   char cmd[] = "AT+QGPSCFG=\"gpsnmeatype\",2\rAT+QGPSCFG=\"outport\",\"usbnmea\"\rAT+QGPS=1\rAT+QGPS?\r";
   size_t status = write(fd_cntl, cmd, sizeof(cmd));
   if (status < 0) log_error("Write GPS init commands error:%s", strerror(errno)); 
  
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

   while (gps->active > 0) 
      { 
      speed = read_gps(&fd_data);
      if (gps->active == SENDING) 
         {
         if ((speed > -2) && (speed != last_speed))
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
 //     vcos_sleep(100);
      }
   
   msg_fd = 0;
   pthread_join(msg_tid, NULL); 
   
   close_gps(&fd_data, &fd_cntl);
   log_status("Ending GPS thread");
}
