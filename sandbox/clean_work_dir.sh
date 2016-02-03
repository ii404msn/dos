dirs=`ls -l work_dir | awk '{print $NF}'`
for dir in $dirs
do
   if [ -d work_dir/$dir ]
   then
	  echo "clean dir $dir"
      	  cd work_dir/$dir
	  umount rootfs/proc >/dev/null
	  umount rootfs/sys >/dev/null
	  umount rootfs/dev/pts >/dev/null
	  umount rootfs/dev/shm >/dev/null
	  umount rootfs/dev/mqueue >/dev/null
	  umount rootfs/dev >/dev/null
	  rm -rf rootfs/dev/*
	  cd -
          rm -rf work_dir/$dir 
   fi
done
