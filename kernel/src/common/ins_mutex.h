#ifndef KENERL_COMMON_INS_MUTEX_H
#define KENERL_COMMON_INS_MUTEX_H

#include <boost/function.hpp>
#include "ins_sdk.h"
#include "logging.h"

namespace dos {

using ::galaxy::ins::sdk::InsSDK;
using ::baidu::common::INFO;
using ::baidu::common::WARNING;
using ::baidu::common::FATAL;


class InsMutex {

public:
  InsMutex(const std::string& path,
           InsSDK* ins):path_(path),ins_(ins) {}

  ~InsMutex(){
    UnLock();
  }
  // lock will try to get lock of path
  // lock sucessfully return true, else will be blocked
  // errors occurs return false
  bool Lock() {
    ::galaxy::ins::sdk::SDKError err;
    if (ins_ == NULL) {
      return false;
    }
    bool lock_ok = ins_->Lock(path_, &err);
    if (lock_ok && err == ::galaxy::ins::sdk::kOK) {
      LOG(INFO, "lock %s sucessfully", path_.c_str());
      return true;
    }
    LOG(WARNING, "some err happened");
    return false;
  }

private:
  void UnLock() {
    if (ins_ == NULL) {
      return;
    }
    ::galaxy::ins::sdk::SDKError err;
    ins_->UnLock(path_, &err);
  }

private:
  std::string path_;
  InsSDK* ins_;
};

}
#endif
