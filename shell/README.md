## dsh
distribution shell for dos, 下面文档将按照运维流程讲解

### 展示所有的容器

```
ps -ac

id                             endpoint           state               rtime  btime  cpu(u/l)    mem(u/l)   restart    health
----------------------------------------------------------------------------------------------------------------------------
0_container.0_pod.redis        127.0.0.1:{6379}   kContainerRunning   12h    1m      2/3        1G/3G      0          Good
```

容器展示的属性项

* id/name 
* endpoint 访问地址， 包含用户设置的访问端口
* state 容器当前状态，
* rtime 运行时间自从上次启动/重启
* btime 容器启动时间，包含下载rootfs和启动initd
* cpu cpu当前使用和最大使用limit
* mem 当前内存使用量，包含rss 和 cache
* restart 包含容器在一个节点上面的重启次数
* health 展示容器健康程度，这个是级别属性，影响它的包含运行时间，重启次数，通过health级别能够快速grep出不健康的container

### 获取似乎有问题的容器

```
ps -ac | grep UnHealthy

id                             endpoint           state               rtime  btime  cpu(u/l)    mem(u/l)   restart    health
-------------------------------------------------------------------------------------------------------------------------------
1_container.3_pod.redis        127.0.0.1:{6379}   kContainerRunning   1m    1m      2/3        1G/3G        20        UnHealthy
```

从上面可以看出，容器刚重启，而且重启次数达20次

### 查看一个容器的事件日志

```
log -a 1_container.3_pod.redis

2016-02-24 23:19:45 1_container.3_pod.redis change state from kContainerPulling to kContainerPulling with msg send fetch cmd to inid successfully
2016-02-24 23:28:43 1_container.3_pod.redis change state from kContainerPulling to kContainerBooting with msg pull image ok
2016-02-24 23:28:47 1_container.3_pod.redis change state from kContainerBooting to kContainerRunning with msg start initd ok
.......
2016-02-24 23:31:48 1_container.3_pod.redis change state from kContainerRunning to kContainerError with msg oom 
.......
2016-02-24 23:40:47 1_container.3_pod.redis change state from kContainerRunning to kContainerError with msg receive signal term, seems coredump
.......
2016-02-24 23:51:44 1_container.3_pod.redis change state from kContainerRunning to kContainerRunning with msg start user process ok
.......
2016-02-24 23:59:30 1_container.3_pod.redis change state from kContainerRunning to kContainerRunning with msg start user process ok
```
数据是造假出来的~，只是为了方便展示相关功能，从日志看起，容器在运行过程中出现过 *oom* *coredump*



