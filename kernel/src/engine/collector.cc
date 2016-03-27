#include "engine/collector.h"
#include "logging.h"

namespace dos {

CgroupResourceCollector::CgroupResourceCollector():mutex_(),
  thread_pool_(NULL), usages_(NULL){
  thread_pool_ = new ThreadPool(1);
  usages_ = new std::map<std::string, ResourceUsage>();
}

CgroupResourceCollector::~CgroupResourceCollector() {
  delete thread_pool_;
  delete containers_;
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
  ResouceUsage usage;
  usages_->insert(std::make_pair(name, usage));
}

void CgroupResourceCollector::RemoveTask(const std::string& cname) {
  MutexLock lock(&mutex_);
  usages_->erase(cname);
}

void CgroupResourceCollector::Collect() {
  MutexLock lock(&mutex_);

}

}
