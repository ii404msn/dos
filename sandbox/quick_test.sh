#!/usr/bin/env sh
SANDBOX_DIR=`pwd`
if [ -d "dfs" ]
then
  cd dfs
  umount rootfs/proc
  umount rootfs/sys
  umount rootfs/dev/pts
  umount rootfs/dev/shm
  umount rootfs/dev/mqueue
  umount rootfs/dev
  rm -rf rootfs/dev/*
  cd -
fi

mkdir -p dfs/rootfs

tar -zxvf rootfs.tar.gz -C dfs/rootfs
cp runtime.json dfs/
cp config.json dfs/
#../../initd -v
#cd testfolder && nohup ../../initd --runtime_conf_path=$SANDBOX_DIR/testfolder/runtime.json >log 2>&1 &
#sleep 2
#cd $SANDBOX_DIR/testfolder && cat log


