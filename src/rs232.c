#include <linux/serial.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <string.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

int cport;
int error;
int extsig;

struct termios new_port_settings, old_port_settings;
       

FILE *RS232_OpenComport(char* path, int baudrate)
{
  int baudr, status;
  int customBaud = 0;
  
  struct serial_struct sstruct;


  switch(baudrate)
  {
    case      50 : baudr = B50;
                   break;
    case      75 : baudr = B75;
                   break;
    case     110 : baudr = B110;
                   break;
    case     134 : baudr = B134;
                   break;
    case     150 : baudr = B150;
                   break;
    case     200 : baudr = B200;
                   break;
    case     300 : baudr = B300;
                   break;
    case     600 : baudr = B600;
                   break;
    case    1200 : baudr = B1200;
                   break;
    case    1800 : baudr = B1800;
                   break;
    case    2400 : baudr = B2400;
                   break;
    case    4800 : baudr = B4800;
                   break;
    case    9600 : baudr = B9600;
                   break;
    case   19200 : baudr = B19200;
                   break;
    case   38400 : baudr = B38400;
                   break;
    case   57600 : baudr = B57600;
                   break;
    case  115200 : baudr = B115200;
                   break;
    case  230400 : baudr = B230400;
                   break;
    case  460800 : baudr = B460800;
                   break;
    case  500000 : baudr = B500000;
                   break;
    case  576000 : baudr = B576000;
                   break;
    case  921600 : baudr = B921600;
                   break;
    case 1000000 : baudr = B1000000;
                   break;
    case 2000000 : baudr = B2000000;
                   break;
    case 3000000 : baudr = B3000000;
                   break;
    case 4000000 : baudr = B4000000;
                   break;
    default      : printf("custom baudrate; proceed at own risk\n");
				   baudr = B38400;
				   customBaud = 1;
                   break;
  }

  cport = open("/dev/ttyAMA0", O_RDWR | O_NOCTTY | O_NDELAY);
  if(cport==-1)
  {
    perror("unable to open comport ");
    return NULL;
  }

  error = tcgetattr(cport, &old_port_settings);
  if(error==-1)
  {
    close(cport);
    perror("unable to read portsettings ");
    return NULL;
  }
  memset(&new_port_settings, 0, sizeof(new_port_settings));  /* clear the new struct */

  new_port_settings.c_cflag = baudr | CS8 | CLOCAL | CREAD;
  //new_port_settings.c_cflag = baudr | CS8 | CLOCAL | CREAD | CRTSCTS;
 // new_port_settings.c_cflag = baudr | CS8 | CLOCAL | CREAD | ( CCTS_OFLOW | CRTS_IFLOW | CDTR_IFLOW | CDSR_OFLOW );
  new_port_settings.c_iflag = IGNPAR | IGNBRK;
  new_port_settings.c_oflag = 0;
  new_port_settings.c_lflag = 0;
  new_port_settings.c_cc[VMIN] = 0;      /* block untill n bytes are received */
  new_port_settings.c_cc[VTIME] = 0;     /* block untill a timer expires (n * 100 mSec.) */
  error = tcsetattr(cport, TCSANOW, &new_port_settings);
  if(error==-1)
  {
    close(cport);
    perror("unable to adjust portsettings ");
    return NULL;
  }
  
  
  if ( customBaud ) {
	printf("Setting custom baud rate\n");
	if(ioctl(cport, TIOCGSERIAL, &sstruct) < 0){
		printf("Error: could not get comm ioctl\n"); 
		exit(0); 
	}
	//def_sstruct = sstruct;
	sstruct.custom_divisor = 2;
	//sstruct.flags &= 0xffff ^ ASYNC_SPD_MASK; //NO! makes read fail.
	sstruct.flags |= ASYNC_SPD_CUST; 
	if(ioctl(cport, TIOCSSERIAL, &sstruct) < 0){
		printf("Error: could not set custom comm baud divisor\n"); 
		exit(0); 
	}
  } else {
	if(ioctl(cport, TIOCGSERIAL, &sstruct) < 0){
		printf("Error: could not get comm ioctl\n"); 
		exit(0); 
	}
	//sstruct = def_sstruct;
	//sstruct.flags &= 0xffff ^ ASYNC_SPD_MASK; //NO! makes read fail.
	
	sstruct.custom_divisor = 1;
	sstruct.flags &= ~ASYNC_SPD_CUST; 
	if(ioctl(cport, TIOCSSERIAL, &sstruct) < 0){
		printf("Error: could not set custom comm baud divisor\n"); 
		exit(0); 
	}
  }
  
  return fdopen(cport, "w+");
}
