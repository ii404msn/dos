#include "engine/engine_impl.h"

#include <unistd.h>
#include <sched.h>
#include <time.h>
#include <sys/wait.h>
#include <boost/algorithm/string/join.hpp>
#include <boost/lexical_cast.hpp>
#include <gflags/gflags.h>
#include "engine/oci_loader.h"
#include "engine/utils.h"
#include "timer.h"

const static int CLONE_FLAGS = CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNS;

DECLARE_string(ce_bin_path);
DECLARE_string(ce_image_fetcher_name);
DECLARE_string(ce_process_default_user);
DECLARE_int32(ce_image_fetch_status_check_interval);
DECLARE_int32(ce_initd_boot_check_max_times);
DECLARE_int32(ce_initd_boot_check_interval);
DECLARE_int32(ce_process_status_check_interval);
DECLARE_int32(ce_container_log_max_size);

namespace dos {

EngineImpl::EngineImpl(const std::string& work_dir,
                       const std::string& gc_dir):mutex_(),
  containers_(NULL),
  thread_pool_(NULL),
  work_dir_(work_dir),
  gc_dir_(gc_dir),
  fsm_(NULL),
  rpc_client_(NULL),
  ports_(NULL),
  user_mgr_(NULL){
  containers_ = new Containers();
  thread_pool_ = new ::baidu::common::ThreadPool(20);
  fsm_ = new FSM();
  fsm_->insert(std::make_pair(kContainerPulling, boost::bind(&EngineImpl::HandlePullImage, this, _1, _2)));
  fsm_->insert(std::make_pair(kContainerBooting, boost::bind(&EngineImpl::HandleBootInitd, this, _1, _2)));
  fsm_->insert(std::make_pair(kContainerRunning, boost::bind(&EngineImpl::HandleRunContainer, this, _1, _2)));
  fsm_->insert(std::make_pair(kContainerError, boost::bind(&EngineImpl::HandleError, this, _1, _2)));
  fsm_->insert(std::make_pair(kContainerCompleted, boost::bind(&EngineImpl::HandleCompleteContainer, this, _1, _2)));
  rpc_client_ = new RpcClient();
  ports_ = new std::queue<int32_t>();
  for (int32_t i= 9000; i < 10000; i++) {
    ports_->push(i);
  }
  user_mgr_ = new UserMgr();
}

EngineImpl::~EngineImpl() {}

bool EngineImpl::Init() {
  std::string name = FLAGS_ce_image_fetcher_name;
  {
    ::baidu::common::MutexLock lock(&mutex_);
    LOG(INFO, "start system container %s", name.c_str());
    ContainerInfo* info = new ContainerInfo();
    info->status.mutable_spec()->set_type(kSystem);
    info->status.mutable_spec()->set_reserve_time(0);
    info->status.set_name(name);
    info->status.set_start_time(0);
    info->status.set_state(kContainerPending);
    containers_->insert(std::make_pair(name, info));
    thread_pool_->AddTask(boost::bind(&EngineImpl::StartContainerFSM, this, name));
    int ok = user_mgr_->SetUp();
    if (ok != 0) {
      return false;
    }
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
  info->status.set_name(request->name());
  info->status.set_start_time(0);
  info->status.set_boot_time(0);
  info->status.set_health_state(kUnCalculated);
  info->status.set_state(kContainerPending);
  info->status.mutable_spec()->CopyFrom(request->container());
  containers_->insert(std::make_pair(request->name(), info));
  response->set_status(kRpcOk);
  thread_pool_->AddTask(boost::bind(&EngineImpl::StartContainerFSM, this, request->name())); 
  done->Run();
}

void EngineImpl::ShowContainer(RpcController* controller,
                               const ShowContainerRequest* request,
                               ShowContainerResponse* response,
                               Closure* done) {
  ::baidu::common::MutexLock lock(&mutex_);
  std::set<std::string> names;
  for (int32_t index = 0; index < request->names_size(); ++index) {
    names.insert(request->names(index));
    LOG(DEBUG, "show container %s request", request->names(index).c_str());
  }
  Containers::iterator it = containers_->begin();
  for (; it != containers_->end(); ++it) {
    if (names.size() != 0 
        && names.find(it->second->status.name()) == names.end()) {
      LOG(DEBUG, "container %s is not in request ", it->second->status.name().c_str());
      continue;
    }
    ContainerOverview* container = response->add_containers();
    container->set_name(it->second->status.name());
    container->set_start_time(it->second->status.start_time());
    container->set_state(it->second->status.state());
    container->set_type(it->second->status.spec().type());
    container->set_boot_time(it->second->status.boot_time());
  }
  response->set_status(kRpcOk);
  done->Run();
}

void EngineImpl::ShowCLog(RpcController* controller,
                          const ShowCLogRequest* request,
                          ShowCLogResponse* response,
                          Closure* done) {

  ::baidu::common::MutexLock lock(&mutex_);
  Containers::iterator it = containers_->find(request->name());
  if (it == containers_->end()) {
    response->set_status(kRpcNotFound);
    done->Run();
    LOG(WARNING, "fail to find log with container %s", request->name().c_str());
    return;
  }
  std::deque<ContainerLog>::iterator clog_it = it->second->logs.begin();
  for (; clog_it != it->second->logs.end(); ++clog_it) {
    ContainerLog* log = response->add_logs();
    log->CopyFrom(*clog_it);
  }
  response->set_status(kRpcOk);
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
    info->start_pull_time = ::baidu::common::timer::get_micros();
    info->status.set_boot_time(0);
  }
  FSM::iterator fsm_it = fsm_->find(kContainerPulling);
  if (fsm_it == fsm_->end()) {
    LOG(WARNING, "container %s has no fsm config with state %s",
        name.c_str(), ContainerState_Name(state).c_str());
    return;
  }
  fsm_it->second(state, name);
}

void EngineImpl::AppendLog(const ContainerState& cfrom,
                           const ContainerState& cto,
                           const std::string& msg,
                           ContainerInfo* info) {
  mutex_.AssertHeld();
  if (info->logs.size() >= (size_t)FLAGS_ce_container_log_max_size) {
    info->logs.pop_front();
  }
  ContainerLog log;
  log.set_name(info->status.name());
  log.set_cfrom(cfrom);
  log.set_cto(cto);
  log.set_time(::baidu::common::timer::get_micros());
  log.set_msg(msg);
  info->logs.push_back(log);
}

void EngineImpl::HandlePullImage(const ContainerState& pre_state,
                                 const std::string& name) {
  ::baidu::common::MutexLock lock(&mutex_);
  Containers::iterator it = containers_->find(name);
  if (it == containers_->end()) {
    // end of container fsm
    LOG(INFO, "container with name %s has been deleted", name.c_str());
    return;
  }
  ContainerInfo* info = it->second;
  ContainerState target_state = kContainerPulling;
  int32_t exec_task_interval = 0;
  if (pre_state == kContainerPending) {
    do { 
      info->status.set_state(kContainerPulling);
      info->work_dir = work_dir_ + "/" + name;
      if (!Mkdir(info->work_dir)) {
        LOG(WARNING, "fail to create work dir %s for container %s ",
            name.c_str(), info->work_dir.c_str());
        target_state = kContainerError;
        exec_task_interval = FLAGS_ce_process_status_check_interval;
        AppendLog(pre_state, kContainerPulling, "fail to create work dir", info);
        break;
      }
      if (info->status.spec().type() == kSystem) {
        // no need pull image when type is kSystem
        // eg image fetcher helper, monitor
        target_state = kContainerBooting;
        exec_task_interval = 0;
        AppendLog(kContainerPulling, kContainerBooting, "pull image ok", info);
        break;
      } else if (info->status.spec().type() == kOci) {
        // fetch oci rootfs by wget
        LOG(INFO, "start to pull image for container %s from uri %s", 
            name.c_str(), info->status.spec().uri().c_str());
        it = containers_->find(FLAGS_ce_image_fetcher_name);
        ContainerInfo* fetcher = it->second;
        if (fetcher->status.state() != kContainerRunning) {
          LOG(WARNING, "fetcher is in invalidate state %s", 
              ContainerState(fetcher->status.state()));
          AppendLog(kContainerPulling, kContainerError,
              "fetcher is no avilable", info);
          target_state = kContainerError;
          exec_task_interval = 0;
          break;
        }
        if (fetcher->initd_stub == NULL) {
          rpc_client_->GetStub(fetcher->initd_endpoint, &fetcher->initd_stub);
        }
        info->fetcher_name = "fetcher_for_" + name;
        //TODO optimalize fetch cmd builder 
        // add limit and retry
        std::string cmd = "cd " + info->work_dir;
        cmd += " && wget -O rootfs.tar.gz " + info->status.spec().uri();
        cmd += " && tar -zxvf rootfs.tar.gz && cp " + FLAGS_ce_bin_path + " ./rootfs/bin/dsh";
        ForkRequest request;
        ForkResponse response;
        Process fetch_process;
        request.mutable_process()->add_args("bash");
        request.mutable_process()->add_args("-c");
        request.mutable_process()->add_args(cmd);
        request.mutable_process()->set_name(info->fetcher_name);
        request.mutable_process()->set_terminal(false);
        request.mutable_process()->set_interceptor("/bin/bash");
        // user root for fetcher
        request.mutable_process()->mutable_user()->set_name("root");
        bool process_user_ok = HandleProcessUser(request.mutable_process());
        if (!process_user_ok) {
          LOG(WARNING, "fail to process user %s", request.process().user().name().c_str());
          target_state = kContainerError;
          exec_task_interval = 0;
          AppendLog(kContainerPulling, kContainerError, "fail to process user ",
              info);
          break;
        }
        bool rpc_ok = rpc_client_->SendRequest(fetcher->initd_stub, 
                                               &Initd_Stub::Fork,
                                               &request, &response, 5, 1);
        if (!rpc_ok || response.status() != kRpcOk) {
          LOG(WARNING, "fail to send fetch cmd %s for container %s",
              cmd.c_str(), name.c_str());
          target_state = kContainerError;
          exec_task_interval = 0;
          AppendLog(kContainerPulling, kContainerError, "fail to send fetch cmd to initd",
              info);
          break;
        } else {
          LOG(INFO, "send fetch cmd %s for container %s successfully",
              cmd.c_str(), name.c_str());
          target_state = kContainerPulling;
          exec_task_interval = FLAGS_ce_image_fetch_status_check_interval;
          AppendLog(kContainerPulling, kContainerPulling, "send fetch cmd to inid successfully",
              info);
        }
      }
    } while(0);
  } else if (pre_state == kContainerPulling) {
    do {
      info->status.set_state(kContainerPulling);
      if (info->status.spec().type() == kOci) {
        it = containers_->find(FLAGS_ce_image_fetcher_name);
        ContainerInfo* fetcher = it->second;
        if (fetcher->status.state() != kContainerRunning) {
          LOG(WARNING, "fetcher is in invalidate state %s", 
              ContainerState(fetcher->status.state()));
          target_state = kContainerError;
          exec_task_interval = 0;
          AppendLog(kContainerPulling, kContainerError, "fetcher is in validate state",
              info);
          break;
        }
        if (fetcher->initd_stub == NULL) {
          rpc_client_->GetStub(fetcher->initd_endpoint, &fetcher->initd_stub);
        }
        WaitRequest request;
        request.add_names(info->fetcher_name);
        WaitResponse response;
        bool rpc_ok = rpc_client_->SendRequest(fetcher->initd_stub, 
                                               &Initd_Stub::Wait,
                                               &request, &response, 5, 1);
        if (!rpc_ok || response.status() != kRpcOk) {
          LOG(WARNING, "fail to wait fetch status for container %s", name.c_str());
          target_state = kContainerError;
          exec_task_interval = 0;
          AppendLog(kContainerPulling, kContainerError, "fail to wait fetch status", info);
          break;
        } else {
          const Process& status = response.processes(0);
          if (status.running()) {
            target_state = kContainerPulling;
            LOG(DEBUG, "container %s is under fetching rootfs", name.c_str());
            exec_task_interval = FLAGS_ce_image_fetch_status_check_interval;
          }else if (status.exit_code() == 0) {
            LOG(INFO, "fetch container %s rootfs successfully", name.c_str());
            target_state = kContainerBooting;
            AppendLog(kContainerPulling, kContainerBooting, "pull image ok", info);
            exec_task_interval = 0;
            // clean fetcher process 
            CleanProcessInInitd(info->fetcher_name, fetcher);
          } else {
            LOG(WARNING, "fail to fetch container %s rootfs", name.c_str());
            target_state = kContainerError;
            exec_task_interval = 0;
            AppendLog(kContainerPulling, kContainerError, "fail to fetch container rootfs", info);
            // clean fetcher process
            CleanProcessInInitd(info->fetcher_name, fetcher);
          }
        }

      }
    } while(0); 
  }
  ProcessHandleResult(target_state, kContainerPulling, name, exec_task_interval);
}

void EngineImpl::ProcessHandleResult(const ContainerState& target_state,
                                     const ContainerState& current_state,
                                     const std::string& name,
                                     int32_t exec_task_interval) {
  FSM::iterator fsm_it = fsm_->find(target_state);
  if (fsm_it == fsm_->end()) {
    LOG(WARNING, "container %s has no fsm config with state %s",
          name.c_str(), ContainerState_Name(target_state).c_str());
    return;
  }

  if (exec_task_interval <= 0) {
    thread_pool_->AddTask(boost::bind(fsm_it->second, current_state, name));
  } else {
    thread_pool_->DelayTask(exec_task_interval, 
        boost::bind(fsm_it->second, current_state, name));
  }
}

void EngineImpl::HandleBootInitd(const ContainerState& pre_state,
                                 const std::string& name) {
  ::baidu::common::MutexLock lock(&mutex_);
  ContainerState target_state = kContainerPulling;
  int32_t exec_task_interval = 0;
  Containers::iterator it = containers_->find(name);
  if (it == containers_->end()) {
    LOG(INFO, "container with name %s has been deleted", name.c_str());
    return;
  }
  ContainerInfo* info = it->second;
  info->status.set_state(kContainerBooting);
  do {
    // boot initd
    if (pre_state == kContainerPulling) {
      int32_t port = ports_->front();
      ports_->pop();
      LOG(INFO, "boot container %s with type %s initd in work dir %s with port %d", name.c_str(),
          ContainerType_Name(info->status.spec().type()).c_str(),
          info->work_dir.c_str(), port);
      info->initd_endpoint = "127.0.0.1:" + boost::lexical_cast<std::string>(port);
      Process initd;
      initd.set_cwd(info->work_dir);
      initd.set_interceptor(FLAGS_ce_bin_path);
      initd.add_args("initd");
      if (info->status.spec().type() == kSystem) {
        initd.add_args("--ce_enable_ns=false");
      } else {
        initd.add_args("--ce_initd_conf_path=./runtime.json");
        initd.set_hostname(name);
      }
      initd.add_args("--ce_initd_port=" + boost::lexical_cast<std::string>(port));
      initd.mutable_user()->set_name("root");
      initd.set_terminal(false);
      // container name used as hostname
      initd.set_name(name);
      bool process_user_ok = HandleProcessUser(&initd);
      if (!process_user_ok) {
        LOG(WARNING, "fail to process user %s", initd.user().name().c_str());
        target_state = kContainerError;
        exec_task_interval = 0;
        AppendLog(kContainerBooting, kContainerError, "fail to process user ",
            info);
        break;
      }
      //TODO read from runtime.json 
      int flag = CLONE_FLAGS;
      if (info->status.spec().type() == kSystem) {
        flag = CLONE_NEWUTS;
      }
      bool ok = info->initd_proc.Clone(initd, flag);
      if (!ok) {
        LOG(WARNING, "fail to clone initd from container %s for %s",
            name.c_str(), strerror(errno));
        info->status.set_state(kContainerError);
        target_state = kContainerError;
        exec_task_interval = 0;
        AppendLog(kContainerBooting, kContainerError, "fail to clone initd", info);
        break;
      } else {
        info->status.set_state(kContainerBooting);
        target_state = kContainerBooting;
        exec_task_interval = FLAGS_ce_initd_boot_check_interval;
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
          target_state = kContainerError;
          exec_task_interval = 0;
          AppendLog(kContainerBooting, kContainerError, "initd booting fails", info);
          break;
        }else {
          target_state = kContainerBooting;
          exec_task_interval = FLAGS_ce_initd_boot_check_interval;
        }
      } else {
        // initd boots successfully 
        info->status.set_boot_time(::baidu::common::timer::get_micros() - info->start_pull_time);
        AppendLog(kContainerBooting, kContainerRunning, "start initd ok", info);
        target_state = kContainerRunning;
        exec_task_interval = 0;
      }
    } else {
      LOG(WARNING, "invalidate pre state for container %s", name.c_str());
    }
  } while(0);
  ProcessHandleResult(target_state, kContainerBooting, name, exec_task_interval);
}

