### 优化
- 按照边权排序
- 稀疏图缩点
- 反向queue改成数组
- pointNum改成局部变量
- likely unlikely
- uint16
- 稀疏图稠密图 if触发条件和顺序
- stack放到pq.pop()那里
- 改指针访存优化
- count可以用ushort
- 稀疏图，稠密图。Node{dis,count}
- JobTask那里可以先判断能不能跳过，这样就可以少锁几次