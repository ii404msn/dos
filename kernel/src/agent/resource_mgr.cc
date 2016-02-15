#include "agent/resource_mgr.h"

#include "agent/proc_helper.h"
#include "logging.h"
#include "string_util.h"
#include "common/resource_util.h"

using ::baidu::common::INFO;
using ::baidu::common::DEBUG;
using ::baidu::common::WARNING;

namespace dos {

ResourceMgr::ResourceMgr():resource_(){}

ResourceMgr::~ResourceMgr() {}

bool ResourceMgr::InitCpu(uint64_t limit) {
  resource_.mutable_cpu()->set_limit(limit);
  resource_.mutable_cpu()->set_share(limit);
  resource_.mutable_cpu()->set_used(0);
  resource_.mutable_cpu()->set_assigned(0);
  LOG(INFO, "init cpu %ld millicores", limit);
  return true;
}

bool ResourceMgr::InitMemory(uint64_t memory) {
  resource_.mutable_memory()->set_limit(memory);
  resource_.mutable_memory()->set_used(0); 
  resource_.mutable_memory()->set_assigned(0);
  LOG(INFO, "init memory %s", ::baidu::common::HumanReadableString(memory).c_str());
  return true;
}

bool ResourceMgr::LoadFromLocal(double cpu_rate, 
                                double mem_rate) {
  Meminfo meminfo;
  bool load_mem_ok = ProcHelper::LoadMeminfo(&meminfo);
  if (!load_mem_ok) {
    return false;
  }

  Cpuinfo cpuinfo;
  bool load_cpu_ok = ProcHelper::LoadCpuinfo(&cpuinfo);
  if (!load_cpu_ok) {
    return false;
  }

  bool init_cpu_ok = InitCpu(cpuinfo.millicores * cpu_rate);
  if (!init_cpu_ok) {
    return false;
  }

  bool init_mem_ok = InitMemory(meminfo.total * mem_rate);
  if (!init_mem_ok) {
    return false;
  }
  return true;
}

bool ResourceMgr::InitPort(uint32_t start,
                           uint32_t end) {
  resource_.mutable_port()->mutable_range()->set_start(start);
  resource_.mutable_port()->mutable_range()->set_end(end);
  LOG(INFO, "init port with range[%d, %d]", start, end);
  return true;
}

bool ResourceMgr::Alloc(const Resource& require) {
  return ResourceUtil::Alloc(require, &resource_);
}

bool ResourceMgr::Release(const Resource& require) {
  return ResourceUtil::Release(require, &resource_);
}

void ResourceMgr::Stat(Resource* resource) {
  resource->CopyFrom(resource_);
}

} // end of namespace dos
