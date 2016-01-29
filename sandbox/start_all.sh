cp ../dos .
sh stop_all.sh
nohup ./dos let >let.log 2>&1 &
nohup ./dos master >master.log 2>&1 &
#nohup ./dos engine --ce_bin_path=`pwd`/dos --ce_work_dir=`pwd` >log 2>&1 &
#sleep 5
#./dos_ce --ce_endpoint=127.0.0.1:7676 ps 
#sleep 5
#./dos_ce --ce_endpoint=127.0.0.1:7676 run -u http://idcos.io/dfs.tar.gz -n dfs

#sleep 10
#./dos_ce --ce_endpoint=127.0.0.1:7676 ps



