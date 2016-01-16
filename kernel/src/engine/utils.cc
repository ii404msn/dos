#include "engine/utils.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include "logging.h"

using ::baidu::common::INFO;
using ::baidu::common::WARNING;

namespace dos {

bool Mkdir(const std::string& path) {
	const int dir_mode = 0777;
  int ret = ::mkdir(path.c_str(), dir_mode); 
  if (ret == 0 || errno == EEXIST) {
      return true; 
  }
  LOG(WARNING, "mkdir %s failed err[%d: %s]", 
        path.c_str(), errno, strerror(errno));
  return false;
}

}
