## dos中对象生命周期

### dos中对象列表

* job
* pod
* container
* workspace

### 生命周期

在dos中对象生命周期非常重要，如果周期和实际生产环境不匹配，dos可能会对业务进程造成重大影响

* job的生命周期，很简单从创建到删除
* pod的生命周期，除去job 对它的影响，pod的生命周期基本是和agent生命周期一致，及当容器出现问题时，如果重启策略甚至为always，pod是不会做迁移操作的
* container的生命周期，它是和业务进程绑定的，因为创建一个容器的代价很小
* workspace的生命周期，它不一定和container生命周期一致，它可以和container的版本生命周期一致，比如一个容器调度在一台以前部署过的机器，并且workspace版本号和容器匹配，那么就可以继续复用workspace, workspace服用能够减少io的消耗

