#include "master/master_impl.h"

namespace dos {

MasterImpl::MasterImpl():node_manager_(NULL),
  job_manager_(NULL),
  pod_manager_(NULL),
  node_status_queue_(NULL),
  pod_opqueue_(NULL),
  job_opqueue_(NULL){
  node_status_queue_ = new FixedBlockingQueue<NodeStatus*>(2 * 10240, "node statue queue");
  pod_opqueue_ = new FixedBlockingQueue<PodOperation*>(2 * 10240, "pod operation queue");
  job_opqueue_ = new FixedBlockingQueue<JobOperation*>(2 * 10240, "job operation queue");
  node_manager_ = new NodeManager(node_status_queue_, pod_opqueue_);
  pod_manager_ = new PodManager(pod_opqueue_, job_opqueue_);
  job_manager_ = new JobManager(job_opqueue_);
}

MasterImpl::~MasterImpl() {
  //TODO make a clean
}

void MasterImpl::Start() {
  pod_manager_->Start();
  node_manager_->Start();
}

void MasterImpl::GetScaleUpPod(RpcController* controller,
                               const GetScaleUpPodRequest* request,
                               GetScaleUpPodResponse* response,
                               Closure* done) {
  // get pods that need to be scheduled
  pod_manager_->GetScaleUpPods(response->mutable_pods());
  response->set_status(kRpcOk);
  done->Run();
}

void MasterImpl::HeartBeat(RpcController* /*controller*/,
                           const HeartBeatRequest* request,
                           HeartBeatResponse* response,
                           Closure* done) {
  node_manager_->KeepAlive(request->hostname(), request->endpoint());
  done->Run();
}

void MasterImpl::ScaleUpPropose(RpcController* controller,
                     const ScaleUpProposeRequest* request,
                     ScaleUpProposeResponse* response,
                     Closure* done) {
  std::vector<boost::tuple<std::string, std::string> > pods;
  for (int32_t index = 0; index < request->proposes_size(); ++index) {
    pods.push_back(boost::make_tuple(request->proposes(index).endpoint(),
                                     request->proposes(index).pod_name()));
  }
  response->set_status(kRpcOk);
  done->Run();
  pod_manager_->SchedPods(pods);
}

void MasterImpl::SyncAgentInfo(RpcController* controller,
                     const SyncAgentInfoRequest* request,
                     SyncAgentInfoResponse* response,
                     Closure* done) {
  node_manager_->SyncAgentInfo(request->versions(),
                               response->mutable_diff_mod(),
                               response->mutable_diff_del());
  response->set_status(kRpcOk);
  done->Run();
}

void MasterImpl::SubmitJob(RpcController* controller,
                           const SubmitJobRequest* request,
                           SubmitJobResponse* response,
                           Closure* done) {
  response->set_status(kRpcOk);
  bool add_ok = job_manager_->Add(request->user_name(), 
                                  request->job());
  if (!add_ok) {
    response->set_status(kRpcNameExist);
  }
  done->Run();
}

} // end of dos
