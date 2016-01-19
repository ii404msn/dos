#ifndef KERNEL_ENGINE_IMPL_H
#define KERNEL_ENGINE_IMPL_H

#include <map>
#include "proto/engine.pb.h"
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include "mutex.h"
#include "thread_pool.h"
#include "engine/process_mgr.h"

using ::google::protobuf::RpcController;
using ::google::protobuf::Closure;

namespace dos {

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
  ProcessMgr init_proc; 
};

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

  // run a image or pull container state from initd
  // pre_state is kContainerPulling, run a image
  // pre_state is kContainerRunning, pull container state from initd
  void HandleRunContainer(const ContainerState& pre_state,
                          const std::string& name);
private:
  ::baidu::common::Mutex mutex_;
  typedef std::map<std::string, ContainerInfo*> Containers;
  Containers* containers_;
  ::baidu::common::ThreadPool* thread_pool_;
  std::string work_dir_;
  std::string gc_dir_;
  typedef boost::function<void (const ContainerState& pre_state, const std::string& name)> Handle;
  typedef std::map<ContainerState, Handle>  FSM;
  FSM* fsm_;
};

} // namespace dos
#endif
