#include <gflags/gflags.h>

DEFINE_string(nexus_servers, "", "server list of nexus, e.g abc.com:1234,def.com:5342");

DEFINE_string(master_port, "9527", "the port that master listen");
DEFINE_string(master_root_path, "/dos", "the root path of master on nexus");
DEFINE_string(master_lock_path, "/lock", "the lock path of master on nexus");
DEFINE_string(master_node_path_prefix, "/nodes", "the node prefix path of master on nexus");

DEFINE_int32(agent_heart_beat_timeout, 10000, "the timeout of agent heart beat");
DEFINE_int32(agent_heart_beat_interval, 2000, "the interval of agent heart beat");
