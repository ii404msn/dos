#ifndef DLFS_LET_IMPL_H
#define DLFS_LET_IMPL_H
#include "proto/dlfslet.pb.h"

#include "thread_pool.h"

using ::google::protobuf::RpcController;
using ::google::protobuf::Closure;

namespace dos {

// dlfs implementation
class DlfsLetImpl : public DlfsLet {

public:
  DlfsLetImpl();
  ~DlfsLetImpl();
  void WriteLayer(RpcController* controller,
                  const WriteLayerRequest* request,
                  WriteLayerResponse* response,
                  Closure* done);
private:
  // the thread pool for writing layer data
  ::baidu::common::ThreadPool* writer_pool_;
};

}
#endif
