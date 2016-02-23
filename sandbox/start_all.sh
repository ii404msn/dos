rm -rf core dos
cp ../dos dos
sh stop_all.sh
sh clean_work_dir.sh
echo "start engine"
nohup ./dos engine --ce_process_default_user=root --ce_bin_path=`pwd`/dos --ce_work_dir=`pwd`/work_dir --ce_gc_dir=`pwd`/gc_dir > engine.log 2>&1 &
sleep 3
nohup ../dos master >master.log 2>&1 &
sleep 3
nohup ../dos let --agent_port_range_end=7000 --agent_port_range_start=4000>let.log 2>&1 &
nohup ../dos scheduler >scheduler.log 2>&1 &
