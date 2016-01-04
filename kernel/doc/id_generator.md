## id 生成器

### 生成器必须满足

* 高效
* 可读性高

### jobid生成策略
参考namespace设计原则，当一个user创建一个名称为test的job时，id则可以为user/test_2016_0101
及 {user}/{jobname}_timestamp

### podid生成策略
{user}/{jobname}_timestamp/{offset}

