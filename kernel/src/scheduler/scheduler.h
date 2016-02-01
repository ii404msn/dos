#ifndef KERNEL_SCHEDULER_SCHEDULER_H
#define KERNEL_SCHEDULER_SCHEDULER_H

#include "rpc/rpc_client.h"
#include "proto/master.pb.h"
#include "thread_pool.h"

namespace dos {

class Scheduler {

public:
  Scheduler(const std::string& master_addr);
  ~Scheduler();
  bool Start();
private:
  void GetScaleUpPods();
private:
  RpcClient* rpc_client_;
  Master_Stub* master_;
  std::string master_addr_;
  ::baidu::common::ThreadPool pool_;
};

} //namespace dos
#endif
