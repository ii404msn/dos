#ifndef KERNEL_ENGINE_COLLECTOR_H
#define KERNEL_ENGINE_COLLECTOR_H

#include <stdint.h>
#include <map>
#include "thread_pool.h"
#include "mutex.h"

using ::baidu::common::Mutex;
using ::baidu::common::MutexLock;
using ::baidu::common::ThreadPool;

namespace dos {


struct CpuUsage {
  int64_t last_user_time;
  int64_t last_sys_time;
  int64_t last_idle_time;
  int64_t current_user_time;
  int64_t current_sys_time;
  int64_t current_idle_time;

  int64_t last_collect_time;
  int64_t current_collect_time;
  CpuUsage():last_user_time(-1),
  last_sys_time(-1),last_idle_time(-1),
  current_user_time(-1),
  current_sys_time(-1),
  current_idle_time(-1),
  last_collect_time(-1),
  current_collect_time(-1){}
};

struct ContainerUsage {
  // the cpu used in millicores
  uint64_t cpu_sys_usage;
  uint64_t cpu_user_usage;
  // the memory used in bytes
  uint64_t mem_rss_usage;
  uint64_t mem_cache_usage;

  ContainerUsage():cpu_sys_usage(0),
  cpu_user_usage(0),mem_rss_usage(0),
  mem_cache_usage(0) {}
};

struct ResourceUsage {
  CpuUsage cpu_usage;
};

// cgroup resource collector 
class CgroupResourceCollector {

public:
  CgroupResourceCollector();
  ~CgroupResourceCollector();
  bool Start();
  // set the interval of resource collector in microsecond
  void SetInterval(int32_t interval);
  // add task to collector , the cname is
  // container name which is used for finding
  // cpu and memory cgroup path
  void AddTask(const std::string& cname);
  // remove task from collector
  void RemoveTask(const std::string& cname);
  // get container resource usage
  bool GetContainerUsage(const std::string& cname, ContainerUsage* usage);

private:
  bool ParseProcStat(ResourceUsage& usage);
  bool ReadAll(const std::string& path, 
               std::string* content);
  void Collect();

  void CollectCpu(const std::string& cname,
                  ResourceUsage& usage);

private:
  Mutex mutex_;
  ThreadPool* thread_pool_;
  std::map<std::string, ResourceUsage>* usages_;
  int32_t interval_;
  bool enable_cpu_;
  bool enable_mem_;
  uint32_t cpu_millicores_;
};

}
#endif
