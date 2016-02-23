CGROUP_ROOT=/cgroups
mkdir -p $CGROUP_ROOT/cpu && mount -t cgroup -ocpu none $CGROUP_ROOT/cpu >/dev/null 2>&1
mkdir -p $CGROUP_ROOT/memory && mount -t cgroup -omemory none $CGROUP_ROOT/memory >/dev/null 2>&1
mkdir -p $CGROUP_ROOT/cpuacct && mount -t cgroup -ocpuacct none $CGROUP_ROOT/cpuacct >/dev/null 2>&1
mkdir -p $CGROUP_ROOT/freezer && mount -t cgroup -ofreezer none $CGROUP_ROOT/freezer >/dev/null 2>&1
mkdir -p $CGROUP_ROOT/blkio && mount -t cgroup -oblkio none $CGROUP_ROOT/blkio >/dev/null 2>&1
