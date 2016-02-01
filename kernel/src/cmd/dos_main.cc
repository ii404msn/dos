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
#include "master/master_impl.h"
#include "scheduler/scheduler.h"
#include "agent/agent_impl.h"
#include "sdk/dos_sdk.h"
#include "version.h"

DECLARE_string(ce_initd_port);
DECLARE_string(ce_port);
DECLARE_string(ce_initd_conf_path);
DECLARE_bool(ce_enable_ns);
DECLARE_string(ce_work_dir);
DECLARE_string(ce_gc_dir);
DECLARE_string(agent_endpoint);
DECLARE_string(master_port);

DEFINE_string(n, "", "specify container name");
DEFINE_bool(v, true, "show version");
DEFINE_string(u, "", "specify uri for rootfs");
DEFINE_int32(r, 1,  "specify replica for job");
DEFINE_int32(c, 1,  "specify cpu for pod instance");
DEFINE_int32(d, 1,  "specify deploy step size for job");
DEFINE_string(ce_endpoint, "127.0.0.1:7676", "specify container engine endpoint");

using ::baidu::common::INFO;
using ::baidu::common::WARNING;
using ::baidu::common::DEBUG;

const std::string kDosCeUsage = "dos help message.\n"
                                "Usage:\n"
                                "    dos initd \n" 
                                "    dos engine \n" 
                                "    dos master \n" 
                                "    dos scheduler \n" 
                                "    dos let \n" 
                                "    dos run -u <uri>  -n <name>\n" 
                                "    dos submit -u <uri>  -n <name> -r <replica> -c <cpu> -m <memory> -d <deploy_step_size>\n"
                                "    dos ps -j <job name> | -e <agent endpoint>\n"
                                "    dos log -n <name> \n"
                                "    dos version\n"
                                "Options:\n"
                                "    -u     Specify uri for download rootfs\n"
                                "    -n     Specify name for container\n"
                                "    -j     Specify name for job\n"
                                "    -r     Specify replica for job\n"
                                "    -c     Specify cpu for single pod instance\n"
                                "    -m     Specify memory for single pod instance\n"
                                "    -d     Specify deploy step size for job \n"
                                "    -e     Specify endpoint for agent\n";

static volatile bool s_quit = false;
static void SignalIntHandler(int /*sig*/){
    s_quit = true;
}

std::string FormatDate(int64_t datetime) {
  if (datetime < 100) {
    return "-";
  }
  char buffer[100];
  time_t time = datetime / 1000000;
  struct tm *tmp;
  tmp = localtime(&time);
  strftime(buffer, 100, "%F %X", tmp);
  std::string ret(buffer);
  return ret;
}

std::string PrettyTime(const int64_t time) {
  int64_t last = time / 1000000;
  char label[3] = {'s', 'm', 'h'};
  double num = last;
  int count = 0;
  while (num > 60 && count < 3 ) {
    count ++;
    num /= 60;
  }
  return ::baidu::common::NumToString(num) + label[count];
}

void StartScheduler() {
  std::string master_endpoint = "127.0.0.1:" + FLAGS_master_port;
  ::dos::Scheduler* scheduler = new ::dos::Scheduler(master_endpoint);
  bool ok = scheduler->Start();
  if (!ok) {
    LOG(WARNING, "fail to start scheduler");
    exit(1);
  }
  LOG(INFO, "start scheduler successfully");
  signal(SIGINT, SignalIntHandler);
  signal(SIGTERM, SignalIntHandler);
  while (!s_quit) {
    sleep(1);
  }

}

void StartMaster() {
  sofa::pbrpc::RpcServerOptions options;
  sofa::pbrpc::RpcServer rpc_server(options);
  dos::MasterImpl* master = new dos::MasterImpl();
  if (!rpc_server.RegisterService(master)) {
    LOG(WARNING, "fail to register master rpc service");
    exit(1);
  }
  master->Start();
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
}

