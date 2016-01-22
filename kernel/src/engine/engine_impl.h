#ifndef KERNEL_ENGINE_IMPL_H
#define KERNEL_ENGINE_IMPL_H

#include <map>
#include "proto/engine.pb.h"
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include "mutex.h"
#include "thread_pool.h"
#include "engine/process_mgr.h"
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
  ContainerInfo():container(), status(),
  work_dir(), gc_dir(), initd_endpoint(),
  initd_proc(), initd_config(NULL),
  initd_stub(NULL),
  initd_status_check_times(0){}
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

  void RunContainer(RpcController* controller,
               const RunContainerRequest* request,
               RunContainerResponse* response,
               Closure* done);
private:
  void StartContainerFSM(const std::string& name);
  // pull a image and produce a state,
  // if pulling is successfully, produce a kContainerRunning state 
  // or produce a kContainerPulling when retry is in limit
  // or produce a kContainerPullFailed 
  void HandlePullImage(const ContainerState& pre_state, 
                       const std::string& name);

  void HandleBootInitd(const ContainerState& pre_state,
                       const std::string& name);
  // run a image or pull container state from initd
  // pre_state is kContainerPulling, run a image
  // pre_state is kContainerRunning, pull container state from initd
  void HandleRunContainer(const ContainerState& pre_state,
                          const std::string& name);

  static int LunachInitd(void* config);
private:
  ::baidu::common::Mutex mutex_;
  typedef std::map<std::string, ContainerInfo*> Containers;
  Containers* containers_;
  ::baidu::common::ThreadPool* thread_pool_;
  std::string work_dir_;
  std::string gc_dir_;
  FSM* fsm_;
  RpcClient* rpc_client_;
};

} // namespace dos
#endif
