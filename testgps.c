#include <fcntl.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define GPSDATA "/dev/ttyUSB1"
#define GPSCNTL "/dev/ttyUSB2"
 
int main(int argc, char **argv)
{
   int fd;
   fd = open(GPSCNTL, O_RDWR | O_NOCTTY ); 
   if (fd <0) 
      {
      printf("Open of GPS data failed! RC=%d\r\n", fd);
      return -1;
      }
      
   struct termios options_data, options_cntl;
   
   tcgetattr(fd,&options_data);
   
   options_data.c_iflag &= ~(IXON | IXOFF | IXANY | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
   options_data.c_iflag |= IGNBRK;
   options_data.c_oflag = 0; 
   options_data.c_lflag = 0;
   options_data.c_cflag &= ~(PARENB | CSTOPB | CSIZE);
   options_data.c_cflag |= CLOCAL | HUPCL | CREAD | CS8 | B115200;
   options_data.c_cc[VMIN]     = 0; 
   options_data.c_cc[VTIME]    = 10;
   
   cfsetspeed(&options_data, B115200);

   tcflush(fd, TCIOFLUSH);
   tcsetattr(fd,TCSANOW,&options_data); 
   
   char cmd[] = "AT+QGPS=1\r";
   size_t wstat = write(fd, cmd, sizeof(cmd));
   if (wstat < 0) printf("Write GPS init commands error:%s\r\n", strerror(wstat));
   tcdrain(fd);
   
   int status =  close(fd);
   if (status) printf("Close of GPS control failed! RC=%d\r\n", status);
   
   //open data
   fd = open(GPSDATA, O_RDONLY | O_NOCTTY );
   if (fd <0) 
      {
      printf("Open of GPS control failed! RC=%d\r\n", fd);
      return -1;
      }
      
   tcgetattr(fd,&options_cntl); 

   options_data.c_iflag &= ~(IXON | IXOFF | IXANY | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
   options_data.c_iflag |= IGNBRK;
   options_data.c_oflag = 0; 
   options_data.c_lflag = 0;
   options_data.c_cflag &= ~(PARENB | CSTOPB | CSIZE);
   options_data.c_cflag |= CLOCAL | HUPCL | CREAD | CS8 | B115200;
   options_cntl.c_cc[VMIN]     = 0; 
   options_cntl.c_cc[VTIME]    = 10;
   
   cfsetspeed(&options_data, B115200);      

   tcflush(fd, TCIOFLUSH);
   tcsetattr(fd,TCSANOW,&options_cntl); 
   //loop thru read and print when cr or lf
   char rbuf[256], msg[256];
   int cnt, i=0, o=0;
   while(1)
      {
      cnt = read(fd,rbuf,255);
      for (i=0; i<cnt; i++)
         {
         if ((rbuf[i] == '\r') || (rbuf[i] == '\n'))
            {
            msg[o] = '\0';
            if (o) printf("GPS control port message %s\r\n", msg);
            o=0;
            }
         else
            {
            msg[o]=rbuf[i];
            o++;
            }
         }
      }
}
