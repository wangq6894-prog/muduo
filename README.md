# muduo

大二暑假照着 muduo 的思路手写的 C++ 网络库，从 socket、epoll 的封装一层层往上搭到 Reactor、多线程 TCP 服务器，最后在上面跑了个 HTTP 服务，没依赖任何现成网络库。一共两个头文件，框架约 1200 行，HTTP 约 500 行。

主要几块：Poller 封 epoll，Channel 管 fd 和事件回调，EventLoop 跑循环（跨线程用 eventfd 唤醒）；Buffer 自己写，空闲超时用六十格时间轮管，LoopThreadPool 轮询分发新连接；HTTP 用分阶段状态机解析请求，正则路由，静态文件带目录穿越检查。

## 跑起来

只要 g++。`source/echo`、`source/http` 各自 make 后跑 `main`：Echo 用 `nc localhost 8500` 测，HTTP 浏览器开 8085。

阿里云服务器100ubantu22.04.05二核四G并发 QPS2670