#ifndef KERNEL_ENGINE_IMPL_H
#define KERNEL_ENGINE_IMPL_H

#include <map>
#include <deque>
#include "proto/engine.pb.h"
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include "mutex.h"
#include "thread_pool.h"
#include "engine/process_mgr.h"
#include "engine/user_mgr.h"
#include "rpc/rpc_client.h"
#include "proto/initd.pb.h"

using ::google::protobuf::RpcController;
using ::google::protobuf::Closure;

namespace dos {

struct InitdConfig {
  std::string work_dir;
  std::string port;
};

struct ContainerInfo {
  Container container;
  ContainerStatus status;
  // the work_dir layout 
  //  work_dir
  //      |
  //      \_ rootfs
  //      |_ config.json
  //      |_ runtime.json
  std::string work_dir;
  std::string gc_dir;
  std::string initd_endpoint;
  ProcessMgr initd_proc; 
  InitdConfig* initd_config;
  Initd_Stub* initd_stub;
  int32_t initd_status_check_times;
  std::string fetcher_name;
  std::deque<ContainerLog> logs;
  int64_t start_pull_time;
  ContainerInfo():container(), status(),
  work_dir(), gc_dir(), initd_endpoint(),
  initd_proc(), initd_config(NULL),
  initd_stub(NULL),
  initd_status_check_times(0),
  fetcher_name(),
  logs(),
  start_pull_time(0){}
  ~ContainerInfo() {
    delete initd_config;
    delete initd_stub; 
  }
};

typedef boost::function<void (const ContainerState& pre_state, const std::string& name)> Handle;
typedef std::map<ContainerState, Handle>  FSM;

class EngineImpl : public Engine {

public:
  EngineImpl(const std::string& work_dir,
             const std::string& gc_dir);
  ~EngineImpl();
  bool Init();
  void RunContainer(RpcController* controller,
               const RunContainerRequest* request,
               RunContainerResponse* response,
               Closure* done);
  void ShowContainer(RpcController* controller,
               const ShowContainerRequest* request,
               ShowContainerResponse* response,
               Closure* done);
  void ShowCLog(RpcController* controller,
               const ShowCLogRequest* request,
               ShowCLogResponse* response,
               Closure* done);
  void JailContainer(RpcController* controller,
                     const JailContainerRequest* request,
                     JailContainerResponse* response,
                     Closure* done);
private:

  void StartContainerFSM(const std::string& name);

  // pull a image and produce a state,
  // if pulling is successfully, produce a kContainerRunning state 
  // or produce a kContainerPulling when retry is in limit
  // or produce a kContainerPullFailed 
  void HandlePullImage(const ContainerState& pre_state, 
                       const std::string& name);

  // handle boot initd , alloc port for it
  // and load rootfs
  void HandleBootInitd(const ContainerState& pre_state,
                       const std::string& name);

  // run a image or pull container state from initd
  // pre_state is kContainerPulling, run a image
  // pre_state is kContainerRunning, pull container state from initd
  void HandleRunContainer(const ContainerState& pre_state,
                          const std::string& name);

  // every handle has a unified result
  // target_state , container name , delay task interval
  void ProcessHandleResult(const ContainerState& target_state,
                           const ContainerState& current_state,
                           const std::string& name,
                           int32_t exec_task_interval);

  // record container state log
  void AppendLog(const ContainerState& cfrom, 
                 const ContainerState& cto,
                 const std::string& msg,
                 ContainerInfo* info);
  void CleanProcessInInitd(const std::string& name, ContainerInfo* info);
  void KillProcessCallback(const KillRequest* request, KillResponse* response,
                           bool failed, int);
  // add user info to process
  bool HandleProcessUser(Process* process);

  void HandleError(const ContainerState& pre_state,
                   const std::string& name);

  void HandleCompleteContainer(const ContainerState& pre_state,
                               const std::string& name);
private:
  ::baidu::common::Mutex mutex_;
  typedef std::map<std::string, ContainerInfo*> Containers;
  Containers* containers_;
  ::baidu::common::ThreadPool* thread_pool_;
  std::string work_dir_;
  std::string gc_dir_;
  FSM* fsm_;
  RpcClient* rpc_client_;
  std::queue<int32_t>* ports_;
  UserMgr* user_mgr_;
};

} // namespace dos
#endif
