#ifndef KERNEL_SDK_ENGINE_SDK_H
#define KERNEL_SDK_ENGINE_SDK_H

#include <string>
#include <vector>
#include <stdint.h>

namespace dos {

enum SdkStatus {
  kSdkOk,
  kSdkError,
  kSdkParseDescError,
  kSdkNameExist
};

struct CDescriptor {
  // kOci , kDocker
  std::string type;
  std::string uri;
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

struct PodDescritpir {
  std::vector<CDescriptor> containers;
};

class DosSdk {

public:
  static DosSdk* Connect(const std::string& addr);

  virtual SdkStatus Run(const std::string& name, 
                        const CDescriptor& desc) = 0;
  virtual SdkStatus ShowAll(std::vector<CInfo>& containers) = 0;
  virtual SdkStatus ShowCLog(const std::string& name,
                             std::vector<CLog>& logs) = 0;
};

}

#endif
