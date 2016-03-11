#include "engine/isolator.h"
#include "engine/utils.h"
#include "logging.h"

using ::baidu::common::INFO;
using ::baidu::common::WARNING;
using ::baidu::common::DEBUG;

namespace dos {

CpuIsolator::CpuIsolator(const std::string& path):
  path_(path){}

CpuIsolator::~CpuIsolator() {}

bool CpuIsolator::Init() {
  bool ok = Mkdir(path_);
  if (!ok) {
    LOG(WARNING,"fail to mkdir %s", path_.c_str());
    return false;
  }
  return true;
}

bool CpuIsolator::Attach(int32_t pid) {

}


} // end of namespace
