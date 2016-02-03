#!/usr/bin/env sh
SANDBOX_DIR=`pwd`
#cd dfs && nohup ../../dos_ce daemon --ce_bin_path=/vagrant/dos/dos_ce >log 2>&1 &
mkdir -p work_dir
mkdir -p gc_dir
echo "--ce_work_dir=$SANDBOX_DIR/work_dir" > dos.flags
 

