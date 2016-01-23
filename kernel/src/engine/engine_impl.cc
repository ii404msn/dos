#include "engine/engine_impl.h"

#include <unistd.h>
#include <sched.h>
#include <sys/wait.h>
#include <boost/algorithm/string/join.hpp>
#include <boost/lexical_cast.hpp>
#include <gflags/gflags.h>
#include "engine/oci_loader.h"
#include "engine/utils.h"

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
  rpc_client_(NULL),
  ports_(NULL){
  containers_ = new Containers();
  thread_pool_ = new ::baidu::common::ThreadPool(20);
  fsm_ = new FSM();
  fsm_->insert(std::make_pair(kContainerPulling, boost::bind(&EngineImpl::HandlePullImage, this, _1, _2)));
  fsm_->insert(std::make_pair(kContainerBooting, boost::bind(&EngineImpl::HandleBootInitd, this, _1, _2)));
  fsm_->insert(std::make_pair(kContainerRunning, boost::bind(&EngineImpl::HandleRunContainer, this, _1, _2)));
  rpc_client_ = new RpcClient();
  ports_ = new std::queue<int32_t>();
  for (int32_t i= 9000; i < 10000; i++) {
    ports_->push(i);
  }
}

EngineImpl::~EngineImpl() {}

bool EngineImpl::Init() {
  std::string name = "image_extractor";
  {
    ::baidu::common::MutexLock lock(&mutex_);
    LOG(INFO, "start system container %s", name.c_str());
    ContainerInfo* info = new ContainerInfo();
    info->container.set_type(kSystem);
    info->container.set_reserved(true);
    info->status.set_name(name);
    info->status.set_start_time(0);
    info->status.set_state(kContainerPending);
    containers_->insert(std::make_pair(name, info));
    thread_pool_->AddTask(boost::bind(&EngineImpl::StartContainerFSM, this, name)); 
  }
  while (true) {
    {
      ::baidu::common::MutexLock lock(&mutex_);
      Containers::iterator it = containers_->find(name);
      if (it->second->status.state() == kContainerRunning) {
        LOG(INFO, "start system container %s successfully", name.c_str());
        return true;
      }
    }
    LOG(WARNING, "wait to system container %s to be running ",
          name.c_str());
    sleep(2);
  }
  return false;
}

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
    info->work_dir= work_dir_ + "/" + name;
    if (!Mkdir(info->work_dir)) {
      LOG(WARNING, "fail to create work dir for container %s ", name.c_str());
      //TODO go to err state;
      return;
    }
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
    int32_t port = ports_->front();
    ports_->pop();
    LOG(INFO, "boot container %s initd in work dir %s with port %d", name.c_str(),
        info->work_dir.c_str(), port);
    info->initd_endpoint = "127.0.0.1:" + boost::lexical_cast<std::string>(port);
    Process initd;
    initd.set_cwd(info->work_dir);
    initd.add_args(FLAGS_ce_bin_path);
    initd.add_args("initd");
    initd.add_args("--ce_initd_conf_path=./runtime.json");
    if (info->container.type() == kSystem) {
      initd.add_args("--ce_enable_ns=false");
    }
    initd.add_args("--ce_initd_port=" + boost::lexical_cast<std::string>(port)); 
    initd.mutable_user()->set_name("root");
    initd.set_terminal(false);
    //TODO read from runtime.json
    
    int flag = CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWUTS | SIGCHLD;
    if (info->container.type() == kSystem) {
      flag = SIGCHLD;
    }
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
    LOG(INFO, "check initd status with endpoint %s", info->initd_endpoint.c_str());
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
    if (info->initd_stub == NULL) {
      rpc_client_->GetStub(info->initd_endpoint, &info->initd_stub);
    }
    bool rpc_ok = false;
    // handle reserved container which only has initd
    if (info->container.reserved()) {
      StatusRequest request;
      StatusResponse response;
      rpc_ok = rpc_client_->SendRequest(info->initd_stub, 
                                        &Initd_Stub::Status,
                                        &request, &response, 5, 1);
      rpc_ok = rpc_ok && response.status() == kRpcOk;

    } else {
      std::string config_path = info->work_dir + "/config.json";
      dos::Config config;
      bool load_ok = dos::LoadConfig(config_path, &config);
      if (!load_ok) {
        LOG(WARNING, "fail to load config.json");
        //TODO handle err state
        return;
      }
      ForkRequest request;
      config.process.set_name(name);
      // TODO handle process create user logic
      config.process.mutable_user()->set_name("root");
      request.mutable_process()->CopyFrom(config.process);
      ForkResponse response;
      rpc_ok = rpc_client_->SendRequest(info->initd_stub, 
                             &Initd_Stub::Fork,
                             &request, &response, 5, 1);
      rpc_ok = rpc_ok && response.status() == kRpcOk;
    }
    if (!rpc_ok) {
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
  } else {
    //TODO loop check process state
  }
}



} // namespace dos
