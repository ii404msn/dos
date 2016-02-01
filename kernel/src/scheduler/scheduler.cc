#include "scheduler/scheduler.h"

#include "logging.h"

using ::baidu::common::INFO;
using ::baidu::common::WARNING;

namespace dos {

Scheduler::Scheduler(const std::string& master_addr):rpc_client_(NULL),
  master_(NULL), master_addr_(master_addr){
  rpc_client_ = new RpcClient();
}

Scheduler::~Scheduler() {}

bool Scheduler::Start() {
  bool get_ok = rpc_client_->GetStub(master_addr_, &master_);
  if (!get_ok) {
    return false;
  }
  pool_.DelayTask(1000, boost::bind(&Scheduler::GetScaleUpPods, this));
  return true;
}

void Scheduler::GetScaleUpPods() {
  LOG(INFO, "get scale up pods from %s", master_addr_.c_str());
  GetScaleUpPodRequest request;
  GetScaleUpPodResponse response;
  bool ok = rpc_client_->SendRequest(master_, &Master_Stub::GetScaleUpPod,
                           &request, &response, 5, 1);
  if (!ok || response.status() != kRpcOk) {
    LOG(WARNING, "fail to get scale up pods");
    pool_.DelayTask(1000, boost::bind(&Scheduler::GetScaleUpPods, this));
  } else {
    for (int i = 0; i < response.pods_size(); i++) {
      const PodOverview& pod = response.pods(i);
      LOG(INFO, "get scale up pod %s", pod.name().c_str());
    }
  }
  pool_.DelayTask(1000, boost::bind(&Scheduler::GetScaleUpPods, this));
}

} // namespace dos
