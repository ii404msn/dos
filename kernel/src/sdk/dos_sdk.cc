#include "sdk/dos_sdk.h"
#include "proto/engine.pb.h"
#include "proto/dos.pb.h"
#include "proto/master.pb.h"
#include "rpc/rpc_client.h"
namespace dos {

class EngineSdkImpl : public EngineSdk {

public:
  EngineSdkImpl(const std::string& addr):rpc_client_(NULL),
  engine_(NULL), addr_(addr) {
    rpc_client_ = new RpcClient();
  }
  bool Init() {
    bool get_ok = rpc_client_->GetStub(addr_, &engine_);
    return get_ok;
  }
  virtual ~EngineSdkImpl() {
    delete engine_;
    delete rpc_client_;
  }

  SdkStatus Run(const std::string& name, 
                const CDescriptor& desc);

  SdkStatus ShowAll(std::vector<CInfo>& containers);
  SdkStatus ShowCLog(const std::string& name,
                     std::vector<CLog>& logs);
  SdkStatus Submit(const JobDescriptor& job);
  SdkStatus Jail(const std::string& name,
                 const JailProcess& process);
private:
  RpcClient* rpc_client_;
  Engine_Stub* engine_;
  std::string addr_;
};

EngineSdk* EngineSdk::Connect(const std::string& addr) {
  EngineSdkImpl* engine = new EngineSdkImpl(addr);
  bool init_ok = engine->Init();
  if (init_ok) {
    return engine;
  }else {
    return NULL;
  }
}

SdkStatus EngineSdkImpl::Jail(const std::string& name,
                              const JailProcess& process) {
  JailContainerRequest request;
  request.set_c_name(name);
  request.mutable_process()->add_args(process.cmds);
  request.mutable_process()->mutable_user()->set_name(process.user);
  for (size_t i = 0; i < process.envs.size(); ++i) {
    request.mutable_process()->add_envs(process.envs[i]);
  }
  request.mutable_process()->set_pty(process.pty);
  JailContainerResponse response;
  bool rpc_ok = rpc_client_->SendRequest(engine_, &Engine_Stub::JailContainer,
                                         &request, &response, 5, 1);
  if (rpc_ok) {
    return kSdkOk;
  }
  return kSdkError;
}

SdkStatus EngineSdkImpl::Run(const std::string& name, 
    const CDescriptor& desc) {
  RunContainerRequest request;
  request.set_name(name);
  ContainerType type ;
  bool parse_ok = ContainerType_Parse(desc.type, &type);
  if (!parse_ok) {
    return kSdkParseDescError;
  }
  request.mutable_container()->set_type(type);
  request.mutable_container()->set_uri(desc.uri);
  RunContainerResponse response;
  bool rpc_ok = rpc_client_->SendRequest(engine_, &Engine_Stub::RunContainer,
                           &request, &response, 5, 1);
  if (!rpc_ok) {
    return kSdkError;
  }
  if (response.status() == kRpcNameExist) {
    return kSdkNameExist;
  }
  return kSdkOk;
}

SdkStatus EngineSdkImpl::ShowAll(std::vector<CInfo>& containers) {
  ShowContainerRequest request;
  ShowContainerResponse response;
  bool rpc_ok = rpc_client_->SendRequest(engine_, 
                                        &Engine_Stub::ShowContainer,
                                        &request, &response, 5, 1);
  if (!rpc_ok) {
    return kSdkError;
  }
  for (int i = 0; i < response.containers_size(); i++) {
    CInfo info;
    info.name = response.containers(i).name();
    info.state = ContainerState_Name(response.containers(i).state());
    info.rtime = response.containers(i).start_time();
    info.btime = response.containers(i).boot_time();
    info.type = ContainerType_Name(response.containers(i).type());
    containers.push_back(info);
  }
  return kSdkOk;
}

SdkStatus EngineSdkImpl::ShowCLog(const std::string& name,
                                  std::vector<CLog>& logs){
  ShowCLogRequest request;
  request.set_name(name);
  ShowCLogResponse response;
  bool rpc_ok = rpc_client_->SendRequest(engine_, 
                                        &Engine_Stub::ShowCLog,
                                        &request, &response, 5, 1);
  if (!rpc_ok || response.status() != kRpcOk) {
    return kSdkError;
  }
  for (int32_t i = 0; i < response.logs_size(); ++i) {
    CLog log;
    log.name = response.logs(i).name();
    log.time = response.logs(i).time();
    log.cfrom = ContainerState_Name(response.logs(i).cfrom());
    log.cto = ContainerState_Name(response.logs(i).cto());
    log.msg = response.logs(i).msg();
    logs.push_back(log);
  }
  return kSdkOk;
}

// Dos Sdk implemetation
class DosSdkImpl : public DosSdk {
  public:
    DosSdkImpl(const std::string& addr): rpc_client_(NULL),
      master_(NULL), addr_(addr){ 
      rpc_client_ = new RpcClient();
    }
    virtual ~DosSdkImpl(){}
    bool Init() {
      bool get_ok = rpc_client_->GetStub(addr_, &master_);
      return get_ok;
    }
    SdkStatus Submit(const JobDescriptor& job);
  private:
    RpcClient* rpc_client_;
    Master_Stub* master_;
    std::string addr_;
};

DosSdk* DosSdk::Connect(const std::string& dos_addr) {
  DosSdkImpl* dos_sdk = new DosSdkImpl(dos_addr);
  if (dos_sdk->Init()) {
    return dos_sdk;
  }
  delete dos_sdk;
  return NULL;
}

SdkStatus DosSdkImpl::Submit(const JobDescriptor& job) {

  SubmitJobRequest request;
  request.set_user_name("imotai");
  request.mutable_job()->set_name(job.name);
  request.mutable_job()->set_replica(job.replica);
  request.mutable_job()->set_deploy_step_size(job.deploy_step_size);
  for (size_t i = 0; i < job.pod.containers.size(); ++i) {
    Container* container = request.mutable_job()->mutable_pod()->add_containers();
    container->set_uri(job.pod.containers[i].uri);
    ContainerType c_type;
    if (!ContainerType_Parse(job.pod.containers[i].type, &c_type)) {
      return kSdkInvalidateEnum;
    }
    container->set_type(c_type);
    std::set<uint32_t>::iterator p_it = job.pod.containers[i].ports.begin();
    for (; p_it != job.pod.containers[i].ports.end(); ++p_it) {
      container->mutable_requirement()->mutable_port()->add_assigned(*p_it);
    }
    container->set_reserve_time(10000);
    container->mutable_requirement()->mutable_cpu()->set_limit(job.pod.containers[i].millicores);
    container->mutable_requirement()->mutable_memory()->set_limit(job.pod.containers[i].memory);
    container->set_restart_strategy(kAlways);
  }
  SubmitJobResponse response;
  bool rpc_ok = rpc_client_->SendRequest(master_, &Master_Stub::SubmitJob, 
                                         &request, &response, 5, 1);
  if (!rpc_ok || response.status() != kRpcOk) {
    return kSdkError;
  }
  return kSdkOk;
}

}
