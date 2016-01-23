#!/usr/bin/env sh
SANDBOX_DIR=`pwd`
killall dos_ce
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
cd dfs && ../../dos_ce daemon --ce_bin_path=/vagrant/dos/dos_ce 

