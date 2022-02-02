#include "GPSUtil.h" 
#include "racecamLogger.h"
#include "racecamCommon.h"
 
int open_gps(int *fd_data, int *fd_cntl)
{
   log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
   struct termios options, ops;
   memset(&options, 0, sizeof(options));
   options.c_cflag = BAUDRATE | CRTSCTS | CS8 | CLOCAL | CREAD;
   options.c_iflag = IGNPAR | ICRNL;
   options.c_lflag = ICANON;
   options.c_cc[VEOF]     = 4;     // Ctrl-d  
   options.c_cc[VMIN]     = 1; 
    
   *fd_data = open(GPSDATA, O_RDWR | O_NOCTTY ); 
   if (*fd_data <0) 
      {
      log_error("Open of GPS data failed! RC=%d", *fd_data);
      return -1;
      }
   tcgetattr(*fd_data,&ops);   //needed???
   tcflush(*fd_data, TCIFLUSH);
   tcsetattr(*fd_data,TCSANOW,&options); 
   
   memset(&options, 0, sizeof(options));
   options.c_cflag = BAUDRATE | CRTSCTS | CS8 | CLOCAL | CREAD;
   options.c_iflag = IGNPAR | ICRNL;
   options.c_lflag = ICANON;
   options.c_cc[VEOF]     = 4;     // Ctrl-d 
   options.c_cc[VMIN]     = 1; 
      
   *fd_cntl = open(GPSCNTL, O_RDWR | O_NOCTTY ); 
   if (*fd_cntl <0) 
      {
      log_error("Open of GPS control failed! RC=%d", *fd_cntl);
      return -1;
      }
   tcflush(*fd_cntl, TCIFLUSH);
   tcsetattr(*fd_cntl,TCSANOW,&options); 
   
   write(*fd_cntl, "AT+QGPS=1\r", 10);
   
   return 0;
}

int close_gps(int *fd_data, int *fd_cntl)
{
   log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
   int status=0;
   write(*fd_cntl, "AT+QGPSEND\r", 11);
     
   status=close(*fd_cntl);
   if (status)
      {
      log_error("Close of GPS control failed! RC=%d", status);
      } 
   status=close(*fd_data);
   if (status)
      {
      log_error("Close of GPS data failed! RC=%d", status);
      }
   
   return status;
}
int read_gps(int *fd_data)
{
   log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
//   log_status("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);

   int cnt=0, i, c=0;
   int index[20];
   char buf[255];
   cnt = read(*fd_data,buf,255);
   cnt--;
   buf[cnt]='\0';
   log_status("all GPS messages size %d data %s", cnt, buf);
   if (!(cnt))
      {
      log_status("no GPS message waiting! 100");
      vcos_sleep(100);
      return -2;
      }
//   buf[cnt-2]='\0';
//   log_status("GPS read cnt %d data %s", cnt, buf);
//   cnt--;
   if ((cnt) && (!(strncmp(buf,"$GPRMC",6))))
      {
      log_status("valid GPS message %s", buf);
      index[0]='\0';
      for (i=0;i<cnt;i++)
         {
          if (buf[i]==',')
            {
            buf[i]='\0';
            c++;
            index[c]=i+1;
            }
         }
//      int statusi=index[2], speedi=index[7]; 
//      log_status("%s %s", buf+statusi, buf+speedi);
      if (buf[index[2]]=='A')
         {
//         log_status("valid GPS %s", buf+index[7]); 
         float fspd=0;
         sscanf(buf+index[7], "%f", &fspd);
//         log_status("speed after sscanf %f", fspd);
         return fspd*1.15078;   
         }
      else
         {
         log_status("not A -->%s", buf+index[2]);
         return -1;
         }
      } 
      
   return -2;   //loop until good meassge?
}

void send_text(int speed, int max_width, GPS_T *gps)
{
   log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
//   log_status("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
//   log_status("%d %d %d %d %d %d", speed, max_width, gps->text.width, gps->text.height, gps->text.x, gps->text.y);
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
//      log_status("text send status %s" , mmal_status_to_string(status));
      if (status) log_error("buffer send of text overlay failed %s", mmal_status_to_string(status));
      }
   else
      {
      log_error("no buffer header returned for text overlay");
      }

}

void *gps_thread(void *argp)
{
   log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__); 
//   log_status("%s in file: %s(%d)", __func__,  __FILE__, __LINE__); 
   
   log_status("Starting GPS thread...");
   vcos_sleep(3000);

   GPS_T *gps = (GPS_T *)argp;
   int fd_data, fd_cntl;
   int speed = -1, last_speed = -1;
//   int *ptr_state=gps->active;
   
   if (open_gps(&fd_data, &fd_cntl)) return NULL;
  
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
   
//   log_status("%d %d %d %d %d", max_width, gps->text.width, gps->text.height, gps->text.x, gps->text.y);

   if (gps->text.x > (gps->text.width-max_width)) gps->text.x = gps->text.width-max_width; 
   if (gps->text.x < 0) gps->text.x = 0; 
   if (gps->text.y > (gps->text.height-max_below_o)) gps->text.y = gps->text.height-max_below_o; 
   if (gps->text.y < max_above_o) gps->text.y = max_above_o; 

//   int64_t start = get_microseconds64()/100000;
 //  while (gps->active) 
//   log_status("GPS flag %d", gps->active);
   while (gps->active > 0) 
      { 
//      speed = get_microseconds64()/100000 - start;
      speed = read_gps(&fd_data);
//      log_status("post read speed %d last %d", speed, last_speed);
      if (speed != -2)  
         {
//         log_status("valid GPS message speed %d last %d", speed, last_speed);
         if (gps->active == SENDING) 
            {
            if (speed != last_speed)
               {
               send_text(speed, max_width, gps);
               vcos_sleep(50);  //wait needed due to 2 threads MMAL release of buffer and create in this thread
               last_speed = speed;
               }
            }
         }
//     vcos_sleep(1000);
      }
 
   close_gps(&fd_data, &fd_cntl);
   log_status("Ending GPS thread");
}
