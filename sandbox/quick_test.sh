#!/usr/bin/env sh
mkdir -p cores
chmod a+rwx cores
WORK_DIR=`pwd`
echo "$WORK_DIR/cores/core.%e.%p.%h.%t" > /proc/sys/kernel/core_pattern
ulimit -c unlimited
mkdir -p work_dir
mkdir -p gc_dir
hn=`hostname`
echo "--ins_servers=$hn:8868,$hn:8869,$hn:8870,$hn:8871,$hn:8872" > dos.flags
echo "--ce_work_dir=$WORK_DIR/work_dir" >> dos.flags
echo "--ce_process_default_user=root" >> dos.flags
echo "--ce_bin_path=$WORK_DIR/initd" >> dos.flags
echo "--ce_work_dir=$WORK_DIR/work_dir" >> dos.flags
echo "--ce_gc_dir=$WORK_DIR/gc_dir" >> dos.flags
echo "--agent_port_range_end=7000" >> dos.flags
echo "--agent_port_range_start=4000" >> dos.flags
echo "prepare dos.flags done"

rm -rf core dos
ln -s -f ../dos master
ln -s -f ../dos scheduler
ln -s -f ../dos doslet
ln -s -f ../dos engine
ln -s -f ../dos initd
ln -s -f ../dos dsh
cp -rf ../dos /bin/dsh

sh start_all.sh


