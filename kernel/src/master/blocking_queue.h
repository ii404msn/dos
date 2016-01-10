#ifndef KERNEL_MASTER_BLOCKING_QUEUE_H
#define KERNEL_MASTER_BLOCKING_QUEUE_H

#include <queue>
#include "mutex.h"

namespace dos {
template <typename T>
class FixedBlockingQueue {

public:
  FixedBlockingQueue(uint32_t capacity):mutex_(),
                        cond_(&mutex_),
                        capacity_(capacity){}
  ~FixedBlockingQueue(){}

  T& Front() {
    MutexLock(&mutex_);
    if (queue_.size <= 0) {
      cond_.Wait("queue is empty");
    }
    return queue_.Front();
  }
  void Pop() {
    mutex_.AssertHeld();
    queue_.pop();
  }
  void Push(const T& t) {
    MutexLock(&mutex_);
    if (queue_.size() >= capacity_) {
      cond_.Wait("queue is full");
    }
    queue_.Push(t);
    cond_.Signal();
  }

private:
  ::baidu::common::Mutex mutex_;
  ::baidu::common::CondVar cond_;
  uint32_t capacity_;
  std::queue<T> queue_;
};

}
#endif
