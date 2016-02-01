#include "sdk/engine_sdk.h"
#include "proto/engine.pb.h"
#include "proto/dos.pb.h"
#include "rpc/rpc_client.h"
namespace dos {

class DosSdk : public DosSdk {

public:
  DosSdk(const std::string& addr):rpc_client_(NULL),
  engine_(NULL), addr_(addr) {
    rpc_client_ = new RpcClient();
  }
  bool Init() {
    bool get_ok = rpc_client_->GetStub(addr_, &engine_);
    return get_ok;
  }
  virtual ~DosSdk() {
    delete engine_;
    delete rpc_client_;
  }

  SdkStatus Run(const std::string& name, 
                const CDescriptor& desc);

  SdkStatus ShowAll(std::vector<CInfo>& containers);
  SdkStatus ShowCLog(const std::string& name,
                     std::vector<CLog>& logs);
private:
  RpcClient* rpc_client_;
  Engine_Stub* engine_;
  std::string addr_;
};

DosSdk* DosSdk::Connect(const std::string& addr) {
  DosSdk* engine = new DosSdk(addr);
  bool init_ok = engine->Init();
  if (init_ok) {
    return engine;
  }else {
    return NULL;
  }
}

SdkStatus DosSdk::Run(const std::string& name, 
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

SdkStatus DosSdk::ShowAll(std::vector<CInfo>& containers) {
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
    info.rtime = response.containers(i).rtime();
    info.btime = response.containers(i).boot_time();
    info.type = ContainerType_Name(response.containers(i).type());
    containers.push_back(info);
  }
  return kSdkOk;
}

SdkStatus DosSdk::ShowCLog(const std::string& name,
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

}