void StartAgent() {
  sofa::pbrpc::RpcServerOptions options;
  sofa::pbrpc::RpcServer rpc_server(options);
  dos::AgentImpl* agent = new dos::AgentImpl();
  if (!rpc_server.RegisterService(agent)) {
    LOG(WARNING, "fail to register agent rpc service");
    exit(1);
  }
  if (!rpc_server.Start(FLAGS_agent_endpoint)) {
    LOG(WARNING, "fail to start agent on %s", FLAGS_agent_endpoint.c_str());
    exit(1);
  }
  bool ok = agent->Start();
  if (!ok) {
    LOG(WARNING, "fail to start agent");
    exit(1);
  }
  LOG(INFO, "start agent on %s", FLAGS_agent_endpoint.c_str());
  signal(SIGINT, SignalIntHandler);
  signal(SIGTERM, SignalIntHandler);
  while (!s_quit) {
    sleep(1);
  }
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

void StartEngine() { 
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

void ShowLog() {
  if (FLAGS_n.empty()) {
    fprintf(stderr, "-n is required \n");
    exit(1);
  }
  
  dos::EngineSdk* engine = dos::EngineSdk::Connect(FLAGS_ce_endpoint);
  if (engine == NULL) {
    fprintf(stderr, "fail to connect %s \n", FLAGS_ce_endpoint.c_str());
    exit(1);
  }

  std::vector<dos::CLog> logs;
  dos::SdkStatus status = engine->ShowCLog(FLAGS_n, logs);
  if (status == dos::kSdkOk) {
    std::vector<dos::CLog>::iterator it = logs.begin();
    for (; it != logs.end(); ++it) {
      fprintf(stdout, "%s %s change state from %s to %s with msg %s \n", 
          FormatDate(it->time).c_str(), it->name.c_str(), 
          it->cfrom.c_str(), it->cto.c_str(), it->msg.c_str());
    }
  }else  {
    fprintf(stderr, "show log %s fails\n", FLAGS_n.c_str());
    exit(1);
  }

}

void Run() {
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
  ::baidu::common::TPrinter tp(6);
  tp.AddRow(6, "", "name", "type","state", "rtime", "btime");
  for (size_t i = 0; i < containers.size(); i++) {
    std::vector<std::string> vs;
    vs.push_back(baidu::common::NumToString((int32_t)i + 1));
    vs.push_back(containers[i].name);
    vs.push_back(containers[i].type);
    vs.push_back(containers[i].state);
    if (containers[i].rtime <= 1000) {
      vs.push_back("-");
    } else {
      vs.push_back(PrettyTime(::baidu::common::timer::get_micros() - containers[i].rtime));
    }
    if (containers[i].btime > 0) {
      vs.push_back(PrettyTime(containers[i].btime));
    } else {
      vs.push_back("-");
    }
    tp.AddRow(vs);
  }
  printf("%s\n", tp.ToString().c_str());
  exit(0);
}

void SubmitJob() {
  if (FLAGS_n.empty()) {
    fprintf(stderr, "-n is required \n");
    exit(1);
  }
  if (FLAGS_u.empty()) {
    fprintf(stderr, "-u is required \n");
    exit(1);
  }
  ::dos::JobDescriptor job;
  job.name= FLAGS_n;
  job.deploy_step_size = FLAGS_d;
  ::dos::CDescriptor container;
  container.type = "kOci";
  container.uri = FLAGS_u;
  job.pod.containers.push_back(container);
  job.replica = FLAGS_r;
  std::string master_endpoint = "127.0.0.1:" + FLAGS_master_port;
  ::dos::DosSdk* dos_sdk = ::dos::DosSdk::Connect(master_endpoint);
  if (dos_sdk == NULL) {
    fprintf(stderr, "fail to create dos sdk \n");
    exit(1);
  }
  dos::SdkStatus status = dos_sdk->Submit(job);
  if (status == dos::kSdkOk) {
    fprintf(stdout, "submit job successfully\n");
    return;
  }
  fprintf(stderr, "fail to submit job\n");
}

int main(int argc, char * args[]) {
  ::baidu::common::SetLogLevel(DEBUG);
  ::google::SetUsageMessage(kDosCeUsage);
  if(argc < 2){
    fprintf(stderr,"%s", kDosCeUsage.c_str());
    return -1;
  }
  ::google::ParseCommandLineFlags(&argc, &args, true);
	if (strcmp(args[1], "version") == 0 ) {
    PrintVersion();
    exit(0);
  } else if (strcmp(args[1], "initd") == 0) {
    StartInitd();
  } else if (strcmp(args[1], "engine") == 0) {
    StartEngine();
  } else if (strcmp(args[1], "master") == 0) {
    StartMaster();
  } else if (strcmp(args[1], "let") == 0) {
    StartAgent();
  } else if (strcmp(args[1], "run") == 0) {
    Run();
  } else if (strcmp(args[1], "ps") == 0) {
    Show();
  } else if (strcmp(args[1], "log") == 0) {
    ShowLog();
  } else if (strcmp(args[1], "submit") == 0) {
    SubmitJob();
  } else if (strcmp(args[1], "scheduler") == 0) {
    StartScheduler();
  } else {
    fprintf(stderr,"%s", kDosCeUsage.c_str());
    return -1;
  }
  return 0;
}