void EngineImpl::HandleError(const ContainerState& pre_state,
                             const std::string& name) {
  ::baidu::common::MutexLock lock(&mutex_);
  Containers::iterator it = containers_->find(name);
  if (it == containers_->end()) {
    LOG(INFO, "container with name %s has been deleted", name.c_str());
    return;
  }
  ContainerInfo* info = it->second;
  info->status.set_state(kContainerError);
  info->status.set_start_time(0);
  LOG(WARNING, "container %s go to %s state", name.c_str(), ContainerState_Name(info->status.state()).c_str());
  if (info->status.spec().reserve_time() > 0 && pre_state != kContainerReserving) {
    info->status.set_state(kContainerReserving);
    AppendLog(kContainerError, kContainerReserving, "enter reserving state", info);
    ProcessHandleResult(kContainerError, kContainerReserving,
                        name, info->status.spec().reserve_time());
  } else {
    LOG(INFO, "container %s enter error", name.c_str());
  }
}

void EngineImpl::HandleCompleteContainer(const ContainerState& pre_state,
                                         const std::string& name) {
  ::baidu::common::MutexLock lock(&mutex_);
  Containers::iterator it = containers_->find(name);
  if (it == containers_->end()) {
    LOG(INFO, "container with name %s has been deleted", name.c_str());
    return;
  }
  ContainerInfo* info = it->second;
  info->status.set_start_time(0);
  info->status.set_state(kContainerCompleted);
  LOG(WARNING, "container %s go to %s state", name.c_str(), ContainerState_Name(info->status.state()).c_str());
}

