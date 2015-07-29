linux下Ｃ语言utils :
包括：线程封装，字符串操作，线程私有数据封装，一些非线程安全函数的重写，通用链表(带锁，不带锁)，taskqueue,socket封装(支持ipv4,ipv6),logger 引擎，配置文件解析引擎，锁封装(互斥锁，读写锁),时间，数据库引擎，引用计数机制大对象，容器，哈希，io管理，调度器，状态机(fsm),tcp engine，cli(控制台接口，基于unix domain socket )，Unit Test Framework.
以上工具为服务器开发中不可或缺的内容，利用这些工具可以使我们专注于自己的业务实现。
此工具链构建在gnu build tools基础上，符合gun标准。