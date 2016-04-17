#include "engine/collector.h"

#include <iostream>
#include <fstream>
#include <gflags/gflags.h>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include "common/proc_helper.h"
#include "logging.h"
#include "timer.h"

DECLARE_string(ce_cgroup_root);
DECLARE_string(ce_isolators);
DECLARE_string(ce_cgroup_root_collect_task_name);

using ::baidu::common::INFO;
using ::baidu::common::WARNING;
using ::baidu::common::DEBUG;

namespace dos {

CgroupResourceCollector::CgroupResourceCollector():mutex_(),
  thread_pool_(NULL), usages_(NULL),
  enable_cpu_(false), enable_mem_(false),
  cpu_millicores_(0){
  thread_pool_ = new ThreadPool(1);
  usages_ = new std::map<std::string, ResourceUsage>();
  if (FLAGS_ce_isolators.find("cpu") != std::string::npos) {
    enable_cpu_ = true;
  }

  if (FLAGS_ce_isolators.find("memory") != std::string::npos) {
    enable_mem_ = true;
  }
  // cgroup root resource collect task
  ResourceUsage usage;
  usages_->insert(std::make_pair(FLAGS_ce_cgroup_root_collect_task_name, usage));
}

CgroupResourceCollector::~CgroupResourceCollector() {
  delete thread_pool_;
  delete usages_;
}

bool CgroupResourceCollector::Start() {
  Cpuinfo cpuinfo;
  bool load_ok = ProcHelper::LoadCpuinfo(&cpuinfo);
  if (!load_ok) {
    LOG(WARNING, "fail to load cpu info");
    return false;
  }
  cpu_millicores_ = cpuinfo.millicores;
  thread_pool_->DelayTask(interval_, boost::bind(&CgroupResourceCollector::Collect, this));
  LOG(INFO, "start collector successfully with cpu millicores %d", cpu_millicores_);
  return true;
}

void CgroupResourceCollector::SetInterval(int32_t interval) {
  MutexLock lock(&mutex_);
  interval_ = interval;
}

void CgroupResourceCollector::AddTask(const std::string& name) {
  MutexLock lock(&mutex_);
  std::map<std::string, ResourceUsage>::iterator it = usages_->find(name);
  if (it != usages_->end()) {
    return;
  }
  LOG(INFO, "add collect task for container %s", name.c_str());
  ResourceUsage usage;
  usages_->insert(std::make_pair(name, usage));
}

void CgroupResourceCollector::RemoveTask(const std::string& cname) {
  MutexLock lock(&mutex_);
  usages_->erase(cname);
}

void CgroupResourceCollector::Collect() {
  MutexLock lock(&mutex_);
  uint64_t now = ::baidu::common::timer::get_micros();
  std::map<std::string, ResourceUsage>::iterator it = usages_->begin();
  for (; it != usages_->end(); ++it) {
    CollectCpu(it->first, it->second);
    LOG(DEBUG, "container %s current_sys_time %ld", it->first.c_str(), it->second.cpu_usage.current_sys_time);
  }
  uint64_t consume = ::baidu::common::timer::get_micros() - now;
  LOG(INFO, "cgroup collector consume %ld", consume);
  thread_pool_->DelayTask(interval_, boost::bind(&CgroupResourceCollector::Collect, this));
}

void CgroupResourceCollector::CollectCpu(const std::string& name,
                                         ResourceUsage& usage) {
  mutex_.AssertHeld();
  if (!enable_cpu_) {
    return;
  }
  LOG(DEBUG, "start to collect cpu stat for container %s", name.c_str());
  std::string path = FLAGS_ce_cgroup_root + "/cpuacct/" + name + "/cpuacct.stat";
  if (name == FLAGS_ce_cgroup_root_collect_task_name) {
    ParseProcStat(usage);
    return;
  }
  std::string content;
  bool ok = ReadAll(path, &content);
  if (!ok) {
    return;
  }
  std::vector<std::string> lines;
  boost::split(lines, content, boost::is_any_of("\n"));
  for (size_t index = 0; index < lines.size(); ++index) {
    if (lines[index].empty()) {
      continue;
    }
    std::string& line = lines[index];
    std::istringstream ss(line);
    std::string name;
    uint64_t use_time;
    ss >> name >> use_time;
    if (ss.fail()) {
      LOG(WARNING, "line format err %s", line.c_str()); 
      return;
    }
    if (name == "user") {
      usage.cpu_usage.last_user_time = usage.cpu_usage.current_user_time;
      usage.cpu_usage.current_user_time = use_time;
    } else if (name == "system") {
      usage.cpu_usage.last_sys_time = usage.cpu_usage.current_sys_time;
      usage.cpu_usage.current_sys_time = use_time;
    } else {
      LOG(WARNING, "invalid name %s %s", 
                   name.c_str(), line.c_str());
    }
  }
  usage.cpu_usage.last_collect_time = usage.cpu_usage.current_collect_time;
  usage.cpu_usage.current_collect_time = ::baidu::common::timer::get_micros();
}

// collect the node resource usage by /proc/stat
bool CgroupResourceCollector::ParseProcStat(ResourceUsage& usage) {
  mutex_.AssertHeld();
  std::string stat;
  bool read_ok = ReadAll("/proc/stat", &stat);
  if (!read_ok) {
    LOG(WARNING, "fail to read /proc/stat");
    return false;
  }
  std::vector<std::string> lines;
  boost::split(lines, stat, boost::is_any_of("\n"));
  if (lines.size() <= 0) {
    LOG(WARNING, "invalid content from /proc/stat");
    return false;
  }
  std::vector<std::string> parts;
  std::string first_line = lines[0];
  boost::split(parts, first_line, boost::is_any_of(" "));
  if (parts.size() < 12) {
    LOG(WARNING, "invalid content from /proc/stat");
    return false;
  }
  int64_t user_time = boost::lexical_cast<int64_t>(parts[2]);
  int64_t nice_time = boost::lexical_cast<int64_t>(parts[3]);
  int64_t system_time = boost::lexical_cast<int64_t>(parts[4]);
  int64_t idle_time = boost::lexical_cast<int64_t>(parts[5]);
  int64_t iowait_time = boost::lexical_cast<int64_t>(parts[6]);
  int64_t irp_time = boost::lexical_cast<int64_t>(parts[7]);
  int64_t softirq_time = boost::lexical_cast<int64_t>(parts[8]);
  usage.cpu_usage.last_user_time = usage.cpu_usage.current_user_time;
  usage.cpu_usage.current_user_time = user_time + nice_time + iowait_time + irp_time + softirq_time;
  usage.cpu_usage.last_sys_time = usage.cpu_usage.current_sys_time;
  usage.cpu_usage.current_sys_time = system_time;
  usage.cpu_usage.last_collect_time = usage.cpu_usage.current_collect_time;
  usage.cpu_usage.current_collect_time = ::baidu::common::timer::get_micros();
  usage.cpu_usage.last_idle_time = usage.cpu_usage.current_idle_time;
  usage.cpu_usage.current_idle_time = idle_time;
  return true;
}

bool CgroupResourceCollector::ReadAll(const std::string& path,
    std::string* content) {
  if (content == NULL) {
    return false;
  }
  FILE* fp = fopen(path.c_str(), "rb");
  if (fp == NULL) {
    LOG(WARNING, "fail to read file %s", path.c_str());
    return false;
  }
  char buf[1024];
  int len = 0;
  while ((len = fread(buf, 1, sizeof(buf), fp)) > 0) {
    content->append(buf, len);
  }
  fclose(fp); 
  return true;
}

bool CgroupResourceCollector::GetContainerUsage(const std::string& cname, 
                                                ContainerUsage* usage) { 
  MutexLock lock(&mutex_);
  std::map<std::string, ResourceUsage>::iterator it = usages_->find(cname);
  if (it == usages_->end()) {
    LOG(WARNING, "container %s does not exist in collector", cname.c_str());
    return false;
  }
  // the cpu usage 
  ResourceUsage& rusage = it->second;
  if (rusage.cpu_usage.last_user_time < 0) { 
    LOG(WARNING, "collector is not read for container %s", cname.c_str());
    return true;
  }
  // container stat
  int64_t used_time = rusage.cpu_usage.current_user_time - rusage.cpu_usage.last_user_time;
  int64_t sys_time = rusage.cpu_usage.current_sys_time - rusage.cpu_usage.last_sys_time;
  LOG(DEBUG, "container %s user time %ld sys time %ld", cname.c_str(),
      used_time, sys_time);
  std::map<std::string, ResourceUsage>::iterator root_it = usages_->find(FLAGS_ce_cgroup_root_collect_task_name);
  if (root_it == usages_->end()) {
    LOG(WARNING, "no root resource stat %s", FLAGS_ce_cgroup_root_collect_task_name.c_str());
    return false;
  }
  ResourceUsage& root_usage = root_it->second;
  if (root_usage.cpu_usage.last_idle_time < 0) {
    LOG(WARNING, "collector is not read for root resource %s", FLAGS_ce_cgroup_root_collect_task_name.c_str());
    return true;
  }
  int64_t total_used_time = root_usage.cpu_usage.current_user_time - rusage.cpu_usage.last_user_time;
  int64_t total_sys_time = root_usage.cpu_usage.current_sys_time - rusage.cpu_usage.last_sys_time;
  int64_t total_idle_time = root_usage.cpu_usage.current_idle_time - rusage.cpu_usage.last_idle_time;

  int64_t total_time = total_idle_time + total_sys_time + total_used_time;
  int64_t used_millicores = (cpu_millicores_ * used_time) / total_time;
  int64_t sys_millicores = (cpu_millicores_ * sys_time) / total_time;
  LOG(DEBUG, "container %s user %ld sys %ld in millicores", cname.c_str(), used_millicores, sys_millicores);
  usage->cpu_sys_usage = sys_millicores;
  usage->cpu_user_usage = used_millicores;
  return true;
}

}
