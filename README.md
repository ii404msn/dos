### dos build status
[![Build Status](https://travis-ci.org/imotai/dos.svg?branch=master)](https://travis-ci.org/imotai/dos)

### compile dos

```
sh build.sh
```

### experience dos container engine

```
cd sandbox && sh start_all.sh
../dos_ce --ce_endpoint=127.0.0.1:7676 ps
-  name           type     state              rtime
---------------------------------------------------------
1  image_fetcher  kSystem  kContainerRunning  40.000s

../dos_ce --ce_endpoint=127.0.0.1:7676 run -u http://idcos.io/dfs.tar.gz -n dfs
-  name           type     state              rtime
---------------------------------------------------------
1  dfs            kOci     kContainerPulling  -
2  image_fetcher  kSystem  kContainerRunning  57.000s
```
