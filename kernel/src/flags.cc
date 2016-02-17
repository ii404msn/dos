#include <gflags/gflags.h>

DEFINE_string(nexus_servers, "", "server list of nexus, e.g abc.com:1234,def.com:5342");

DEFINE_string(master_port, "9527", "the port that master listen");
DEFINE_string(master_root_path, "/dos", "the root path of master on nexus");
DEFINE_string(master_lock_path, "/lock", "the lock path of master on nexus");
DEFINE_string(master_node_path_prefix, "/nodes", "the node prefix path of master on nexus");

DEFINE_string(agent_endpoint, "127.0.0.1:8527", "the endpoint of agent");
// the time to check agent whether it's timeout
DEFINE_int32(agent_heart_beat_timeout, 10000, "the timeout of agent heart beat");
// the interval to send heart beat to master , it should be less than agent_heart_beat_timeout
DEFINE_int32(agent_heart_beat_interval, 2000, "the interval of agent heart beat");
DEFINE_double(agent_cpu_rate, 0.7, "the cpu rate for sharing agent cpu");
DEFINE_double(agent_memory_rate, 0.7, "the memory rate for sharing agent memory");
DEFINE_int32(agent_port_range_start, 4000, "the port start range for agent");
DEFINE_int32(agent_port_range_end, 6000, "the port end range for agent");
DEFINE_int32(agent_sync_container_stat_interval, 1000, "the interval for agent sync container stat");
DEFINE_int32(scheduler_sync_agent_info_interval, 2000, "the interval of scheduler sync agent info from master");

DEFINE_string(ce_process_default_user, "dos", "launch process with default user");
DEFINE_string(ce_port, "7676", "dos container engine listen port");
DEFINE_string(ce_initd_port, "9527", "initd listen port");
DEFINE_string(ce_initd_conf_path, "runtime.json", "the default runtime path");
DEFINE_bool(ce_enable_ns, true, "enable linux namespace");
DEFINE_string(ce_bin_path,"./dos","the path of dos container engine");
DEFINE_string(ce_gc_dir,"./gc_dir","the gc path of dos ce");
DEFINE_string(ce_work_dir,"./work_dir","the work path of dos ce");
DEFINE_string(ce_image_fetcher_name, "image_fetcher", "the name of image fetcher");
DEFINE_int32(ce_image_fetch_status_check_interval, 2000, "the interval of checking download image");
// the max times that try to connect to initd, when reaching the times, container will change
// it's state from kContainerBooting to kContainerError
DEFINE_int32(ce_initd_boot_check_max_times, 5, "the max times that try to connect initd ");
// the interval to check initd whether it has been booted successfully
DEFINE_int32(ce_initd_boot_check_interval, 4000, "the interval of check initd boot");
DEFINE_int32(ce_process_status_check_interval, 2000, "the interval of check process status");
DEFINE_int32(ce_container_log_max_size, 100, "the max size of container logs");
DEFINE_int32(ce_initd_process_wait_interval, 1000, "the interval of initd to wait process status to change");
