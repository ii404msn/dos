#include <gflags/gflags.h>

DEFINE_string(ins_servers, "", "server list of nexus, e.g abc.com:1234,def.com:5342");
DEFINE_string(my_ip, "", "the ip of host");
DEFINE_string(master_port, "9527", "the port that master listen");
DEFINE_string(dos_root_path, "/dos", "the root path of dos on nexus");
DEFINE_string(master_lock_path, "/master_lock", "the lock path of master on nexus");
DEFINE_string(master_endpoint, "/master_endpoint", "the endpoint of master on nexus");
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
DEFINE_int32(scheduler_feasibility_factor, 3, "the factor of scheduler choosing feasibile agent count");
DEFINE_int32(scheduler_max_pod_count, 20, "the max pod count on agent");
DEFINE_double(scheduler_score_longrun_pod_factor, 20.0, "the long run pod factor for scoring agent");
DEFINE_double(scheduler_score_pod_factor, 10.0, "the pod factor for scoring agent");
DEFINE_double(scheduler_score_cpu_factor, 10.0, "the cpu factor for scoring agent");
DEFINE_double(scheduler_score_memory_factor, 10.0, "the memory factor for scoring agent");


DEFINE_string(ce_cgroup_root, "/cgroups", "the root path of cgroup");
// the name used by initd for calculate cgroup path 
DEFINE_string(ce_container_name, "", "the name of container for booting initd");
DEFINE_string(ce_initd_cgroup_root, "cgroups", "the root path of cgroup in initd");
// enable isolator
DEFINE_string(ce_isolators, "cpu,memory,io", "the isolators that are enabled");
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
DEFINE_int32(ce_resource_collect_interval, 6000, "the interval of collecting resource");
// the max times that try to connect to initd, when reaching the times, container will change
// it's state from kContainerBooting to kContainerError
DEFINE_int32(ce_initd_boot_check_max_times, 5, "the max times that try to connect initd ");
// the interval to check initd whether it has been booted successfully
DEFINE_int32(ce_initd_boot_check_interval, 4000, "the interval of check initd boot");
DEFINE_int32(ce_process_status_check_interval, 2000, "the interval of check process status");
DEFINE_int32(ce_container_log_max_size, 100, "the max size of container logs");
DEFINE_int32(ce_initd_process_wait_interval, 1000, "the interval of initd to wait process status to change");
