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

typedef boost::function<void (const std::string& value)> WatchHandle;

static void HandleChange(const ::galaxy::ins::sdk::WatchParam& param,
                         ::galaxy::ins::sdk::SDKError /*error*/) {
  InsWatcher* watcher = static_cast<InsWatcher*>(param.context);
  bool ok = watcher->NewWatch();
  if (!ok) {
    LOG(WARNING, "fail to do next watch");
    abort();
  }
  return ok;
}

class InsWatcher {

public:
  InsWatcher(const std::string& path,
             const std::string& ins_servers,
             WatchHandle handle):handle_(handle),path_(path_), 
             ins_servers_(ins_servers),
             ins_(NULL),
             value_(){}
  ~InsWatcher() {}
  bool Init() { 
    ins_ = new InsSDK(ins_servers_);
    return true;
  }

  bool NewWatch() {
    ::galaxy::ins::sdk::SDKError err;
    bool ok = ins_->Watch(path_, &HandleChange, this, &err);
    if (!ok) {
      LOG(WARNING, "fail to watch key %s", path_);
      return false;
    }
    std::string new_value;
    bool get_ok = ins_->Get(path_, &new_value, &err);
    if (!get_ok) {
      LOG(WARNING, "fail to get key %s", path_);
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
  WatchHandle handle_;
  std::string path_;
  std::string ins_servers_;
  InsSDK* ins_;
  std::string value_;
};

}
#endif

