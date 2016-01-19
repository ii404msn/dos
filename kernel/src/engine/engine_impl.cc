#include "engine/engine_impl.h"

namespace dos {

EngineImpl::EngineImpl(const std::string& work_dir,
                       const std::string& gc_dir):mutex_(),
  containers_(NULL),
  thread_pool_(NULL),
  work_dir_(work_dir),
  gc_dir_(gc_dir) {
  containers_ = new Containers();
  thread_pool_ = new ThreadPool(20);
  fsm_ = new FSM();
  fsm_->insert(kContainerPulling, boost::bind(&EngineImpl::HandlePullImage, this, _1, _2));
  fsm_->insert(kContainerRunning, boost::bind(&EngineImpl::HandleRunContainer, this, _1, _2));
}

EngineImpl::~EngineImpl() {}

void EngineImpl::RunContainer(RpcController* controller,
                              const RunContainerRequest* request,
                              RunContainerResponse* response,
                              Closure* done) {
  ::baidu::common::MutexLock lock(&mutex_);
  LOG(INFO, "run container %s", request->name().c_str());
  if (containers_->find(request->name()) != containers_->end()) {
    LOG(WARNING, "container with name %s does exist", request->name().c_str());
    response->set_status(kDosNameExist);
    done->Run();
    return;
  }
  std::string rootfs_path; 
  //TODO container validate
  ContainerInfo* info = new ContainerInfo();
  info->container = request->container();
  info->status.set_name(request->name());
  info->status.start_time(0);
  info->status.state = kContainerPending;
  containers_->insert(std::make_pair(request->name(), info));
  response->set_status(kDosOk);
  done->Run();
  thread_pool_->AddTask(boost::bind(&EngineImpl::StartContainerFSM, this, request->name()));
}

void EngineImpl::StartContainerFSM(const std::string& name) {
  ContainerState state;
  {
    ::baidu::common::MutexLock lock(&mutex_);
    Containers::iterator it = containers_->find(name);
    if (it == containers_->end()) {
      LOG(INFO, "stop container %s fsm", name.c_str());
      return;
    }
    LOG(INFO, "start fsm for container %s", name.c_str());
    ContainerInfo* info = it->second;
    state = info->status.state();
  }
  FMS::iterator fsm_it = fsm_->find(state);
  if (fsm_it == fsm_.end()) {
    LOG(WARNING, "container %s has no fsm config with state %s",
        name.c_str(), ContainerState_Name(state));
    return;
  }
  fsm_it->second(state, name);
}

void EngineImpl::HandlePullImage(const ContainerState& pre_state,
                                 const std::string& name) {

}

void EngineImpl::HandleRunContainer(const ContainerState& pre_state,
                                    const std::string& name) {
  // start a new container 
  if (pre_state == kContainerPulling) {
    ::baidu::common::MutexLock lock(&mutex_);
    Containers::iterator it = containers_->find(name);
    if (it == containers_->end()) {
      LOG(INFO, "container with name %s has been deleted", name.c_str());
      return;
    }
    ContainerInfo* info = it->second;
    LOG(INFO, "start container %s in work dir %s", name.c_str(),
        info->work_dir.c_str());

  }
}

} // namespace dos
