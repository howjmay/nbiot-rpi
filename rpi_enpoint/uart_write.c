#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/select.h>

int set_interface_attribs(int fd, int speed) {
  struct termios tty;

  if (tcgetattr(fd, &tty) < 0) {
    printf("Error from tcgetattr: %s\n", strerror(errno));
    return -1;
  }

  cfsetospeed(&tty, (speed_t)speed);
  cfsetispeed(&tty, (speed_t)speed);

  tty.c_cflag |= (CLOCAL | CREAD); /* ignore modem controls */
  tty.c_cflag &= ~CSIZE;
  tty.c_cflag |= CS8;      /* 8-bit characters */
  tty.c_cflag &= ~PARENB;  /* no parity bit */
  tty.c_cflag &= ~CSTOPB;  /* only need 1 stop bit */
  tty.c_cflag |= CRTSCTS; /* no hardware flowcontrol */

  /* setup for non-canonical mode */
  tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
  tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
  tty.c_oflag &= ~OPOST;

  /* fetch bytes as they become available */
  tty.c_cc[VMIN] = 1;
  tty.c_cc[VTIME] = 1;

  if (tcsetattr(fd, TCSANOW, &tty) != 0) {
    printf("Error from tcsetattr: %s\n", strerror(errno));
    return -1;
  }
  return 0;
}

void set_mincount(int fd, int mcount) {
  struct termios tty;

  if (tcgetattr(fd, &tty) < 0) {
    printf("Error tcgetattr: %s\n", strerror(errno));
    return;
  }

  tty.c_cc[VMIN] = mcount ? 1 : 0;
  tty.c_cc[VTIME] = 5; /* half second timer */

  if (tcsetattr(fd, TCSANOW, &tty) < 0) printf("Error tcsetattr: %s\n", strerror(errno));
}

int main() {
  char *portname = "/dev/ttyUSB0";
  int fd;
  int wlen;

  fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC | O_NDELAY);
  if (fd < 0) {
    printf("Error opening %s: %s\n", portname, strerror(errno));
    return -1;
  }
  /*baudrate 115200, 8 bits, no parity, 1 stop bit */
  set_interface_attribs(fd, B9600);
  // set_mincount(fd, 0);                /* set to pure timed read */
  
  fd_set wset;
  struct timeval tv;
  tv.tv_sec = 0;
	tv.tv_usec = 500;
  for (int i = 1000; i < 5000; i+=100) {
    /* simple output */
    FD_ZERO(&wset);
    FD_SET(fd, &wset);
    int rs = select(fd + 1, NULL, &wset, NULL, &tv);
    if (rs < 0) {
      printf("select error \n");
    }

    if (FD_ISSET(fd, &wset)) {
      char str_temp[] = "input = %d\r\n";
      char str[20] = {0};
      sprintf(str, str_temp, i);
      printf("%s, %ld ",str, strlen(str));
      wlen = write(fd, str, strlen(str));
      if (wlen != strlen(str)) {
        printf("Error from write: %d, %d\n", wlen, errno);
      }
#if 0
      tcdrain(fd);
      if (tcflush(fd, TCIOFLUSH) != 0){
          perror("tcflush error");
      }
#endif
    }
    sleep(1);
  }
}