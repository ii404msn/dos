#include <signal.h>
#include <unistd.h>
#include <sstream>
#include <string>

#include <sofa/pbrpc/pbrpc.h>
#include <gflags/gflags.h>

#include "master/master_impl.h"
#include "logging.h"

DECLARE_string(master_port);

using ::baidu::common::INFO;
using ::baidu::common::WARNING;

static volatile bool s_quit = false;
static void SignalIntHandler(int /*sig*/){
    s_quit = true;
}

int main(int argc, char* args[]) {
  ::google::ParseCommandLineFlags(&argc, &args, true);
  sofa::pbrpc::RpcServerOptions options;
  sofa::pbrpc::RpcServer rpc_server(options);
  dos::Master* master = new dos::MasterImpl();
  if (!rpc_server.RegisterService(master)) {
    LOG(WARNING, "fail to register master rpc service");
    exit(1);
  }
  std::string endpoint = "0.0.0.0:" + FLAGS_master_port;
  if (!rpc_server.Start(endpoint)) {
    LOG(WARNING, "fail to listen port %s", FLAGS_master_port.c_str());
    exit(1);
  }
  LOG(INFO, "start master on port %s", FLAGS_master_port.c_str());
  signal(SIGINT, SignalIntHandler);
  signal(SIGTERM, SignalIntHandler);
  while (!s_quit) {
    sleep(1);
  }
  return 0;
}
