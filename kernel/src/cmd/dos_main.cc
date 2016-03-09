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
#include <boost/lexical_cast.hpp>
#include "engine/oc.h"
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include "engine/initd.h"
#include "tprinter.h"
#include "string_util.h"
#include "timer.h"
#include "engine/engine_impl.h"
#include "master/master_impl.h"
#include "scheduler/scheduler.h"
#include "agent/agent_impl.h"
#include "sdk/dos_sdk.h"
#include "yaml-cpp/yaml.h"
#include "cmd/pty.h"
#include "dsh/dsh.h"
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
DEFINE_string(f, "job.yml", "specify job.yml to submit");
DEFINE_int32(r, 1,  "specify replica for job");
DEFINE_int32(c, 1,  "specify cpu for pod instance");
DEFINE_int32(d, 1,  "specify deploy step size for job");
DEFINE_int32(p, 0,  "specify port for job instance");
DEFINE_string(ce_endpoint, "127.0.0.1:7676", "specify container engine endpoint");

DEFINE_int32(terminal_lines, 90, "specify the terminal lines");
DEFINE_int32(terminal_columes, 139, "specify the terminal columes");
DEFINE_string(term, "xterm-256color", "specify the terminal mode");

using ::baidu::common::INFO;
using ::baidu::common::WARNING;
using ::baidu::common::DEBUG;

typedef boost::function<void ()> Handle;

const std::string kDosCeUsage = "dos help message.\n"
                                "Usage:\n"
                                "    dos run -u <uri>  -n <name>\n" 
                                "    dos submit -f <job.yml>\n"
                                "    dos ps -j <job name> | -e <agent endpoint>\n"
                                "    dos log -n <name> \n"
                                "    dos jail -n <name> \n"
                                "    dos version\n"
                                "Options:\n"
                                "    -u     Specify uri for download rootfs\n"
                                "    -n     Specify name for container\n"
                                "    -j     Specify name for job\n"
                                "    -r     Specify replica for job\n"
                                "    -c     Specify cpu for single pod instance\n"
                                "    -m     Specify memory for single pod instance\n"
                                "    -d     Specify deploy step size for job \n"
                                "    -p     Specify port for job port \n"
                                "    -e     Specify endpoint for agent\n";

static volatile bool s_quit = false;
static void SignalIntHandler(int /*sig*/){
    s_quit = true;
}

