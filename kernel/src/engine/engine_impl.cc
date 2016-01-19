
#include "engine/engine_impl.h"

#include <sched.h>
#include <gflags/gflags.h>

#ifndef CLONE_NEWPID
#define CLONE_NEWPID 0x02000000
#endif

#ifndef CLONE_NEWUTS
#define CLONE_NEWUTS 0x04000000
#endif

#ifndef CLONE_NEWNS
#define CLONE_NEWNS 0x00020000
#endif

#define STACK_SIZE (1024 * 1024)

DECLARE_string(ce_bin_path);
DECLARE_int32(ce_initd_boot_check_max_times);
DECLARE_int32(ce_initd_boot_check_interval);

static char CONTAINER_STACK[STACK_SIZE];
namespace dos {

EngineImpl::EngineImpl(const std::string& work_dir,
                       const std::string& gc_dir):mutex_(),
  containers_(NULL),
  thread_pool_(NULL),
  work_dir_(work_dir),
  gc_dir_(gc_dir),
  fsm_(NULL),
  rpc_client_(NULL){
  containers_ = new Containers();
  thread_pool_ = new ThreadPool(20);
  fsm_ = new FSM();
  fsm_->insert(kContainerPulling, boost::bind(&EngineImpl::HandlePullImage, this, _1, _2));
  fsm_->insert(kContainerRunning, boost::bind(&EngineImpl::HandleRunContainer, this, _1, _2));
  rpc_client_ = new RpcClient();
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

void EngineImpl::HandleBootInitd(const ContainerState& pre_state,
                                 const std::string& name) {
  ::baidu::common::MutexLock lock(&mutex_);
  Containers::iterator it = containers_->find(name);
  if (it == containers_->end()) {
    LOG(INFO, "container with name %s has been deleted", name.c_str());
    return;
  }
  ContainerInfo* info = it->second;
  info->status.set_state(kContainerBooting);
  // boot initd
  if (pre_state == kContainerPulling) {
    LOG(INFO, "boot container %s initd in work dir %s", name.c_str(),
        info->work_dir.c_str());
    if (info->initd_config == NULL) {
      info->initd_config = new InitdConfig();
      info->initd_config->work_dir = info->work_dir;
      info->initd_config->port = "9527";
    }
    //TODO read from runtime.json
    int flag = CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWUTS;
    // clone initd 
    int ok = ::clone(&EngineImpl::LunachInitd,
                    CONTAINER_STACK + STACK_SIZE,
                    flag | SIGCHLD,
                    info->initd_config);
    if (ok != 0) {
      LOG(WARNING, "fail to clone initd from container %s for %s",
          name.c_str(), strerrno(errno));
      info->status.set_state(kContainerError);
    } else {
      info->status.set_state(kContainerBooting);
      thread_pool_->DelayTask(FLAGS_ce_initd_boot_check_interval,
                              boost::bind(&EngineImpl::HandleBootInitd,
                              this, kContainerBooting, name))
    }

  } else if (pre_state == kContainerBooting) {
    // check initd is ok
    if (info->initd_stub == NULL) {
      rpc_client_->GetStub(info->initd_endpoint, &info->initd_stub);
    }
    info->initd_status_check_times ++;
    StatusRequest request;
    StatusResponse response;
    bool ok = rpc_client_->SendRequest(info->initd_stub, &Initd_Stub::Status,
                             &request, &response, 5, 1);
    if (!ok) { 
      if (info->initd_status_check_times > FLAGS_ce_initd_boot_check_max_times) {
        LOG(WARNING, "init for container %s has reach max boot times", name.c_str());
        info->status.set_state(kContainerError);
      }else {
        thread_pool_->DelayTask(FLAGS_ce_initd_boot_check_interval,
                              boost::bind(&EngineImpl::HandleBootInitd,
                              this, kContainerBooting, name))
      }
    } else {
      FMS::iterator fsm_it = fsm_->find(kContainerRunning);
      if (fsm_it == fsm_.end()) {
        LOG(WARNING, "container %s has no fsm config with state %s",
          name.c_str(), ContainerState_Name(kContainerRunning));
        return;
      }
      fsm_it->second(kContainerBooting, name);
    }
  }
}

void EngineImpl::HandleRunContainer(const ContainerState& pre_state,
                                    const std::string& name) {
  ::baidu::common::MutexLock lock(&mutex_);
  Containers::iterator it = containers_->find(name);
  if (it == containers_->end()) {
    LOG(INFO, "container with name %s has been deleted", name.c_str());
    return;
  }
  ContainerInfo* info = it->second;
  // start a new container 
  // clone  initd 
  if (pre_state == kContainerBooting) {
    LOG(INFO, "start container %s in work dir %s", name.c_str(),
        info->work_dir.c_str());
    ForkRequest request;
    request.set_user("galaxy");
    ForkResponse response;
  }
}

} // namespace dos
