#include "engine/isolator.h"

#include <stdio.h>
#include <vector>
#include "engine/utils.h"
#include "logging.h"

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

using ::baidu::common::INFO;
using ::baidu::common::WARNING;
using ::baidu::common::DEBUG;

namespace dos {

//cpu cfs sched period
const static int CPU_CFS_PERIOD = 100000;

CgroupBase::CgroupBase(const std::string& path):
  path_(path) {}

CgroupBase::~CgroupBase(){}

bool CgroupBase::Init() {
  bool ok = Mkdir(path_);
  if (!ok) {
    LOG(WARNING,"fail to mkdir %s", path_.c_str());
    return false;
  }
  LOG(DEBUG, "create cgroup path %s successfully", path_.c_str());
  return false;
}

bool CgroupBase::Attach(int32_t pid) {
  std::string cgroup_proc = path_ + "/tasks";
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
  LOG(DEBUG, "add pid %d to cgroup %s", pid, cgroup_proc.c_str());
  return true;
}

bool CgroupBase::GetPids(std::set<int32_t>* pids) {
  std::string procs = path_ + "/cgroup.procs";
  FILE* fd = fopen(procs.c_str(), "re");
  if (!fd) {
    LOG(WARNING, "fail to open %s", procs.c_str());
    return false;
  }
  char buffer[1024];
  std::string content;
  while (feof(fd)) {
    size_t read_len = fread((void*)buffer, 
                            sizeof(char),
                            1024,
                            fd);
    if (!read_len) {
      break;
    }
    content.append(buffer, read_len);
  }
  LOG(DEBUG, "read pids from %s with content %s", procs.c_str(),
      content.c_str());
  std::vector<std::string> str_pids;
  boost::split(str_pids, content, boost::is_any_of("\n"));
  for (size_t index = 0; index < str_pids.size(); ++index) {
    if (str_pids[index].empty()) {
      continue;
    }
    pids->insert(boost::lexical_cast<int32_t>(str_pids[index]));
  }
  return false;
}

bool CgroupBase::Destroy() {
  return false;
}

CpuIsolator::CpuIsolator(const std::string& cpu_path,
                         const std::string& cpu_acct_path):
  cpu_path_(cpu_path),
  cpu_acct_path_(cpu_acct_path),
  cpu_base_(NULL),
  cpu_acct_base_(NULL){
  cpu_base_ = new CgroupBase(cpu_path_); 
  cpu_acct_base_ = new CgroupBase(cpu_acct_path_);
}

CpuIsolator::~CpuIsolator() {
  delete cpu_base_;
  delete cpu_acct_base_;
}

bool CpuIsolator::Init() {
  bool cpu_ok = cpu_base_->Init();
  if (!cpu_ok) {
    return false;
  }
  bool cpu_acct_ok = cpu_acct_base_->Init();
  if (!cpu_acct_ok) {
    return false;
  }
  return true;
}

bool CpuIsolator::Attach(int32_t pid) {
  bool attach_ok = cpu_base_->Attach(pid);
  if (!attach_ok) {
    return false;
  }
  attach_ok = cpu_acct_base_->Attach(pid);
  if (!attach_ok) {
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
  return cpu_base_->GetPids(pids);
}

bool CpuIsolator::Destroy() {
  bool destroy_ok = cpu_base_->Destroy();
  if (!destroy_ok) {
    return false;
  }
  return cpu_acct_base_->Destroy();
}

} // end of namespace
