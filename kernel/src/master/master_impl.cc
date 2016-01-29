#include "master/master_impl.h"

namespace dos {

MasterImpl::MasterImpl():node_manager_(NULL),
  node_status_queue_(NULL){
  node_status_queue_ = new FixedBlockingQueue<NodeStatus*>(2 * 10240, "node_status"); 
  node_manager_ = new NodeManager(node_status_queue_);
}

MasterImpl::~MasterImpl() {
  delete node_manager_;
  delete node_status_queue_;
}

void MasterImpl::HeartBeat(RpcController* /*controller*/,
                           const HeartBeatRequest* request,
                           HeartBeatResponse* response,
                           Closure* done) {
  node_manager_->KeepAlive(request->hostname(), request->endpoint());
  done->Run();
}

void MasterImpl::SubmitJob(RpcController* controller,
                           const SubmitJobRequest* request,
                           SubmitJobResponse* response,
                           Closure* done) {
  done->Run();

}

} // end of dos
