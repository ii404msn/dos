#include <gflags/gflags.h>

DEFINE_string(nexus_servers, "", "server list of nexus, e.g abc.com:1234,def.com:5342");

DEFINE_string(master_port, "9527", "the port that master listen");
DEFINE_string(master_root_path, "/dos", "the root path of master on nexus");
DEFINE_string(master_lock_path, "/lock", "the lock path of master on nexus");
DEFINE_string(master_node_path_prefix, "/nodes", "the node prefix path of master on nexus");

DEFINE_string(agent_port, "8527", "the port that agent listen");
// the time to check agent whether it's timeout
DEFINE_int32(agent_heart_beat_timeout, 10000, "the timeout of agent heart beat");
// the interval to send heart beat to master , it should be less than agent_heart_beat_timeout
DEFINE_int32(agent_heart_beat_interval, 2000, "the interval of agent heart beat");

DEFINE_string(ce_initd_port, "9527", "initd listen port");
DEFINE_string(ce_initd_conf_path, "runtime.json", "the default runtime path");
DEFINE_bool(ce_enable_ns, true, "enable linux namespace");
DEFINE_string(ce_bin_path,"./dos_ce","the path of dos ce");
// the max times that try to connect to initd, when reaching the times, container will change
// it's state from kContainerBooting to kContainerError
DEFINE_int32(ce_initd_boot_check_max_times, 5, "the max times that try to connect initd ");
// the interval to check initd whether it has been booted successfully
DEFINE_int32(ce_initd_boot_check_interval, 2000, "the path of dos ce");


