// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "engine/initd.h"

#include <signal.h>
#include <unistd.h>
#include <sstream>
#include <string>
#include <sofa/pbrpc/pbrpc.h>
#include <logging.h>
#include <gflags/gflags.h>
#include <engine/oc.h>
#include "version.h"

DECLARE_string(ce_initd_port);
DECLARE_string(ce_initd_conf_path);
using ::baidu::common::INFO;
using ::baidu::common::WARNING;

const std::string kDosCeUsage = "dos_ce help message.\n"
                                 "Usage:\n"
                                 "    dos_ce initd \n" 
                                 "Options:\n"
                                 "    -d     Start dos_ce deamon\n"
                                 "    -v     Show dos_ce build information\n";

static volatile bool s_quit = false;
static void SignalIntHandler(int /*sig*/){
    s_quit = true;
}

void StartInitd() {
  std::string oc_path = ".";
  dos::Oc oc(oc_path, FLAGS_ce_initd_conf_path);
  bool ok = oc.Init();
  if (!ok) {
    LOG(WARNING, "fail to init rootfs");
    exit(0);
  }
  sofa::pbrpc::RpcServerOptions options;
  sofa::pbrpc::RpcServer rpc_server(options);
  dos::InitdImpl* initd = new dos::InitdImpl();
  if (!rpc_server.RegisterService(initd)) {
    LOG(WARNING, "failed to register initd service");
    exit(1);
  }
  std::string server_addr = "0.0.0.0:" + FLAGS_ce_initd_port;
  if (!rpc_server.Start(server_addr)) {
    LOG(WARNING, "failed to start initd on %s", server_addr.c_str());
    exit(1);
  }
  LOG(INFO, "start initd on port %s", FLAGS_ce_initd_port.c_str());
  signal(SIGINT, SignalIntHandler);
  signal(SIGTERM, SignalIntHandler);
  while (!s_quit) {
    sleep(1);
  }
}

int main(int argc, char * args[]) {
  ::baidu::common::SetLogLevel(::baidu::common::DEBUG);
  ::google::SetUsageMessage(kDosCeUsage);
  if(argc < 2){
    fprintf(stderr,"%s", kDosCeUsage.c_str());
    return -1;
  }
	if (strcmp(args[1], "-v") == 0 ||
      strcmp(args[1], "--version") == 0) {
    PrintVersion();
    exit(0);
  } else if (strcmp(args[1], "init") == 0) {
    ::google::ParseCommandLineFlags(&argc, &args, true);
    StartInitd();
  } else {
    fprintf(stderr,"%s", kDosCeUsage.c_str());
    return -1;
  }
  return 0;
}
