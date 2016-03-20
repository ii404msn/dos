mkdir -p cores
chmod a+rwx cores
WORK_DIR=`pwd`
echo "$WORK_DIR/cores/core.%e.%p.%h.%t" > /proc/sys/kernel/core_pattern
mkdir -p $WORK_DIR/work_dir
ulimit -c unlimited
rm -rf core dos
cp ../dos dos
ln -s -f ./dos master
ln -s -f ./dos scheduler
ln -s -f ./dos doslet
ln -s -f ./dos engine
ln -s -f ./dos initd
ln -s -f ./dos dsh
cp -rf ./dos /bin/dsh
sh stop_all.sh
sh clean_work_dir.sh
echo "start engine"
nohup ./engine --ce_process_default_user=root --ce_bin_path=`pwd`/initd --ce_work_dir=`pwd`/work_dir --ce_gc_dir=`pwd`/gc_dir > engine.log 2>&1 &
sleep 3
nohup ./master >master.log 2>&1 &
sleep 3
nohup ./doslet --agent_port_range_end=7000 --agent_port_range_start=4000>let.log 2>&1 &
nohup ./scheduler >scheduler.log 2>&1 &
