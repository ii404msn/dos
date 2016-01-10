#ifndef KERNEL_MASTER_IMPL_H
#define KERNEL_MASTER_IMPL_H
#include "proto/master.pb.h"

#include "master/blocking_queue.h"
#include "master/node_manager.h"

using ::google::protobuf::RpcController;
using ::google::protobuf::Closure;
namespace dos {

class MasterImpl : public Master {

public:
  MasterImpl();
  ~MasterImpl();

  void SubmitJob(RpcController* controller,
                   const SubmitJobRequest* request,
                   SubmitJobResponse* response,
                   Closure* done);

private:

};

}
#endif
