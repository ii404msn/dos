## dos 隔离设计

### 支持隔离列表

* cpu
* memory
* disk io
* network

### cpu 隔离器
cpu 隔离要实现一下目标
* 资源隔离
* 资源动态伸缩
* 资源监控

前面两点可以通过cgroup 软限实现，但是软限在高版本上内核是否ok需要验证，第3点资源监控，定时采集cpu
使用情况，然后存储

