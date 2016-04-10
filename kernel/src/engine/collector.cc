#include "engine/collector.h"

#include <iostream>
#include <fstream>
#include <gflags/gflags.h>
#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include "logging.h"
#include "timer.h"

DECLARE_string(ce_cgroup_root);
DECLARE_string(ce_isolators);

using ::baidu::common::INFO;
using ::baidu::common::WARNING;
using ::baidu::common::DEBUG;

namespace dos {

CgroupResourceCollector::CgroupResourceCollector():mutex_(),
  thread_pool_(NULL), usages_(NULL),
  enable_cpu_(false), enable_mem_(false){
  thread_pool_ = new ThreadPool(1);
  usages_ = new std::map<std::string, ResourceUsage>();
  if (FLAGS_ce_isolators.find("cpu") != std::string::npos) {
    enable_cpu_ = true;
  }

  if (FLAGS_ce_isolators.find("memory") != std::string::npos) {
    enable_mem_ = true;
  }
}

CgroupResourceCollector::~CgroupResourceCollector() {
  delete thread_pool_;
  delete usages_;
}

void CgroupResourceCollector::Start() {
  thread_pool_->DelayTask(interval_, boost::bind(&CgroupResourceCollector::Collect, this));
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

}
