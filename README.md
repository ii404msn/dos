### DOS
一个简单的分布式操作系统，面向小的团队，机器（或者虚拟机）规模在5~100台规模的项目，比如通过dos一键在云服务商（aws,gce,阿里云），初始化好5~100虚拟机实例，一键部署你的任务实例（jenkins持续集成环境，webserver, tomcat等service服务，以及一些批处理任务）；通过合理的调度算法，隔离手段，naming,实现对资源充分利用

### 构建状态
[![Build Status](https://travis-ci.org/imotai/dos.svg?branch=master)](https://travis-ci.org/imotai/dos)

### 编译DOS

```
sh build.sh
```

### 体验 dos的容器引擎

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

### 感兴趣的可以随时提pr
