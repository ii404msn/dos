test -d /dproc && rm -rf /dproc
ps -ef | grep engine | grep -v grep | awk '{print $2}' | while read line; do kill -9 $line;done
ps -ef | grep doslet | grep -v grep | awk '{print $2}' | while read line; do kill -9 $line;done
ps -ef | grep initd | grep -v grep | awk '{print $2}' | while read line; do kill -9 $line;done
ps -ef | grep master | grep -v grep | awk '{print $2}' | while read line; do kill -9 $line;done
ps -ef | grep scheduler | grep -v grep | awk '{print $2}' | while read line; do kill -9 $line;done
