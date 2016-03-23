#ifndef KERNEL_ENGINE_CGROUP_V1_H
#define KERNEL_ENGINE_CGROUP_V1_H
#include <set>
#include <string>
#include <stdint.h>

namespace dos {

class CpuCtrl {
public:
  // specify the cpu controller path, if it does not exist,
  // the create it.
  CpuCtrl(const std::string& path);
  // attach pid the controller
  bool Attach(int32_t pid);
  // get all pids from the controller
  bool GetPids(std::set<int32_t>& pids);
  // set cpu limit
  bool AssignLimit(int64_t millicores);
  // set cpu share
  bool AssignShare(int64_t weight);
private:
  std::string path_;
  // limit in millicores
  int64_t limit_;
  // share 
  int64_t share_;
};

} // end of namespace dos
#endif
