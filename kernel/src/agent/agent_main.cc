#include <signal.h>
#include <unistd.h>
#include <sstream>
#include <string>

#include <sofa/pbrpc/pbrpc.h>
#include <gflags/gflags.h>

#include "version.h"
#include "agent/agent_impl.h"
#include "logging.h"

DECLARE_string(agent_port);

using ::baidu::common::INFO;
using ::baidu::common::WARNING;

static volatile bool s_quit = false;
static void SignalIntHandler(int /*sig*/){
    s_quit = true;
}

int main(int argc, char* args[]) {
  if (strcmp(args[1], "-v") == 0 ||
      strcmp(args[1], "--version") == 0) {
    PrintVersion();
    exit(1);
  }

  ::google::ParseCommandLineFlags(&argc, &args, true);
  sofa::pbrpc::RpcServerOptions options;
  sofa::pbrpc::RpcServer rpc_server(options);
  dos::AgentImpl* agent = new dos::AgentImpl();
  if (!rpc_server.RegisterService(agent)) {
    LOG(WARNING, "fail to register agent rpc service");
    exit(1);
  }
  std::string endpoint = "0.0.0.0:" + FLAGS_agent_port;
  if (!rpc_server.Start(endpoint)) {
    LOG(WARNING, "fail to listen port %s", FLAGS_agent_port.c_str());
    exit(1);
  }
  bool ok = agent->Start();
  if (!ok) {
    LOG(WARNING, "fail to start agent");
    exit(1);
  }
  LOG(INFO, "start agent on port %s", FLAGS_agent_port.c_str());
  signal(SIGINT, SignalIntHandler);
  signal(SIGTERM, SignalIntHandler);
  while (!s_quit) {
    sleep(1);
  }
  return 0;
}