int ReadableStringToInt(const std::string& input, int64_t* output) {
  if (output == NULL) {
      return -1;
  }
  std::map<char, int32_t> subfix_table;
  subfix_table['K'] = 1;
  subfix_table['M'] = 2;
  subfix_table['G'] = 3;
  subfix_table['T'] = 4;
  subfix_table['B'] = 5;
  subfix_table['Z'] = 6;
  int64_t num = 0;
  char subfix = 0;
  int32_t shift = 0;
  int32_t matched = sscanf(input.c_str(), "%ld%c", &num, &subfix);
  if (matched <= 0) {
      return -1;
  }
  if (matched == 2) {
      std::map<char, int32_t>::iterator it = subfix_table.find(subfix);
      if (it == subfix_table.end()) {
          return -1;
      }
      shift = it->second;
  } 
  while (shift > 0) {
      num *= 1024;
      shift--;
  }
  *output = num;
  return 0;
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

void JailContainer() {
  if (FLAGS_n.empty()) {
    fprintf(stderr, "-n is required \n");
    exit(1);
  }
  dos::Pty pty;
  int master;
  std::string pty_path;
  bool create_ok = pty.Create(&master, &pty_path);
  if (!create_ok) {
    fprintf(stderr, "fail to create pty");
    exit(1);
  }
  ::dos::JailProcess process;
  process.envs.push_back("LINES="+ boost::lexical_cast<std::string>(FLAGS_terminal_lines));
  process.envs.push_back("COLUMES="+ boost::lexical_cast<std::string>(FLAGS_terminal_columes));
  process.envs.push_back("TERM="+ FLAGS_term);
  process.cmds = "bash";
  process.user = "root";
  process.pty = pty_path;
  dos::EngineSdk* engine = dos::EngineSdk::Connect(FLAGS_ce_endpoint);
  if (engine == NULL) {
    fprintf(stderr, "fail to connect %s \n", FLAGS_ce_endpoint.c_str());
    exit(1);
  }
  dos::SdkStatus status = engine->Jail(FLAGS_n, process);
  if (status == dos::kSdkOk) {
    pty.ConnectMaster(master);
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

void AddJob() {
  if (FLAGS_f.empty()){
    fprintf(stderr, "-f option is required \n ");
    return;
  } 
  YAML::Node node = YAML::LoadFile(FLAGS_f);
  if (!node["job"]) {
    fprintf(stderr, "job field is required in config file");
    return;
  }
  ::dos::JobDescriptor job;
  if (!node["job"]["name"]) {
    fprintf(stderr, "name is required in job \n");
    return;
  }
  job.name= node["job"]["name"].as<std::string>();
  job.deploy_step_size = node["job"]["deploy_step"].as<uint32_t>(); 
  ::dos::CDescriptor container;
  if (!node["job"]["type"]){
    fprintf(stderr, "type is required in job\n");
    return;
  }
  container.type = node["job"]["type"].as<std::string>();
  if (!node["job"]["url"]) {
    fprintf(stderr, "url is required is job\n");
    return;
  }
  container.uri = node["job"]["url"].as<std::string>();
  if (node["job"]["ports"]) {
    for (size_t index = 0; index < node["job"]["ports"].size(); index++) {
      container.ports.insert(node["job"]["ports"][index].as<uint32_t>());
    }
  }
  if (!node["job"]["cpu"]) {
    fprintf(stderr, "cpu is required\n");
    return;
  }
  container.millicores = node["job"]["cpu"].as<int32_t>();
  if (!node["job"]["memory"]) {
    fprintf(stderr, "memory is required\n");
    return;
  }
  int convert_ok = ReadableStringToInt(node["job"]["memory"].as<std::string>(), 
                                        &container.memory);
  if (convert_ok != 0) {
    fprintf(stderr, "fail to convert memory string\n");
    return;
  }
  job.pod.containers.push_back(container);
  if (!node["job"]["replica"]) {
    fprintf(stderr, "replica is required\n");
    return;
  } 
  job.replica = node["job"]["replica"].as<uint32_t>();
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
  fprintf(stderr, "fail to submit job for %d\n", status);
}

void StartDsh() {
  if (FLAGS_f.empty()){
    fprintf(stderr, "-f option is required \n ");
    exit(1);
  }
  ::dos::Dsh dsh;
  dsh.LoadAndRunByYml(FLAGS_f);
}

bool EndWiths(const std::string& str,
              const std::string& suffix) {
  if (str.length() >= suffix.length()) {
     return (0 == str.compare (str.length() - suffix.length(), suffix.length(), suffix));
  } else {
     return false;
  }
}

void GetJob() {
  if (FLAGS_n.empty()) {
    fprintf(stderr, "-n is required");
    exit(1);
  }
  std::string master_endpoint = "127.0.0.1:" + FLAGS_master_port;
  ::dos::DosSdk* dos_sdk = ::dos::DosSdk::Connect(master_endpoint);
  if (!dos_sdk) {
    fprintf(stderr, "fail to connect to dos");
    exit(1);
  }
  ::dos::JobInfo job;
  ::dos::SdkStatus status = dos_sdk->GetJob(FLAGS_n, &job);
  if (status != ::dos::kSdkOk) {
    fprintf(stderr, "fail to get job \n");
    exit(1);
  }
  ::baidu::common::TPrinter tp(7);
  tp.AddRow(7, "", "name", "running","death", "pending", "replica", "state");
  std::vector<std::string> vs;
  vs.push_back("1");
  vs.push_back(job.name);
  vs.push_back(::baidu::common::NumToString(job.running));
  vs.push_back(::baidu::common::NumToString(job.death));
  vs.push_back(::baidu::common::NumToString(job.pending));
  vs.push_back(::baidu::common::NumToString(job.replica));
  vs.push_back(job.state);
  tp.AddRow(vs);
  printf("%s\n", tp.ToString().c_str());
}

int main(int argc, char * args[]) {
  ::baidu::common::SetLogLevel(DEBUG);
  ::google::SetUsageMessage(kDosCeUsage);
  std::string bin(args[0]);
  if (argc > 1 && strcmp(args[1], "version") ==0) {
    PrintVersion();
    return 0;
  }
  std::map<std::string, Handle> daemon_map;
  daemon_map.insert(std::make_pair("initd", boost::bind(&StartInitd)));
  daemon_map.insert(std::make_pair("engine", boost::bind(&StartEngine)));
  daemon_map.insert(std::make_pair("master", boost::bind(&StartMaster)));
  daemon_map.insert(std::make_pair("scheduler", boost::bind(&StartScheduler)));
  daemon_map.insert(std::make_pair("let", boost::bind(&StartAgent)));
  daemon_map.insert(std::make_pair("dsh", boost::bind(&StartDsh)));
  ::google::ParseCommandLineFlags(&argc, &args, true);
  std::map<std::string, Handle>::iterator h_it = daemon_map.begin();
  bool find_daemon = false;
  for (; h_it != daemon_map.end(); ++h_it) {
    if (EndWiths(bin, h_it->first)) {
      fprintf(stdout, "start daemon %s\n", h_it->first.c_str());
      h_it->second();
      find_daemon = true;
    } 
  }
  if (find_daemon) {
    return 0;
  }
  if (argc < 3) {
    fprintf(stderr,"%s", kDosCeUsage.c_str());
    return -1;
  }
  std::string action(args[1]);
  std::string object(args[2]);
  std::string key = action + object;
  std::map<std::string, Handle> action_map;
  action_map.insert(std::make_pair("addjob", boost::bind(&AddJob)));
  action_map.insert(std::make_pair("getjob", boost::bind(&GetJob)));
  std::map<std::string, Handle>::iterator action_it = action_map.find(key);
  if (action_it != action_map.end()) {
    action_it->second();
  } else {
    fprintf(stderr, "no handle with key %s \n", key.c_str());
  }

	/*if (strcmp(args[1], "run") == 0) {
    Run();
  } else if (strcmp(args[1], "ps") == 0) {
    Show();
  } else if (strcmp(args[1], "log") == 0) {
    ShowLog();
  } else if (strcmp(args[1], "submit") == 0) {
    SubmitJob();
  } else if (strcmp(args[1], "jail") == 0) {
    JailContainer();
  } else if (strcmp(args[1], "job") == 0) {
    ShowJob();
  } else {
    fprintf(stderr,"%s", kDosCeUsage.c_str());
    return -1;
  }*/
  return 0;
}
