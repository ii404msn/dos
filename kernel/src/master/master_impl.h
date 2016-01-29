#ifndef KERNEL_MASTER_IMPL_H
#define KERNEL_MASTER_IMPL_H
#include "proto/master.pb.h"

#include "master/blocking_queue.h"
#include "master/node_manager.h"
#include "master/job_manager.h"
#include "master/pod_manager.h"
#include "master/master_internal_types.h"

using ::google::protobuf::RpcController;
using ::google::protobuf::Closure;
namespace dos {

class MasterImpl : public Master {

public:
  MasterImpl();
  ~MasterImpl();

  void SubmitJob(RpcController* controller,
                 const SubmitJobRequest* request,
                 SubmitJobResponse* response,
                 Closure* done);

  void HeartBeat(RpcController* controller,
                 const HeartBeatRequest* request,
                 HeartBeatResponse* response,
                 Closure* done);

private:
  NodeManager* node_manager_;
  JobManager* job_manager_;
  PodManager* pod_manager_;
  FixedBlockingQueue<NodeStatus*>* node_status_queue_;
  FixedBlockingQueue<PodOperation*>* pod_opqueue_;
  FixedBlockingQueue<JobOperation*>* job_opqueue_;
};

}
#endif
