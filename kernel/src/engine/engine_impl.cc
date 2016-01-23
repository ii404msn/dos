
#include "engine/engine_impl.h"

#include <boost/algorithm/string/join.hpp>
#include <sched.h>
#include <sys/wait.h>
#include <gflags/gflags.h>
#include "engine/oci_loader.h"

#ifndef CLONE_NEWPID
#define CLONE_NEWPID 0x02000000
#endif

#ifndef CLONE_NEWUTS
#define CLONE_NEWUTS 0x04000000
#endif

#ifndef CLONE_NEWNS
#define CLONE_NEWNS 0x00020000
#endif


DECLARE_string(ce_bin_path);
DECLARE_int32(ce_initd_boot_check_max_times);
DECLARE_int32(ce_initd_boot_check_interval);
DECLARE_int32(ce_process_status_check_interval);

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
  thread_pool_ = new ::baidu::common::ThreadPool(20);
  fsm_ = new FSM();
  fsm_->insert(std::make_pair(kContainerPulling, boost::bind(&EngineImpl::HandlePullImage, this, _1, _2)));
  fsm_->insert(std::make_pair(kContainerBooting, boost::bind(&EngineImpl::HandleBootInitd, this, _1, _2)));
  fsm_->insert(std::make_pair(kContainerRunning, boost::bind(&EngineImpl::HandleRunContainer, this, _1, _2)));
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
    response->set_status(kRpcNameExist);
    done->Run();
    return;
  }
  std::string rootfs_path; 
  //TODO container validate
  ContainerInfo* info = new ContainerInfo();
  info->container = request->container();
  info->status.set_name(request->name());
  info->status.set_start_time(0);
  info->status.set_state(kContainerPending);
  containers_->insert(std::make_pair(request->name(), info));
  response->set_status(kRpcOk);
  thread_pool_->AddTask(boost::bind(&EngineImpl::StartContainerFSM, this, request->name())); 
  done->Run();
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
  FSM::iterator fsm_it = fsm_->find(kContainerPulling);
  if (fsm_it == fsm_->end()) {
    LOG(WARNING, "container %s has no fsm config with state %s",
        name.c_str(), ContainerState_Name(state).c_str());
    return;
  }
  fsm_it->second(state, name);
}

void EngineImpl::HandlePullImage(const ContainerState& pre_state,
                                 const std::string& name) {
  {
    ::baidu::common::MutexLock lock(&mutex_);
    Containers::iterator it = containers_->find(name);
    if (it == containers_->end()) {
      LOG(INFO, "container with name %s has been deleted", name.c_str());
      return;
    }
    LOG(INFO, "start to pull image for container %s", name.c_str());
    ContainerInfo* info = it->second;
    info->status.set_state(kContainerPulling);
    info->work_dir="/home/galaxy/dos/sandbox/dfs";
  } 
  FSM::iterator fsm_it = fsm_->find(kContainerBooting);
  if (fsm_it == fsm_->end()) {
    LOG(WARNING, "container %s has no fsm config with state %s",
          name.c_str(), ContainerState_Name(kContainerPulling).c_str());
    return;
  }
  fsm_it->second(kContainerPulling, name);
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
    info->initd_endpoint = "127.0.0.1:9527";
    Process initd;
    initd.set_cwd(info->work_dir);
    initd.add_args(FLAGS_ce_bin_path);
    initd.add_args("initd");
    initd.add_args("--ce_initd_conf_path=./runtime.json");
    initd.add_args("--ce_initd_port=9527");
    initd.mutable_user()->set_name("root");
    initd.set_terminal(false);
    //TODO read from runtime.json
    int flag = CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWUTS | SIGCHLD;
    bool ok = info->initd_proc.Clone(initd, flag);
    if (!ok) {
      LOG(WARNING, "fail to clone initd from container %s for %s",
          name.c_str(), strerror(errno));
      info->status.set_state(kContainerError);
    } else {
      info->status.set_state(kContainerBooting);
      thread_pool_->DelayTask(FLAGS_ce_initd_boot_check_interval,
                              boost::bind(&EngineImpl::HandleBootInitd,
                              this, kContainerBooting, name));
    }

  } else if (pre_state == kContainerBooting) {
    // check initd is ok
    LOG(INFO, "checkout initd with endpoint %s", info->initd_endpoint.c_str());
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
                              this, kContainerBooting, name));
      }
    } else {
      FSM::iterator fsm_it = fsm_->find(kContainerRunning);
      if (fsm_it == fsm_->end()) {
        LOG(WARNING, "container %s has no fsm config with state %s",
          name.c_str(), ContainerState_Name(kContainerRunning).c_str());
        return;
      }
      thread_pool_->AddTask(boost::bind(fsm_it->second, kContainerBooting, name)); 
    }
  } else {
    LOG(WARNING, "invalidate pre state");
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
  // run command in container 
  if (pre_state == kContainerBooting) {
    LOG(INFO, "start container %s in work dir %s", name.c_str(),
        info->work_dir.c_str());
    std::string config_path = info->work_dir + "/config.json";
    dos::Config config;
    bool ok = dos::LoadConfig(config_path, &config);
    ForkRequest request;
    config.process.set_name(name);
    config.process.mutable_user()->set_name("root");
    request.mutable_process()->CopyFrom(config.process);
    ForkResponse response;
    if (info->initd_stub == NULL) {
      rpc_client_->GetStub(info->initd_endpoint, &info->initd_stub);
    }
    ok = rpc_client_->SendRequest(info->initd_stub, 
                             &Initd_Stub::Fork,
                             &request, &response, 5, 1);
    if (!ok || response.status() != kRpcOk) {
      LOG(WARNING, "fail to fork process for container %s", name.c_str());
      info->status.set_state(kContainerError);
    } else {
      LOG(INFO, "fork process for container %s successfully", name.c_str());
      info->status.set_state(kContainerRunning);
      FSM::iterator fsm_it = fsm_->find(kContainerRunning);
      if (fsm_it == fsm_->end()) {
        LOG(WARNING, "container %s has no fsm config with state %s",
          name.c_str(), ContainerState_Name(kContainerRunning).c_str());
        return;
      }
      thread_pool_->DelayTask(FLAGS_ce_process_status_check_interval,
                              boost::bind(fsm_it->second, kContainerRunning, name));
    }
  }
}

int EngineImpl::LunachInitd(void* config) {
   return 0;
}

} // namespace dos
