#ifndef KENERL_COMMON_INS_WATCHER_H
#define KENERL_COMMON_INS_WATCHER_H

#include <boost/function.hpp>
#include "ins_sdk.h"
#include "logging.h"

namespace dos {

using ::galaxy::ins::sdk::InsSDK;
using ::baidu::common::INFO;
using ::baidu::common::WARNING;
using ::baidu::common::FATAL;

class InsWatcher;

typedef boost::function<void (const std::string& value)> WatchHandle;

// nexus path watcher
class InsWatcher {

public:
  InsWatcher(const std::string& path,
             InsSDK* ins,
             WatchHandle handle):path_(path),
             ins_(ins),
             handle_(handle),
             value_(){}
  ~InsWatcher() {}

  bool Watch() { 
    if (ins_ == NULL) {
      return false;
    }
    return NewWatch();
  }

  static void HandleChange(const ::galaxy::ins::sdk::WatchParam& param,
                         ::galaxy::ins::sdk::SDKError /*error*/) {
    InsWatcher* watcher = static_cast<InsWatcher*>(param.context);
    bool ok = watcher->NewWatch();
    if (!ok) {
      LOG(WARNING, "fail to do next watch");
      abort();
    }
  } 

  bool NewWatch() {
    ::galaxy::ins::sdk::SDKError err;
    bool ok = ins_->Watch(path_, &InsWatcher::HandleChange, this, &err);
    if (!ok) {
      LOG(WARNING, "fail to watch key %s", path_.c_str());
      return false;
    }
    std::string new_value;
    bool get_ok = ins_->Get(path_, &new_value, &err);
    if (!get_ok) {
      LOG(WARNING, "fail to get key %s", path_.c_str());
      return false;
    }
    if (new_value != value_) {
      value_ = new_value;
      handle_(value_);
    }
    return true;
  }

  bool GetValue(std::string* value) {
    if (value == NULL) {
      return false;
    }
    *value = value_;
    return true;
  }

private:
  std::string path_;
  InsSDK* ins_;
  WatchHandle handle_;
  std::string value_;
};

}
#endif

