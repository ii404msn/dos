rm -rf core dos
cp ../dos dos
ln -s -f ./dos master
ln -s -f ./dos scheduler
ln -s -f ./dos let
ln -s -f ./dos engine
ln -s -f ./dos initd
sh stop_all.sh
sh clean_work_dir.sh
echo "start engine"
nohup ./engine --ce_process_default_user=root --ce_bin_path=`pwd`/initd --ce_work_dir=`pwd`/work_dir --ce_gc_dir=`pwd`/gc_dir > engine.log 2>&1 &
sleep 3
nohup ./master >master.log 2>&1 &
sleep 3
nohup ./let --agent_port_range_end=7000 --agent_port_range_start=4000>let.log 2>&1 &
nohup ./scheduler >scheduler.log 2>&1 &
