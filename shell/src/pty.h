#ifndef KERNEL_CMD_PTY_H
#define KERNEL_CMD_PTY_H
#include <string>
namespace dos {

class Pty {

public:
  Pty(){}
  ~Pty(){}
  bool Create(int* master, std::string* pty_path);
  bool ConnectMaster(int master);
};

}
#endif
