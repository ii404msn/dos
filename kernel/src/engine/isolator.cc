#include "engine/isolator.h"

#include <stdio.h>
#include "engine/utils.h"
#include "logging.h"

using ::baidu::common::INFO;
using ::baidu::common::WARNING;
using ::baidu::common::DEBUG;

namespace dos {

CpuIsolator::CpuIsolator(const std::string& path):
  path_(path){}

CpuIsolator::~CpuIsolator() {}

bool CpuIsolator::Init() {
  bool ok = Mkdir(path_);
  if (!ok) {
    LOG(WARNING,"fail to mkdir %s", path_.c_str());
    return false;
  }
  return true;
}

bool CpuIsolator::Attach(int32_t pid) {
  std::string cgroup_proc = path_ + "/cgroup.proc";
  FILE* fd = fopen(cgroup_proc.c_str(), "ae");
  if (!fd) {
    LOG(WARNING, "fail to open %s", cgroup_proc.c_str());
    return false;
  }
  int ok = fprintf(fd, "%d", pid);
  fclose(fd);
  if (ok <=0) {
    LOG(WARNING, "fail to write pid %d to %s", pid, cgroup_proc.c_str());
    return false;
  }
  return true;
}

bool CpuIsolator::AssignQuota(int32_t quota) {
  return true;
}

bool CpuIsolator::AssignLimit(int32_t limit) {
  return true;
}

} // end of namespace