void EngineImpl::CleanProcessInInitd(const std::string& name, ContainerInfo* info) {
  mutex_.AssertHeld();
  KillRequest request;
  request.add_names(name);
  KillResponse response;
  bool ok = rpc_client_->SendRequest(info->initd_stub, 
                                     &Initd_Stub::Kill,
                                     &request, 
                                     &response, 
                                     5,1);
  if (!ok || response.status() != kRpcOk) {
    LOG(WARNING, "fail to clean process %s for %s", 
        name.c_str(),
        RpcStatus_Name(response.status()).c_str());
  } else {
    LOG(INFO, "clean process %s successfully", name.c_str());
  }
}


bool EngineImpl::DoStartProcess(const std::string& name,
                                ContainerInfo* info) {
  mutex_.AssertHeld();
  std::string config_path = info->work_dir + "/config.json";
  dos::Config config;
  bool load_ok = dos::LoadConfig(config_path, &config);
  if (!load_ok) {
    LOG(WARNING, "fail to load config.json");
    info->status.set_state(kContainerError);
    AppendLog(info->status.state(), kContainerError, "fail to load config.json", info);
    return false;
  }
  ForkRequest request;
  config.process.set_name(name);
  request.mutable_process()->CopyFrom(config.process);
  bool process_user_ok = HandleProcessUser(request.mutable_process());
  if (!process_user_ok) {
    LOG(WARNING, "fail to process user %s", request.process().user().name().c_str());
    AppendLog(info->status.state(), kContainerError, "fail to process user ", info);
    return false;
  }
  ForkResponse response;
  bool rpc_ok = rpc_client_->SendRequest(info->initd_stub, 
                         &Initd_Stub::Fork,
                         &request, &response, 5, 1);
  rpc_ok = rpc_ok && response.status() == kRpcOk;
  if (!rpc_ok) {
    LOG(WARNING, "fail send request to initd %s for container %s",
        info->initd_endpoint.c_str(),
        name.c_str());
    AppendLog(info->status.state(), kContainerError, "fail to process user ", info);
  }
  return rpc_ok;
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
  ContainerState target_state = kContainerRunning;
  int32_t exec_task_interval = 0;
  do {
    // run command in container 
    if (pre_state == kContainerBooting) {
      LOG(INFO, "start container %s in work dir %s", name.c_str(),
          info->work_dir.c_str());
      if (info->initd_stub == NULL) {
        rpc_client_->GetStub(info->initd_endpoint, &info->initd_stub);
      }
      bool rpc_ok = false;
      // handle reserved container which only has initd
      if (info->status.spec().reserve_time() <= 0) {
        StatusRequest request;
        StatusResponse response;
        rpc_ok = rpc_client_->SendRequest(info->initd_stub, 
                                          &Initd_Stub::Status,
                                          &request, &response, 5, 1);
        rpc_ok = rpc_ok && response.status() == kRpcOk;
      } else {
        rpc_ok = DoStartProcess(name, info);
      }
      if (!rpc_ok) {
        LOG(WARNING, "fail to fork process for container %s", name.c_str());
        info->status.set_state(kContainerError);
        target_state = kContainerError;
        exec_task_interval = 0;
        AppendLog(kContainerRunning, kContainerError, "fail to fork process", info);
        break;
      } else {
        info->status.set_start_time(::baidu::common::timer::get_micros());
        LOG(INFO, "fork process for container %s successfully", name.c_str());
        info->status.set_state(kContainerRunning);
        AppendLog(kContainerRunning, kContainerRunning, "start user process ok", info);
        exec_task_interval = FLAGS_ce_process_status_check_interval;
        target_state = kContainerRunning;
      }
    } else if (pre_state == kContainerRunning) {
      // forever reserve initd
      if (info->status.spec().reserve_time() <=0) {
        StatusRequest request;
        StatusResponse response;
        bool rpc_ok = rpc_client_->SendRequest(info->initd_stub, 
                                              &Initd_Stub::Status,
                                              &request, &response, 5, 1);
        rpc_ok = rpc_ok && response.status() == kRpcOk;
        if (rpc_ok) {
          target_state = kContainerRunning;
          LOG(DEBUG, "container %s is under running", name.c_str());
          exec_task_interval = FLAGS_ce_process_status_check_interval;
          info->status.set_state(kContainerRunning);
        } else {
          target_state = kContainerError;
          exec_task_interval = 0;
          info->status.set_state(kContainerError);
          AppendLog(kContainerRunning, kContainerError, "fail to check process", info);
        }
        break;
      }
      // check container process status
      WaitRequest request;
      request.add_names(name);
      std::set<std::string>::iterator name_it = info->batch_process.begin();
      for (; name_it != info->batch_process.end(); ++name_it) {
        request.add_names(*name_it);
      }
      WaitResponse response;
      if (info->initd_stub == NULL) {
        rpc_client_->GetStub(info->initd_endpoint, &info->initd_stub);
      }
      bool rpc_ok = rpc_client_->SendRequest(info->initd_stub, 
                                             &Initd_Stub::Wait,
                                             &request, &response, 5, 1);
      if (!rpc_ok || response.status() != kRpcOk) {
        LOG(WARNING, "fail to connect to initd %s", info->initd_endpoint.c_str());
        // TODO add check times that fails reach
        target_state = kContainerError;
        exec_task_interval = 0;
        AppendLog(kContainerRunning, kContainerError, "fail to connect to initd", info);
        break;
      } else {
        for (int32_t p_index = 0; p_index < response.processes_size(); ++p_index) {
          const Process& status = response.processes(p_index);
          if (status.name() == name) {
            if (status.running()) {
              target_state = kContainerRunning;
              LOG(DEBUG, "container %s is under running", name.c_str());
              exec_task_interval = FLAGS_ce_process_status_check_interval;
              info->status.set_state(kContainerRunning);
            }else if (status.exit_code() == 0) {
              LOG(INFO, "container %s exit with 0", name.c_str());
              target_state = kContainerCompleted;
              AppendLog(kContainerRunning, kContainerCompleted, "container completed", info);
              exec_task_interval = 0;
              info->status.set_state(kContainerCompleted);
            } else {
              LOG(WARNING, "fail to check container %s status", name.c_str());
              target_state = kContainerError;
              exec_task_interval = 0;
              info->status.set_state(kContainerError);
              AppendLog(kContainerRunning, kContainerError, "fail to check container status", info);
              if (info->status.spec().restart_strategy() == kAlways) {
                LOG(DEBUG, "process always restart process for container %s", name.c_str());
                CleanProcessInInitd(status.name(), info);
                bool restart_ok = DoStartProcess(status.name(), info);
                target_state = kContainerRunning;
                exec_task_interval = FLAGS_ce_process_status_check_interval;
                info->status.set_restart_count(info->status.restart_count() + 1);
                if (restart_ok) {
                  LOG(INFO, "restart container %s successfully", status.name().c_str());
                } else {
                  LOG(WARNING, "fail to restart container %s", status.name().c_str());
                }
              } else  {
                LOG(DEBUG, "container restart strategy %s", RestartStrategy_Name(info->status.spec().restart_strategy()).c_str());
              }
            }
          } else {
            if (!status.running()) {
              LOG(DEBUG, "clean process %s in pod %s", status.name().c_str(), name.c_str());
              CleanProcessInInitd(status.name(), info);
            }
          }
        }
      }
    }
  } while(0);
  ProcessHandleResult(target_state, kContainerRunning, name, exec_task_interval);
}

