### dos build status
[![Build Status](https://travis-ci.org/imotai/dos.svg?branch=master)](https://travis-ci.org/imotai/dos)

### compile dos

```
sh build.sh
```

### experience dos container engine

```
cd sandbox && sh start_all.sh
# image fetcher is the system level container for download image and uncompress it
../dos ps
 -  name           type     state              rtime   btime
----------------------------------------------------------------
 1  image_fetcher  kSystem  kContainerRunning  3.000s  4.000s

../dos run -u http://idcos.io/dfs.tar.gz -n dfs
 -  name           type     state              rtime    btime
------------------------------------------------------------------
 1  dfs            kOci     kContainerRunning  14.000s  18.000s
 2  image_fetcher  kSystem  kContainerRunning  1.283m   4.000s

../dos log -n dfs
2016-01-24 14:01:48 dfs change state from kContainerRunning to kContainerRunning with msg start user process ok
2016-01-24 14:01:47 dfs change state from kContainerBooting to kContainerRunning with msg start initd ok
2016-01-24 14:01:43 dfs change state from kContainerPulling to kContainerBooting with msg pull image ok
```
