#include "master/master_impl.h"

namespace dos {

MasterImpl::MasterImpl():node_manager_(NULL),
  job_manager_(NULL),
  pod_manager_(NULL),
  node_opqueue_(NULL),
  pod_opqueue_(NULL),
  job_opqueue_(NULL){
  node_opqueue_ = new FixedBlockingQueue<NodeStatus*>(2 * 10240, "node statue queue");
  pod_opqueue_ = new FixedBlockingQueue<PodOperation*>(2 * 10240, "pod operation queue");
  job_opqueue_ = new FixedBlockingQueue<JobOperation*>(2 * 10240, "job operation queue");
  node_manager_ = new NodeManager(node_opqueue_, pod_opqueue_);
  pod_manager_ = new PodManager(pod_opqueue_, 
                                job_opqueue_,
                                node_opqueue_);
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
  pod_manager_->GetScaleUpPods(request->condition(),
                               response->mutable_pods());
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

void MasterImpl::GetJob(RpcController* controller,
                        const GetJobRequest* request,
                        GetJobResponse* response,
                        Closure* done) {
  response->set_status(kRpcOk);
  bool get_ok = job_manager_->GetJob(request->name(), response->mutable_job());
  if (!get_ok) {
    LOG(WARNING, "fail to get jobs");
    response->set_status(kRpcError);
    done->Run();
    return;
  }
  JobStat stat;
  bool ok = pod_manager_->GetJobStat(request->name(), &stat);
  if (!ok) {
    LOG(WARNING, "fail to get pod stat of %s", request->name().c_str());
    response->set_status(kRpcError);
    done->Run();
    return;
  }
  JobOverview* job = response->mutable_job();
  job->set_running(stat.running_);
  job->set_death(stat.death_);
  job->set_pending(stat.pending_);
  done->Run();
}

void MasterImpl::KillJob(RpcController* controller,
                         const KillJobRequest* request,
                         KillJobResponse* response,
                         Closure* done) {
  bool del_ok = job_manager_->KillJob(request->name());
  if (del_ok) {
    response->set_status(kRpcOk);
  } else {
    response->set_status(kRpcError);
  }
  done->Run();
}

} // end of dos
