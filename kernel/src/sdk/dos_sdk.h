#ifndef KERNEL_SDK_ENGINE_SDK_H
#define KERNEL_SDK_ENGINE_SDK_H

#include <string>
#include <vector>
#include <stdint.h>
#include <set>

namespace dos {

enum SdkStatus {
  kSdkOk,
  kSdkError,
  kSdkParseDescError,
  kSdkNameExist,
  kSdkInvalidateEnum
};

struct CDescriptor {
  // kOci , kDocker
  std::string type;
  std::string uri;
  std::set<uint32_t> ports;
};

struct CInfo {
  std::string name;
  int64_t rtime;
  std::string state;
  std::string type;
  int64_t btime;
};

struct CLog {
  std::string name;
  int64_t time;
  std::string cfrom;
  std::string cto;
  std::string msg;
};

struct PodDescritpor {
  std::vector<CDescriptor> containers;
};

struct JobDescriptor {
  std::string name;
  PodDescritpor pod;
  uint32_t replica;
  uint32_t deploy_step_size;
  std::string raw;
};

struct JailProcess {
  std::string cmds;
  std::vector<std::string> envs;
  std::string user;
  std::string pty;
};

class EngineSdk {

public:
  static EngineSdk* Connect(const std::string& engine_addr);

  virtual SdkStatus Run(const std::string& name, 
                        const CDescriptor& desc) = 0;
  virtual SdkStatus ShowAll(std::vector<CInfo>& containers) = 0;
  virtual SdkStatus ShowCLog(const std::string& name,
                             std::vector<CLog>& logs) = 0;
  virtual SdkStatus Jail(const std::string& name,
                         const JailProcess& process) = 0;
};

class DosSdk {
  public:
    static DosSdk* Connect(const std::string& dos_addr);
    // submit a job to dos
    virtual SdkStatus Submit(const JobDescriptor& job) = 0;
};

}

#endif