bool EngineImpl::HandleProcessUser(Process* process) {
  mutex_.AssertHeld();
  if (process->user().name().empty()) {
    process->mutable_user()->set_name(FLAGS_ce_process_default_user);
  }
  int add_ok = user_mgr_->AddUser(process->user());
  if (add_ok != 0) {
    LOG(WARNING, "fail to add user %s ", process->user().name().c_str());
    return false;
  }
  int get_ok = user_mgr_->GetUser(process->user().name(), 
                                  process->mutable_user());
  if (get_ok != 0) {
    LOG(WARNING, "fail to get user %s", process->user().name().c_str());
    return false;
  }
  return true;
}

void EngineImpl::GetInitd(RpcController* controller,
                          const GetInitdRequest* request,
                          GetInitdResponse* response,
                          Closure* done) {
  ::baidu::common::MutexLock lock(&mutex_);
  LOG(DEBUG, "get initd of container %s", request->name().c_str());
  Containers::iterator it = containers_->find(request->c_name());
  if (it == containers_->end()) {
    LOG(INFO, "container with name %s has been deleted", request->c_name().c_str());
    response->set_status(kRpcNotFound);
    done->Run();
    return;
  }
  ContainerInfo* info = it->second;
}


std::string EngineImpl::CurrentDatetimeStr() {
  int64_t now = ::baidu::common::timer::get_micros();
  char buffer[100];
  time_t time = now / 1000000;
  struct tm *tmp;
  tmp = localtime(&time);
  strftime(buffer, 100, "%F %X", tmp);
  std::string ret(buffer);
  return ret;
}

} // namespace dos
