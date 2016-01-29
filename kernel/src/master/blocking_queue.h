#ifndef KERNEL_MASTER_BLOCKING_QUEUE_H
#define KERNEL_MASTER_BLOCKING_QUEUE_H

#include <queue>
#include "mutex.h"
#include "logging.h"

using ::baidu::common::MutexLock;
using ::baidu::common::INFO;
using ::baidu::common::WARNING;

namespace dos {

template <typename T>
class FixedBlockingQueue {

public:
  FixedBlockingQueue(uint32_t capacity, const std::string& name):mutex_(),
                        cond_(&mutex_),
                        capacity_(capacity),
  name_(name){}
  ~FixedBlockingQueue(){}

  T Pop() {
    MutexLock lock(&mutex_);
    while (queue_.size() <= 0) {
      LOG(INFO, "%s queue is empty", name_.c_str());
      cond_.Wait();
    }
    T t = queue_.front();
    queue_.pop();
    cond_.Signal();
    return t;
  }

  void Push(const T& t) {
    MutexLock lock(&mutex_);
    while (queue_.size() >= capacity_) {
      LOG(WARNING, "%s queue is full", name_.c_str());
      cond_.Wait();
    }
    queue_.push(t);
    cond_.Signal();
  }

private:
  ::baidu::common::Mutex mutex_;
  ::baidu::common::CondVar cond_;
  uint32_t capacity_;
  std::queue<T> queue_;
  std::string name_;
};

}
#endif
