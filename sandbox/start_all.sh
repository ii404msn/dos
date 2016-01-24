cp ../dos_ce .
sh stop_all.sh
nohup ./dos_ce daemon --ce_bin_path=`pwd`/dos_ce --ce_work_dir=`pwd` >log 2>&1 &
#sleep 5
#./dos_ce --ce_endpoint=127.0.0.1:7676 ps 
#sleep 5
#./dos_ce --ce_endpoint=127.0.0.1:7676 run -u http://idcos.io/dfs.tar.gz -n dfs

#sleep 10
#./dos_ce --ce_endpoint=127.0.0.1:7676 ps



