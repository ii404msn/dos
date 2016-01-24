// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include <signal.h>
#include <unistd.h>
#include <sstream>
#include <string>
#include <sofa/pbrpc/pbrpc.h>
#include <logging.h>
#include <gflags/gflags.h>
#include "engine/oc.h"
#include "engine/initd.h"
#include "tprinter.h"
#include "string_util.h"
#include "timer.h"
#include "engine/engine_impl.h"
#include "sdk/engine_sdk.h"
#include "version.h"

DECLARE_string(ce_initd_port);
DECLARE_string(ce_port);
DECLARE_string(ce_initd_conf_path);
DECLARE_bool(ce_enable_ns);
DECLARE_string(ce_work_dir);
DECLARE_string(ce_gc_dir);

DEFINE_string(n, "", "specify container name");
DEFINE_string(v, "", "show version");
DEFINE_string(u, "", "specify uri for rootfs");
DEFINE_string(ce_endpoint, "127.0.0.1:9527", "specify container engine endpoint");

using ::baidu::common::INFO;
using ::baidu::common::WARNING;

const std::string kDosCeUsage = "dos_ce help message.\n"
                                "Usage:\n"
                                "    dos_ce initd \n" 
                                "    dos_ce daemon \n" 
                                "    dos_ce run -u <uri>  -n <name>\n" 
                                "    dos_ce ps \n" 
                                "Options:\n"
                                "    -v     Show dos_ce build information\n"
                                "    -u     Specify uri for download rootfs\n"
                                "    -n     Specify name for container\n";

static volatile bool s_quit = false;
static void SignalIntHandler(int /*sig*/){
    s_quit = true;
}

std::string PrettyTime(const int64_t start_time) {
  int64_t last = ( baidu::common::timer::get_micros() - start_time) / 1000000;
  char label[3] = {'s', 'm', 'h'};
  double num = last;
  int count = 0;
  while (num > 60 && count < 3 ) {
    count ++;
    num /= 60;
  }
  return ::baidu::common::NumToString(num) + label[count];
 }

void StartInitd() {
  if (FLAGS_ce_enable_ns) {
    std::string oc_path = ".";
    dos::Oc oc(oc_path, FLAGS_ce_initd_conf_path);
    bool ok = oc.Init();
    if (!ok) {
      LOG(WARNING, "fail to init rootfs");
      exit(0);
    }
  } else {
    LOG(INFO, "disable linux namespace");
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

void StartDeamon() { 
  sofa::pbrpc::RpcServerOptions options;
  sofa::pbrpc::RpcServer rpc_server(options);
  dos::EngineImpl* engine = new dos::EngineImpl(FLAGS_ce_work_dir, FLAGS_ce_gc_dir);
  bool init_ok = engine->Init();
  if (!init_ok) {
    LOG(WARNING, "fail to init engine");
    exit(1);
  }
  if (!rpc_server.RegisterService(engine)) {
    LOG(WARNING, "failed to register engine service");
    exit(1);
  }
  std::string server_addr = "0.0.0.0:" + FLAGS_ce_port;
  if (!rpc_server.Start(server_addr)) {
    LOG(WARNING, "failed to start engine on %s", server_addr.c_str());
    exit(1);
  }
  LOG(INFO, "start engine on port %s", FLAGS_ce_port.c_str());
  signal(SIGINT, SignalIntHandler);
  signal(SIGTERM, SignalIntHandler);
  while (!s_quit) {
    sleep(1);
  }
}

void Run () {
  if (FLAGS_n.empty()) {
    fprintf(stderr, "-n is required \n");
    exit(1);
  }
  if (FLAGS_u.empty()) {
    fprintf(stderr, "-u is required \n");
    exit(1);
  }
  dos::EngineSdk* engine = dos::EngineSdk::Connect(FLAGS_ce_endpoint);
  if (engine == NULL) {
    fprintf(stderr, "fail to connect %s \n", FLAGS_ce_endpoint.c_str());
    exit(1);
  }
  dos::CDescriptor desc;
  desc.uri = FLAGS_u;
  desc.type = "kOci";
  dos::SdkStatus status = engine->Run(FLAGS_n, desc);
  if (status == dos::kSdkOk) {
    fprintf(stdout, "run container %s successfully\n", FLAGS_n.c_str());
    exit(0);
  }else  {
    fprintf(stderr, "run container %s fails\n", FLAGS_n.c_str());
    exit(1);
  }
}

void Show() {
  dos::EngineSdk* engine = dos::EngineSdk::Connect(FLAGS_ce_endpoint);
  if (engine == NULL) {
    fprintf(stderr, "fail to connect %s \n", FLAGS_ce_endpoint.c_str());
    exit(1);
  }
  std::vector<dos::CInfo> containers;
  dos::SdkStatus status = engine->ShowAll(containers);
  if (status != dos::kSdkOk) {
    fprintf(stderr, "fail to show containers \n");
    exit(1);
  }
  ::baidu::common::TPrinter tp(5);
  tp.AddRow(5, "", "name", "type","state", "rtime");
  for (size_t i = 0; i < containers.size(); i++) {
    std::vector<std::string> vs;
    vs.push_back(baidu::common::NumToString((int32_t)i + 1));
    vs.push_back(containers[i].name);
    vs.push_back(containers[i].type);
    vs.push_back(containers[i].state);
    if (containers[i].rtime <= 1000) {
      vs.push_back("-");
    } else {
      vs.push_back(PrettyTime(containers[i].rtime));
    }
    tp.AddRow(vs);
  }
  printf("%s\n", tp.ToString().c_str());
  exit(0);
}

int main(int argc, char * args[]) {
  ::google::SetUsageMessage(kDosCeUsage);
  if(argc < 2){
    fprintf(stderr,"%s", kDosCeUsage.c_str());
    return -1;
  }
  ::google::ParseCommandLineFlags(&argc, &args, true);
	if (strcmp(args[1], "-v") == 0 ||
      strcmp(args[1], "--version") == 0) {
    PrintVersion();
    exit(0);
  } else if (strcmp(args[1], "initd") == 0) {
    StartInitd();
  } else if (strcmp(args[1], "daemon") == 0) {
    StartDeamon();
  } else if (strcmp(args[1], "run") == 0) {
    Run();
  } else if (strcmp(args[1], "ps") == 0) {
    Show();
  } else {
    fprintf(stderr,"%s", kDosCeUsage.c_str());
    return -1;
  }
  return 0;
}
