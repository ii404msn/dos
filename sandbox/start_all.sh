rm -rf core dos
cp ../dos dos
sh stop_all.sh
sh clean_work_dir.sh
echo "start engine"
nohup ./dos engine --ce_bin_path=`pwd`/dos --ce_work_dir=`pwd`/work_dir --ce_gc_dir=`pwd`/gc_dir > engine.log 2>&1 &
sleep 3
nohup ../dos master >master.log 2>&1 &
sleep 3
nohup ../dos let >let.log 2>&1 &
nohup ../dos scheduler >scheduler.log 2>&1 &
sleep 2
#../dos submit  -u http://idcos.io/dfs.tar.gz -n dfs -r 5 -d 2
#sleep 1
#../thirdparty/bin/python ../case/test_sched.py
#sleep 5
#./dos_ce --ce_endpoint=127.0.0.1:7676 run -u http://idcos.io/dfs.tar.gz -n dfs

#sleep 10
#./dos_ce --ce_endpoint=127.0.0.1:7676 ps



