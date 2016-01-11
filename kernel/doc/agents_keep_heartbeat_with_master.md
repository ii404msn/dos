### dos 系统 agent与master心跳保持设计

#### 心跳解决问题
  和其他分布式系统一样，dos 系统master 通过心跳来判断agent健康状态，心跳正常agent则正常，反之agent将被master设置为kDead状态。


#### 心跳设计

* agent 配置一个心跳周期定时调用master rpc接口，dos实现方式是
```
//通过线程池添加定时任务，然后在回调函数中继续添加定时任务
//HeartBeatCallback会添加这样一段代码
thread_pool.DelayTask(FLAGS_agent_heartbeat_interval, boost::bind(&AgentImpl::HeartBeat, this))
```

* master 检测agent心跳超时逻辑，master也是使用线程池，当agent心跳过来，master会向线程池添加一个定时任务并且记录task_id,当agent 心跳再次过来时取消任务,并在添加定时任务



