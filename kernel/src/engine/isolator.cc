#include "engine/isolator.h"

#include <stdio.h>
#include "engine/utils.h"
#include "logging.h"

using ::baidu::common::INFO;
using ::baidu::common::WARNING;
using ::baidu::common::DEBUG;

namespace dos {

//cpu cfs sched period
const static int CPU_CFS_PERIOD = 100000;

CpuIsolator::CpuIsolator(const std::string& cpu_path,
                         const std::string& cpu_acct_path):
  cpu_path_(cpu_path),
  cpu_acct_path_(cpu_acct_path){}

CpuIsolator::~CpuIsolator() {}

bool CpuIsolator::Init() {
  bool ok = Mkdir(cpu_path_);
  if (!ok) {
    LOG(WARNING,"fail to mkdir %s", cpu_path_.c_str());
    return false;
  }
  return true;
}

bool CpuIsolator::Attach(int32_t pid) {
  std::string cgroup_proc = cpu_path_ + "/cgroup.procs";
  FILE* fd = fopen(cgroup_proc.c_str(), "ae");
  if (!fd) {
    LOG(WARNING, "fail to open %s", cgroup_proc.c_str());
    return false;
  }
  LOG(DEBUG, "add pid %d to cgroup %s", pid, cgroup_proc.c_str());
  int ok = fprintf(fd, "%d", pid);
  fclose(fd);
  if (ok <=0) {
    LOG(WARNING, "fail to write pid %d to %s", pid, cgroup_proc.c_str());
    return false;
  }
  return true;
}

bool CpuIsolator::AssignQuota(int32_t quota) {
  std::string cpu_share = cpu_path_ + "/cpu.share";
  FILE* fd = fopen(cpu_share.c_str(), "ae");
  if (!fd) {
    LOG(WARNING, "fail to open %s", cpu_share.c_str());
    return false;
  }
  LOG(DEBUG, "add quota %d to cgroup %s", quota, cpu_share.c_str());
  int ok = fprintf(fd, "%d", quota);
  fclose(fd);
  if (ok <=0) {
    LOG(WARNING, "fail to write quota to %s", cpu_share.c_str());
    return false;
  }
  return true;
}

bool CpuIsolator::AssignLimit(int32_t limit) {
  std::string cpu_limit = cpu_path_ + "/cpu.cfs_quota_us";
  FILE* fd = fopen(cpu_limit.c_str(), "ae");
  if (!fd) {
    LOG(WARNING, "fail to open %s", cpu_limit.c_str());
    return false;
  }
  LOG(DEBUG, "add limit %d to cgroup %s", limit, cpu_limit.c_str());
  int ok = fprintf(fd, "%d", limit);
  fclose(fd);
  if (ok <=0) {
    LOG(WARNING, "fail to write limit to %s", cpu_limit.c_str());
    return false;
  }
  return true;
}

bool CpuIsolator::GetCpuUsage(int32_t* used) {
  return false;
}

bool CpuIsolator::GetPids(std::set<int32_t>* pids) {
  return false;
}

bool CpuIsolator::Destroy() {
  return false;
}

} // end of namespace
