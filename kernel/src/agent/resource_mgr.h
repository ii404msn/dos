#ifndef KERNEL_AGENT_RESOURCE_MGR_H
#define KERNEL_AGENT_RESOURCE_MGR_H
#include <stdint.h>
#include "proto/dos.pb.h"

namespace dos {

class ResourceMgr {

public:
  ResourceMgr();
  ~ResourceMgr();
  // set max cpu that is shared
  // the unit is millicore
  bool InitCpu(uint64_t limit);
  // set max memory that is shared
  // the unit is byte
  bool InitMemory(uint64_t memory);
  // load resource from local machine
  bool LoadFromLocal(double cpu_rate, 
                     double mem_rate);
  // config the port range for sharing
  bool InitPort(uint32_t start, 
                uint32_t end);

  // alloc resource
  bool Alloc(const Resource& require);
  // release resource
  bool Release(const Resource& require);
  void Stat(Resource* resource);
private:
  Resource resource_;
};

}
#endif
