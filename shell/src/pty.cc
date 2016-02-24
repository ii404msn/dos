#include "pty.h"

#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <signal.h>
#include "logging.h"

using ::baidu::common::INFO;
using ::baidu::common::WARNING;

namespace dos {

bool Pty::Create(int* master, std::string* pty_path) {
  if (master == NULL || pty_path == NULL) {
    return false;
  }
  *master = -1;
  *master = ::posix_openpt(O_RDWR);
  if (*master < 0) {
    LOG(WARNING, "fail to create pty master %d, %s", errno, strerror(errno)); 
    return false;
  }
  int ret = ::grantpt(*master);
  if (ret != 0) {
    LOG(WARNING, "grantpt err[%d: %s]", errno, strerror(errno)); 
    ::close(*master);
    return false;
  }
  ret = ::unlockpt(*master);
  if (ret != 0) {
    LOG(WARNING, "unlockpt err[%d: %s]", errno, strerror(errno));
    ::close(*master);
    return false;
  }
  pty_path->clear();
  pty_path->append(::ptsname(*master));
  return true;
}

bool Pty::ConnectMaster(int master) {
  if (master < 0) {
    return false; 
  }
  struct termios temp_termios;
  struct termios orig_termios;
  ::tcgetattr(0, &orig_termios);
  temp_termios = orig_termios;
  temp_termios.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ECHOPRT | ECHOKE | ICRNL | ISIG);
  temp_termios.c_cc[VTIME] = 1;   
  temp_termios.c_cc[VMIN] = 1; 
  ::tcsetattr(0, TCSANOW, &temp_termios);
  const int INPUT_BUFFER_LEN = 1024 * 10;
  char input[INPUT_BUFFER_LEN];
  fd_set fd_in; 
  int ret = 0;
  while (1) {
    FD_ZERO(&fd_in); 
    FD_SET(0, &fd_in);
    FD_SET(master, &fd_in);
    ret = ::select(master + 1, &fd_in, NULL, NULL, NULL);
    switch (ret) {
      case -1 :  
        fprintf(stderr, "select err[%d: %s]\n", 
                  errno, strerror(errno));
        break;
      default : {
        if (FD_ISSET(0, &fd_in))  {
          ret = ::read(0, input, sizeof(input)); 
          if (ret > 0) {
            ::write(master, input, ret); 
          } else {
            if (ret < 0) {
              fprintf(stderr, "write err[%d: %s]\n", errno, strerror(errno)); 
              break;
            }
          }
        }
        if (FD_ISSET(master, &fd_in)) {
          ret = ::read(master, input, sizeof(input)); 
          if (ret > 0) {
            ::write(1, input, ret);
          } else if (ret == 0) {
            fprintf(stdout, "terminal exit \n");
            break; 
          } else {
            fprintf(stderr, "read err[%d: %s]\n", errno, strerror(errno)); 
            break;
          }
        }
      }
    }
    if (ret < 0) {
      break; 
    }
  }
  ::tcsetattr(0, TCSANOW, &orig_termios);
  if (ret < 0) {
    return false; 
  }
  return true;
}

}
