#ifndef KERNEL_MASTER_IMPL_H
#define KERNEL_MASTER_IMPL_H

#include "master/node_manager.h"
#include "master/pod_manager.h"
#include "proto/master.pb.h"
#include "common/blocking_queue.h"
#include "common/ins_mutex.h"
#include "master/job_manager.h"
#include "master/master_internal_types.h"
#include "ins_sdk.h"
#include "thread_pool.h"
#include "mutex.h"

using ::galaxy::ins::sdk::InsSDK;
using ::google::protobuf::RpcController;
using ::google::protobuf::Closure;

namespace dos {

class MasterImpl : public Master {

public:
  MasterImpl();
  ~MasterImpl();

  void Start();
  void SubmitJob(RpcController* controller,
                 const SubmitJobRequest* request,
                 SubmitJobResponse* response,
                 Closure* done);

  void HeartBeat(RpcController* controller,
                 const HeartBeatRequest* request,
                 HeartBeatResponse* response,
                 Closure* done);

  void GetScaleUpPod(RpcController* controller,
                    const GetScaleUpPodRequest* request,
                    GetScaleUpPodResponse* response,
                    Closure* done);

  void SyncAgentInfo(RpcController* controller,
                     const SyncAgentInfoRequest* request,
                     SyncAgentInfoResponse* response,
                     Closure* done);

  void ScaleUpPropose(RpcController* controller,
                     const ScaleUpProposeRequest* request,
                     ScaleUpProposeResponse* response,
                     Closure* done);

  void GetJob(RpcController* controller,
               const GetJobRequest* request,
               GetJobResponse* response,
               Closure* done);

  void KillJob(RpcController* controller,
               const KillJobRequest* request,
               KillJobResponse* response,
               Closure* done);
private:
  void SchedNextGc();
private:
  NodeManager* node_manager_;
  JobManager* job_manager_;
  PodManager* pod_manager_;
  FixedBlockingQueue<NodeStatus*>* node_opqueue_;
  FixedBlockingQueue<PodOperation*>* pod_opqueue_;
  FixedBlockingQueue<JobOperation*>* job_opqueue_;
  InsMutex* master_lock_;
  InsSDK* ins_;
  ::baidu::common::Mutex gc_mutex_;
  std::set<std::string> job_to_gc_;
  ::baidu::common::ThreadPool* gc_pool_;
};

}
#endif
