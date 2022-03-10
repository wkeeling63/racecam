#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#define GPSDATA "/dev/ttyUSB1"
#define GPSCNTL "/dev/ttyUSB2"
int main(int argc, char **argv)
{
	int fd = open(GPSDATA, O_RDONLY | O_NOCTTY );
	if (fd <0) 
		{
		printf("Open of GPS data failed! RC=%d\r\n", fd);
		return -1;
		} 
		
   struct termios opt;
   printf("sizes and order ==> iflag, oflag, cflag and lflag %d, cline %d, c_cc %d, ispeed and ospeed %d\r\n", 
		sizeof(opt.c_iflag), sizeof(opt.c_line), sizeof(opt.c_cc), sizeof(opt.c_ispeed));
   tcgetattr(fd,&opt);
   int size = sizeof(opt), i;
   char *byte = (char *)&opt;
   printf("USB1>");
   for (i=0;i<size;i++)
		{
		printf("%hhx ", *byte);
		byte++;
		}
	printf("\r\n");
	printf("iflag %u oflag %u cflag %u lflag %u cline %d\r\n", opt.c_iflag, opt.c_oflag, opt.c_cflag, opt.c_lflag, opt.c_line);
	printf("c_cc ");
	for (i=0;i<16;i++)
		{
		printf("%d>%c<%hhx ", i ,opt.c_cc[i], opt.c_cc[i]);
		}
	printf("\r\n ispeed %u ospeed %u\r\n", opt.c_ispeed, opt.c_ospeed);
	
	fd = open(GPSCNTL, O_RDONLY | O_NOCTTY );
	if (fd <0) 
		{
		printf("Open of GPS control failed! RC=%d\r\n", fd);
		return -1;
		} 
		
   printf("sizes and order ==> iflag, oflag, cflag and lflag %d, cline %d, c_cc %d, ispeed and ospeed %d\r\n", 
		sizeof(opt.c_iflag), sizeof(opt.c_line), sizeof(opt.c_cc), sizeof(opt.c_ispeed));
   tcgetattr(fd,&opt);
   size = sizeof(opt), i;
   byte = (char *)&opt;
   printf("USB2>");
   for (i=0;i<size;i++)
		{
		printf("%hhx ", *byte);
		byte++;
		}
	printf("\r\n");
	printf("iflag %u oflag %u cflag %u lflag %u cline %u\r\n", opt.c_iflag, opt.c_oflag, opt.c_cflag, opt.c_lflag, opt.c_line);
	printf("c_cc ");
	for (i=0;i<16;i++)
		{
		printf("%d>%c<%hhx ", i ,opt.c_cc[i], opt.c_cc[i]);
		}
	printf("\r\n ispeed %u ospeed %u\r\n", opt.c_ispeed, opt.c_ospeed);

}
