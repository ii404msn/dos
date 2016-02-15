#ifndef KERNEL_AGENT_PROC_HELPER_H
#define KERNEL_AGENT_PROC_HELPER_H
#include <stdint.h>

namespace dos {

struct Meminfo {
  uint64_t total;
  uint64_t free;
  uint64_t buffer;
  uint64_t cached;
};

struct Cpuinfo {
  uint64_t millicores;
};

class ProcHelper {

public:
  ProcHelper(){}
  ~ProcHelper(){}
  // load /proc/meminfo to struct 
  static bool LoadMeminfo(Meminfo* meminfo);
  // load /proc/cpuinfo to struct
  static bool LoadCpuinfo(Cpuinfo* cpuinfo);
};

} // end of namespace dos
#endif
