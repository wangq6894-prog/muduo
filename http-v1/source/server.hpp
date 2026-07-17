#ifndef __M_SERVER_H__
#define __M_SERVER_H__
#include <signal.h>
#include <arpa/inet.h>
#include <cassert>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <netinet/in.h>
#include <pthread.h>
#include <string>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <thread>
#include <time.h>
#include <typeinfo>
#include <unistd.h>
#include <unordered_map>
#include <vector>
#define DEFAULT_TIMEOUT 30
#define INF 0
#define DBG 1
#define ERR 2
#define LOG_LEVEL DBG

#define LOG(level, format, ...)                                                \
  do {                                                                         \
    if (level < LOG_LEVEL)                                                     \
      break;                                                                   \
    time_t t = time(NULL);                                                     \
    struct tm *ltm = localtime(&t);                                            \
    char tmp[32] = {0};                                                        \
    strftime(tmp, 31, "%H:%M:%S", ltm);                                        \
    fprintf(stdout, "[%p %s %s:%d] " format "\n", (void *)pthread_self(), tmp, \
            __FILE__, __LINE__, ##__VA_ARGS__);                                \
  } while (0)

#define INF_LOG(format, ...) LOG(INF, format, ##__VA_ARGS__)
#define DBG_LOG(format, ...) LOG(DBG, format, ##__VA_ARGS__)
#define ERR_LOG(format, ...) LOG(ERR, format, ##__VA_ARGS__)

#define BUFFER_DEFAULT_SIZE 1024
class Buffer {
private:
  std::vector<char> _buffer;
  uint64_t _reader_idx; //读偏移量
  uint64_t _writer_idx; //写偏移量
public:
  Buffer() : _buffer(BUFFER_DEFAULT_SIZE), _reader_idx(0), _writer_idx(0) {}
  ~Buffer(){};
  char *Begin() { return &*_buffer.begin(); };
  //获取当前写入起始地址
  char *WritePosition() {
    //_buffer的空间起始地址加偏移量
    return Begin() + _writer_idx;
  };
  //获取当前读取起始地址
  char *ReadPosition() {
    //_buffer的空间起始地址加偏移量
    return Begin() + _reader_idx;
  };
  //获取缓冲区起始空闲空间大小--写偏移之后的空闲空间
  uint64_t HeadIdleSize() { return _reader_idx; };
  //获取缓冲区末尾空闲空间大小--读偏移之后的空闲空间
  uint64_t TailIdleSize() { return _buffer.size() - _writer_idx; };
  //获取可读数据大小--写偏移减去读偏移
  uint64_t ReadAbleSize() { return _writer_idx - _reader_idx; };
  //获取可写数据大小--缓冲区大小减去写偏移
  uint64_t WriteAbleSize() { return _buffer.size() - _writer_idx; };
  //将读偏移向后移动
  void MoveReadOffset(uint64_t len) {
    if (len == 0)
      return;
    //向后移动的大小必须小于可读数据大小
    assert(len <= ReadAbleSize());
    _reader_idx += len;
  };
  //将写偏移向后移动
  void MoveWriteOffset(uint64_t len) {
    //向后移动的大小必须小于可写数据大小
    assert(len <= TailIdleSize());
    _writer_idx += len;
  };
  //确保可写空间足够(整体空闲空间够了就移动数据，否则就扩容)
  void EnsureWriteSpace(uint64_t len) {
    if (len <= TailIdleSize()) {
      return;
    }
    if (len <= HeadIdleSize() + TailIdleSize()) {
      uint64_t rsz = ReadAbleSize();
      std::copy(ReadPosition(), ReadPosition() + rsz, Begin());
      _reader_idx = 0;
      _writer_idx = rsz;
    } else {
      //总容量不够，进行扩容
      _buffer.resize(_writer_idx + len);
    }
  };
  //写入数据
  void Write(const void *data, uint64_t len) {
    // 1.保证有足够空间2.拷贝数据进去
    EnsureWriteSpace(len);
    const char *d = (const char *)data;
    std::copy(d, d + len, WritePosition());
  };
  void WriteAndPush(const void *data, uint64_t len) {
    Write(data, len);
    MoveWriteOffset(len);
  };
  void WriteString(const std::string &data) {
    Write(data.c_str(), data.size());
  };
  void WriteStringAndPush(const std::string &data) {
    WriteString(data);
    MoveWriteOffset(data.size());
  };
  void WriteBuffer(Buffer &data) {
    Write(data.ReadPosition(), data.ReadAbleSize());
  };
  void WriteBufferAndPush(Buffer &data) {
    WriteBuffer(data);
    MoveWriteOffset(data.ReadAbleSize());
  };
  //读取数据
  void Read(void *buf, uint64_t len) {
    // 1.获取的数据大小小于可读数据大小
    assert(len <= ReadAbleSize());
    std::copy(ReadPosition(), ReadPosition() + len, (char *)buf);
  };
  std::string ReadAsString(uint64_t len) {
    assert(len <= ReadAbleSize());
    std::string str;
    str.resize(len);
    Read(&str[0], len);
    return str;
  };
  std::string ReadAsStringAndPop(uint64_t len) {
    std::string str = ReadAsString(len);
    MoveReadOffset(len);
    return str;
  };
  char *FindCRLF() {
    void *res = memchr(ReadPosition(), '\n', ReadAbleSize());
    if (res == nullptr) {
      return nullptr;
    }
    return (char *)res;
  };
  void PopCRLF() { MoveReadOffset(2); };
  //通常获取一行数据都是针对http
  std::string GetLine() {
    char *pos = FindCRLF();
    if (pos == nullptr) {
      //没有换行
      return "";
    }
    //有换行
    return ReadAsString(pos - ReadPosition() + 1); //+1是为了把换行取出来
  };
  std::string GetLineAndPop() {
    std::string str = GetLine();
    MoveReadOffset(str.size());
    return str;
  };
  //清空缓冲区
  void Clear() {
    //将偏移量归零
    _reader_idx = 0;
    _writer_idx = 0;
  };
};
#define Max_Listen 1024
class Socket {
private:
  int _sockfd;

public:
  Socket() : _sockfd(-1){};
  Socket(int fd) : _sockfd(fd) {}
  ~Socket() { Close(); };
  int Fd() { return _sockfd; };
  bool Create() {
    // int socket(int domain, int type, int protocol);
    // domain:协议族
    // type:协议类型
    // protocol:协议
    _sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (_sockfd == -1) {
      ERR_LOG("socket failed");
      return false;
    }
    return true;
  };
  bool Bind(const std::string &ip, uint64_t port) {
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip.c_str());
    addr.sin_port = htons(port);
    socklen_t len = sizeof(struct sockaddr_in);
    int ret = bind(_sockfd, (struct sockaddr *)&addr, len);
    if (ret == -1) {
      ERR_LOG("bind failed");
      return false;
    }
    return true;
  };
  bool Listen(int backlog = Max_Listen) {
    // int listen(int sockfd, int backlog);
    int ret = listen(_sockfd, backlog);
    if (ret == -1) {
      ERR_LOG("listen failed");
      return false;
    }
    return true;
  }; // backlog是监听队列的大小，也是最大连接数
  bool Connect(const std::string &ip, uint64_t port) {
    // int connect(int sockfd,struct sockaddr* addr,socklen_t addrlen);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip.c_str());
    addr.sin_port = htons(port);
    socklen_t len = sizeof(struct sockaddr_in);
    int ret = connect(_sockfd, (struct sockaddr *)&addr, len);
    if (ret == -1) {
      ERR_LOG("connect failed");
      return false;
    }
    return true;
  };
  int Accept() {
    // int accept(int sockfd,struct sockaddr* addr,socklen_t* addrlen);
    struct sockaddr_in addr;
    socklen_t len = sizeof(struct sockaddr_in);
    int ret = accept(_sockfd, (struct sockaddr *)&addr, &len);
    if (ret == -1) {
      ERR_LOG("accept failed");
      return -1;
    }
    return ret;
  };
  ssize_t Recv(void *buf, size_t len, int flag = 0) {
    // ssize_t recv(int sockfd, void *buf, size_t len, int flags);
    //  sockfd:套接字描述符
    //  buf:接收缓冲区
    //  len:接收缓冲区大小
    //  flags:接收标志
    ssize_t ret = recv(_sockfd, buf, len, flag);
    if (ret == 0) {
      return 0; //对方关闭连接
    }
    if (ret < 0) {
      // EAGAIN:没有数据可读
      // EINTR:被信号中断
      if (errno == EAGAIN || errno == EINTR) {
        return 0; //没有接收到数据
      }
      ERR_LOG("recv failed");
      return -1;
    }
    return ret;
  };
  ssize_t NoneBlockRecv(void *buf, size_t len) {
    return Recv(buf, len, MSG_DONTWAIT);
  }
  ssize_t Send(const void *buf, size_t len, int flag = 0) {
    // ssize_t send(int sockfd, const void *buf, size_t len, int flags);
    //  sockfd:套接字描述符
    //  buf:发送缓冲区
    //  len:发送缓冲区大小
    //  flags:发送标志
    ssize_t ret = send(_sockfd, buf, len, flag);
    if (ret <= 0) {
      if (errno == EAGAIN || errno == EINTR) {
        return 0; //没有发送完数据
      }
      ERR_LOG("send failed");
      return -1;
    }
    return ret;
  };
  ssize_t NoneBlockSend(const void *buf, size_t len) {
    if (len == 0)
      return 0;
    return Send(buf, len, MSG_DONTWAIT);
  }
  //关闭套接字
  void Close() {
    if (_sockfd != -1) {
      close(_sockfd);
      _sockfd = -1;
    }
  }

  //?????????????????????????????????????????????????????????????????

  //创建一个服务端连接
  bool CreateServer(uint16_t port, const std::string &ip = "0.0.0.0",
                    bool block_flag = false) {
    // 1. 创建套接字，2. 启动地址重用，3. 绑定地址，4. 开始监听，5. 设置非阻塞
    if (Create() == false)
      return false;
    ReuseAddress();
    if (Bind(ip, port) == false)
      return false;
    if (Listen() == false)
      return false;
    if (!block_flag)
      NonBlock();
    return true;
  }
  //创建一个客户端连接
  bool CreateClient(uint16_t port, const std::string &ip) {
    // 1. 创建套接字，2.指向连接服务器
    if (Create() == false)
      return false;
    if (Connect(ip, port) == false)
      return false;
    return true;
  }
  //设置套接字选项---开启地址端口重用
  void ReuseAddress() {
    // int setsockopt(int fd, int leve, int optname, void *val, int vallen)
    int val = 1;
    setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&val, sizeof(int));
    val = 1;
    setsockopt(_sockfd, SOL_SOCKET, SO_REUSEPORT, (void *)&val, sizeof(int));
  }
  //设置套接字阻塞属性-- 设置为非阻塞
  void NonBlock() {
    // int fcntl(int fd, int cmd, ... /* arg */ );
    int flag = fcntl(_sockfd, F_GETFL, 0);
    fcntl(_sockfd, F_SETFL, flag | O_NONBLOCK);
  }
};
class EventLoop;
class Poller;
class Channel {
private:
  int _fd;
  // Poller *_poller;
  EventLoop *_loop;
  uint32_t _events;  // 当前需要监控的事件
  uint32_t _revents; // 当前连接触发的事件
  using EventCallback = std::function<void()>;
  EventCallback _read_callback;  //可读事件被触发的回调函数
  EventCallback _write_callback; //可写事件被触发的回调函数
  EventCallback _error_callback; //错误事件被触发的回调函数
  EventCallback _close_callback; //连接断开事件被触发的回调函数
  EventCallback _event_callback; //任意事件被触发的回调函数
public:
  void Update();
  void Remove();
  Channel(EventLoop *loop, int fd)
      : _fd(fd), _loop(loop), _events(0), _revents(0) {}
  int Fd() { return _fd; }
  uint32_t Events() { return _events; } //获取想要监控的事件
  void SetREvents(uint32_t events) { _revents = events; } //设置实际就绪的事件
  void SetReadCallback(const EventCallback &cb) { _read_callback = cb; }
  void SetWriteCallback(const EventCallback &cb) { _write_callback = cb; }
  void SetErrorCallback(const EventCallback &cb) { _error_callback = cb; }
  void SetCloseCallback(const EventCallback &cb) { _close_callback = cb; }
  void SetEventCallback(const EventCallback &cb) { _event_callback = cb; }
  //当前是否监控了可读
  bool ReadAble() { return (_events & EPOLLIN); }
  //当前是否监控了可写
  bool WriteAble() { return (_events & EPOLLOUT); }
  //启动读事件监控
  void EnableRead() {
    _events |= EPOLLIN;
    Update();
  }
  //启动写事件监控
  void EnableWrite() {
    _events |= EPOLLOUT;
    Update();
  }
  //关闭读事件监控
  void DisableRead() {
    _events &= ~EPOLLIN;
    Update();
  }
  //关闭写事件监控
  void DisableWrite() {
    _events &= ~EPOLLOUT;
    Update();
  }
  //关闭所有事件监控
  void DisableAll() {
    _events = 0;
    Update();
  }
  //事件处理，一旦连接触发了事件，就调用这个函数，自己触发了什么事件如何处理自己决定
  void HandleEvent() {
    // 对方关闭了连接，或者读取到数据，或者有优先数据可读
    if ((_revents & EPOLLIN) || (_revents & EPOLLRDHUP) ||
        (_revents & EPOLLPRI)) {
      //不管任何时间都触发任意事件回调
      if (_event_callback) {
        //触发任意事件回调
        _event_callback();
      }
      //触发可读事件回调
      if (_read_callback)
        _read_callback();
    }

    /*有可能会释放连接的操作事件，一次只处理一个*/
    if (_revents & EPOLLOUT) {
      //触发可写事件
      if (_write_callback)
        _write_callback();
    } else if (_revents & EPOLLERR) {
      //触发错误事件
      if (_error_callback)
        _error_callback(); //一旦出错，就会释放连接，因此要放到前边调用任意回调
    } else if (_revents & EPOLLHUP) {
      //触发连接断开事件
      if (_close_callback)
        _close_callback();
    }
  }
};
#define MAX_EPOLLEVENTS 1024
class Poller {
private:
  int _epfd;
  struct epoll_event _evs[MAX_EPOLLEVENTS];
  std::unordered_map<int, Channel *> _channels;

public:
  Poller() {
    _epfd = epoll_create1(0);
    if (_epfd < 0) {
      ERR_LOG("epoll_create1 failed");
      abort();
    }
  };
  void Update(Channel *channel, int op) {
    int fd = channel->Fd();
    struct epoll_event ev;
    ev.events = channel->Events();
    ev.data.fd = fd;
    int ret = epoll_ctl(_epfd, op, fd, &ev);
    if (ret < 0) {
      ERR_LOG("epoll_ctl failed: fd=%d, op=%d, errno=%d (%s)", fd, op, errno, strerror(errno));
    }
  };
  void UpdateEvent(Channel *channel) {
    bool ret = HasChannel(channel);
    if (ret == false) {
      //不存在则添加
      _channels.insert(std::make_pair(channel->Fd(), channel));
      return Update(channel, EPOLL_CTL_ADD);
    }
    return Update(channel, EPOLL_CTL_MOD);
  };
  bool HasChannel(Channel *channel) {
    auto it = _channels.find(channel->Fd());
    if (it == _channels.end()) {
      return false;
    }
    return true;
  }
  void RemoveEvent(Channel *channel) {
    auto it = _channels.find(channel->Fd());
    if (it == _channels.end()) {
      return;
    }
    _channels.erase(it);
    Update(channel, EPOLL_CTL_DEL);
  };
  void Poll(std::vector<Channel *> *active) {
    // int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int
    // timeout);
    int nfds = epoll_wait(_epfd, _evs, MAX_EPOLLEVENTS, -1);
    if (nfds < 0) {
      if (errno == EINTR) {
        return;
      }
      ERR_LOG("EPOLL WAIT ERROR:%s", strerror(errno));
      abort();
    }
    for (int i = 0; i < nfds; i++) {
      auto it = _channels.find(_evs[i].data.fd);
      assert(it != _channels.end());
      it->second->SetREvents(_evs[i].events);
      active->push_back(it->second);
    }
  }
};
using TaskFunc = std::function<void()>;
using ReleaseFunc = std::function<void()>;
class TimerTask {
private:
  uint64_t _id;         // 定时器任务对象ID
  uint32_t _timeout;    //定时任务的超时时间
  bool _canceled;       // false-表示没有被取消， true-表示被取消
  TaskFunc _task_cb;    //定时器对象要执行的定时任务
  ReleaseFunc _release; //用于删除TimerWheel中保存的定时器对象信息
public:
  TimerTask(uint64_t id, uint32_t delay, const TaskFunc &cb)
      : _id(id), _timeout(delay), _task_cb(cb), _canceled(false) {}
  ~TimerTask() {
    if (_canceled == false)
      _task_cb();
    _release();
  }
  void Cancel() { _canceled = true; }
  void SetRelease(const ReleaseFunc &cb) { _release = cb; }
  uint32_t DelayTime() { return _timeout; }
};

class TimerWheel {
private:
  using WeakTask = std::weak_ptr<TimerTask>;
  using PtrTask = std::shared_ptr<TimerTask>;
  int _tick; //当前的秒针，走到哪里释放哪里，释放哪里，就相当于执行哪里的任务
  int _capacity; //表盘最大数量---其实就是最大延迟时间
  std::vector<std::vector<PtrTask>> _wheel;
  std::unordered_map<uint64_t, WeakTask> _timers;

  EventLoop *_loop;
  int _timerfd; //定时器描述符--可读事件回调就是读取计数器，执行定时任务
  std::unique_ptr<Channel> _timer_channel;

private:
  void RemoveTimer(uint64_t id) {
    auto it = _timers.find(id);
    if (it != _timers.end()) {
      _timers.erase(it);
    }
  }
  static int CreateTimerfd() {
    int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (timerfd < 0) {
      ERR_LOG("TIMERFD CREATE FAILED!");
      abort();
    }
    // int timerfd_settime(int fd, int flags, struct itimerspec *new, struct
    // itimerspec *old);
    struct itimerspec itime;
    itime.it_value.tv_sec = 1;
    itime.it_value.tv_nsec = 0; //第一次超时时间为1s后
    itime.it_interval.tv_sec = 1;
    itime.it_interval.tv_nsec = 0; //第一次超时后，每次超时的间隔时
    timerfd_settime(timerfd, 0, &itime, NULL);
    return timerfd;
  }
  int ReadTimefd() {
    uint64_t times;
    //有可能因为其他描述符的事件处理花费事件比较长，然后在处理定时器描述符事件的时候，有可能就已经超时了很多次
    // read读取到的数据times就是从上一次read之后超时的次数
    int ret = read(_timerfd, &times, 8);
    if (ret < 0) {
      ERR_LOG("READ TIMEFD FAILED!");
      abort();
    }
    return times;
  }
  //这个函数应该每秒钟被执行一次，相当于秒针向后走了一步
  void RunTimerTask() {
    _tick = (_tick + 1) % _capacity;
    _wheel[_tick]
        .clear(); //清空指定位置的数组，就会把数组中保存的所有管理定时器对象的shared_ptr释放掉
  }
  void OnTime() {
    //根据实际超时的次数，执行对应的超时任务
    int times = ReadTimefd();
    for (int i = 0; i < times; i++) {
      RunTimerTask();
    }
  }
  void TimerAddInLoop(uint64_t id, uint32_t delay, const TaskFunc &cb) {
    PtrTask pt(new TimerTask(id, delay, cb));
    pt->SetRelease(std::bind(&TimerWheel::RemoveTimer, this, id));
    int pos = (_tick + delay) % _capacity;
    _wheel[pos].push_back(pt);
    _timers[id] = WeakTask(pt);
  }
  void TimerRefreshInLoop(uint64_t id) {
    //通过保存的定时器对象的weak_ptr构造一个shared_ptr出来，添加到轮子中
    auto it = _timers.find(id);
    if (it == _timers.end()) {
      return; //没找着定时任务，没法刷新，没法延迟
    }
    PtrTask pt =
        it->second.lock(); // lock获取weak_ptr管理的对象对应的shared_ptr
    int delay = pt->DelayTime();
    int pos = (_tick + delay) % _capacity;
    _wheel[pos].push_back(pt);
  }
  void TimerCancelInLoop(uint64_t id) {
    auto it = _timers.find(id);
    if (it == _timers.end()) {
      return; //没找着定时任务，没法刷新，没法延迟
    }
    PtrTask pt = it->second.lock();
    if (pt)
      pt->Cancel();
  }

public:
  TimerWheel(EventLoop *loop)
      : _capacity(60), _tick(0), _wheel(_capacity), _loop(loop),
        _timerfd(CreateTimerfd()),
        _timer_channel(new Channel(_loop, _timerfd)) {
    _timer_channel->SetReadCallback(std::bind(&TimerWheel::OnTime, this));
    _timer_channel->EnableRead(); //启动读事件监控
  }
  /*定时器中有个_timers成员，定时器信息的操作有可能在多线程中进行，因此需要考虑线程安全问题*/
  /*如果不想加锁，那就把对定期的所有操作，都放到一个线程中进行*/
  void TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb);
  //刷新/延迟定时任务
  void TimerRefresh(uint64_t id);
  void TimerCancel(uint64_t id);
  /*这个接口存在线程安全问题--这个接口实际上不能被外界使用者调用，只能在模块内，在对应的EventLoop线程内执行*/
  bool HasTimer(uint64_t id) {
    auto it = _timers.find(id);
    if (it == _timers.end()) {
      return false;
    }
    return true;
  }
};
class EventLoop {
private:
  std::thread::id _thread_id; //线程id
  int _event_fd;              //事件fd
  std::unique_ptr<Channel> _event_channel;
  std::unique_ptr<Poller> _poller;
  using Functor = std::function<void()>;
  std::vector<Functor> _tasks;
  std::mutex _mutex;
  TimerWheel _timer_wheel;

public:
  void RunAllTask() {
    std::vector<Functor> functor;
    {
      std::unique_lock<std::mutex> lock(_mutex);
      _tasks.swap(functor);
    }
    for (auto &f : functor) {
      f();
    }
    return;
  }
  int CreateEventFd() {
    int efd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (efd < 0) {
      ERR_LOG("eventfd failed");
      abort();
    }
    return efd;
  };
  void ReadEventFd() {
    uint64_t res = 0;
    int ret = read(_event_fd, &res, sizeof(res));
    if (ret < 0) {
      if (errno == EINTR || errno == EAGAIN)
        return;
      ERR_LOG("read failed");
      abort();
    }
    return;
  };
  void WakeUpEventFd() {
    uint64_t val = 0;
    int ret = write(_event_fd, &val, sizeof(val));
    if (ret < 0) {
      if (errno == EINTR)
        return;
      ERR_LOG("write failed");
      abort();
    }
    return;
  };
  EventLoop()
      : _thread_id(std::this_thread::get_id()), _event_fd(CreateEventFd()),
        _event_channel(new Channel(this, _event_fd)), _poller(new Poller()),
        _timer_wheel(this) {
    _event_channel->SetReadCallback(std::bind(&EventLoop::ReadEventFd, this));
    _event_channel->EnableRead();
  };
  //事件监控-》就绪事件处理-》执行任务
  void Start() {
    while (1) {
      // 1.事件监控
      std::vector<Channel *> actives;
      _poller->Poll(&actives);
      // 2.事件处理
      for (auto &c : actives) {
        c->HandleEvent();
      }
      // 3.执行任务
      RunAllTask();
    }
  };

  void RunInLoop(Functor cb) {
    if (IsInLoop()) {
      return cb();
    } else {
      return QueueInLoop(cb);
    }
  }; //判断将要执行的任务是否已经处于当前线程中，如已经存在则执行，不存在则压入队列
  void AssertInLoop() { assert(_thread_id == std::this_thread::get_id()); }
  void QueueInLoop(Functor cb) {
    {
      std::unique_lock<std::mutex> lock(_mutex);
      _tasks.push_back(cb);
    }
    //唤醒因为没有事件就绪而导致的epoll阻塞
    WakeUpEventFd();
  };                //将任务压入任务池
  bool IsInLoop() { //判断是否在当前线程是否是EventLoop对应的线程
    return (_thread_id == std::this_thread::get_id());
  };
  void UpdateEvent(Channel *channel) {
    return _poller->UpdateEvent(channel);
  }; //更新事件监控
  void RemoveEvent(Channel *channel) {
    return _poller->RemoveEvent(channel);
  }; //移除事件监控
  void TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb) {
    _timer_wheel.TimerAdd(id, delay, cb);
  };
  void TimerRefresh(uint64_t id) { _timer_wheel.TimerRefresh(id); };
  void TimerCancel(uint64_t id) { _timer_wheel.TimerCancel(id); };
  bool HasTimer(uint64_t id) { return _timer_wheel.HasTimer(id); };
  ~EventLoop() {
    if (_event_fd != -1) {
      close(_event_fd);
      _event_fd = -1;
    }
  }
};
class LoopThread {
private:
  //用于实现_loop获取的同步关系，避免线程创建了但_loop还没有实例化之前调用GetLoop
  std::mutex _mutex;             //锁
  std::condition_variable _cond; //条件变量
  std::thread _thread;           // EventLoop对应的线程
  EventLoop *_loop;

private:
  void ThreadEntry() {
    EventLoop loop;
    {
      std::unique_lock<std::mutex> lock(_mutex); //加锁
      _loop = &loop;
      _cond.notify_all();
    }
    loop.Start();
  }; //实例化EventLoop对象
public:
  LoopThread()
      : _loop(NULL), _thread(std::thread(&LoopThread::ThreadEntry, this)){

                     };
  EventLoop *GetLoop() {
    EventLoop *loop = NULL;
    {
      std::unique_lock<std::mutex> lock(_mutex);
      _cond.wait(lock, [&]() -> bool { return _loop != NULL; });
      loop = _loop;
    }
    return loop;
  };
};
class LoopThreadPool {
private:
  int _thread_count;
  int _next_idx;
  EventLoop *_baseloop;
  std::vector<LoopThread *> _threads;
  std::vector<EventLoop *> _loops;

public:
  LoopThreadPool(EventLoop *baseloop)
      : _thread_count(0), _next_idx(0), _baseloop(baseloop) {}
  void SetThreadCount(int count) { _thread_count = count; };
  void Create() {
    if (_thread_count > 0) {
      _threads.resize(_thread_count);
      _loops.resize(_thread_count);
      for (int i = 0; i < _thread_count; i++) {
        _threads[i] = new LoopThread();
        _loops[i] = _threads[i]->GetLoop();
      }
    }
    return;
  }
  EventLoop *NextLoop() {
    if (_thread_count == 0) {
      return _baseloop;
    }
    _next_idx = (_next_idx + 1) % _thread_count;
    return _loops[_next_idx];
  };
};
class Any {
private:
  class holder {
  public:
    virtual ~holder() {}
    virtual const std::type_info &type() = 0;
    virtual holder *clone() = 0;
  };
  template <class T> class placeholder : public holder {
  public:
    placeholder(const T &val) : _val(val) {}
    ~placeholder() {}
    virtual const std::type_info &type() {
      return typeid(T);
    } //获取子类对象保存的数据类型
    virtual holder *clone() {
      return new placeholder<T>(_val);
    }; //针对当前的对象自身，克隆出一个新的子类对象
    T _val;
  };
  holder *_content;

public:
  Any() : _content(nullptr) {}
  template <class T> Any(const T &val) : _content(new placeholder<T>(val)){};
  Any(const Any &other)
      : _content(other._content ? other._content->clone() : nullptr) {}
  ~Any() {
    if (_content) {
      delete _content;
      _content = nullptr;
    }
  }
  Any swap(Any &other) {
    std::swap(_content, other._content);
    return *this;
  };

  template <class T> T *get() {
    if (typeid(T) != _content->type()) {
      throw nullptr;
    }
    return &((placeholder<T> *)_content)->_val;
  }; //返回子类对象保存的数据指针
  template <class T> Any &operator=(const T &val) {
    Any(val).swap(*this);
    return *this;
  };
  Any &operator=(const Any &other) {
    Any(other).swap(*this);
    return *this;
  };
};
class Connection;
typedef enum {
  DISCONNECTED,
  CONNECTING,
  CONNECTED,
  DISCONNECTING,
} ConnStatu;
using PtrConnection = std::shared_ptr<Connection>;
class Connection : public std::enable_shared_from_this<Connection> {
private:
  uint64_t _conn_id;

  int _sockfd;
  bool _enable_inactive_release; //默认为false
  EventLoop *_loop;
  ConnStatu _status;
  Socket _socket;
  Channel _channel;
  Buffer _in_buffer;
  Buffer _out_buffer;
  Any _context;

  using ConnectedCallback = std::function<void(const PtrConnection &)>;
  using MessageCallback = std::function<void(const PtrConnection &, Buffer *)>;
  using ClosedCallback = std::function<void(const PtrConnection &)>;
  using AnyEventCallback = std::function<void(const PtrConnection &)>;
  ConnectedCallback _connected_callback;
  MessageCallback _message_callback;
  ClosedCallback _closed_callback;
  AnyEventCallback _event_callback;
  /*组件内的连接关闭回调--组件内设置的
  因为服务器组件会把所有的连接管理起来，一旦某个连接关闭，就应该从管理的地方移除掉自己的信息*/
  ClosedCallback _server_closed_callback;

private:
  void HandleRead() {
    char buf[65536];
    int ret = _socket.NoneBlockRecv(buf, sizeof(buf));
    if (ret < 0) {
      return Shutdown();
    } else if (ret == 0) {
      //对端关闭连接
      return ReleaseInLoop();
    } else if (ret > 0) {
      _in_buffer.WriteAndPush(buf, ret);
      if (_in_buffer.ReadAbleSize() > 0) {
        return _message_callback(
            shared_from_this(),
            &_in_buffer); // shared_from_this()返回当前对象的智能指针，用于在回调函数中使用当前对象的智能指针，使用需所属类集成std::enable_shared_from_this<类型名>
      }
    }
  };
  void HandleWrite() {
    ssize_t ret = _socket.NoneBlockSend(_out_buffer.ReadPosition(),
                                        _out_buffer.ReadAbleSize());
    if (ret < 0) {
      if (_in_buffer.ReadAbleSize() > 0) {
        _message_callback(shared_from_this(), &_in_buffer);
      }
      return ReleaseInLoop();
    }
    _out_buffer.MoveReadOffset(ret); //移动读偏移
    if (_out_buffer.ReadAbleSize() == 0) {
      _channel.DisableWrite();
      if (_status == DISCONNECTING) {
        return ReleaseInLoop();
      }
    }
    return;
  };
  void HandleClose() {
    if (_in_buffer.ReadAbleSize() > 0) {
      _message_callback(shared_from_this(), &_in_buffer);
    }
    return ReleaseInLoop();
  };
  void HandleError() { return HandleClose(); };
  void HandleEvent() {
    // 1.刷新连接的活跃度--延迟销毁任务
    // 2.调用组件使用者得到任意回调
    if (_enable_inactive_release == true) {
      _loop->TimerRefresh(_conn_id);
    }
    if (_event_callback != nullptr) {
      _event_callback(shared_from_this());
    }
  };
  void SendInLoop(Buffer buf) {
    if (_status == DISCONNECTED) {
      return;
    }
    _out_buffer.WriteBufferAndPush(buf);
    if (_channel.WriteAble() == false) {
      _channel.EnableWrite();
    }
  };
  void ReleaseInLoop() {
    if (_status == DISCONNECTED) return;
    auto self = shared_from_this();
    _status = DISCONNECTED;
    _channel.Remove();
    _socket.Close();
    if (_loop->HasTimer(_conn_id)) {
      CancelInactiveReleaseInLoop();
    }
    if (_closed_callback)
      _closed_callback(self);
    if (_server_closed_callback)
      _server_closed_callback(self);
  }; //实际的释放接口
  void EstablishedInLoop() {
    if (_status != CONNECTING) return;
    //修改连接状态
    assert(_status == CONNECTING);
    _status = CONNECTED;
    //启动读事件监控
    _channel.EnableRead();
    if (_connected_callback)
      _connected_callback(shared_from_this());
    //调用回调函数
  };
  void ShutdownInLoop() {
    _status = DISCONNECTING;
    if (_in_buffer.ReadAbleSize() > 0) {
      if (_message_callback != nullptr)
        _message_callback(shared_from_this(), &_in_buffer);
    }
    if (_out_buffer.ReadAbleSize() > 0) {
      if (_channel.WriteAble() == false) {
        _channel.EnableWrite();
      }
    }
    if (_out_buffer.ReadAbleSize() == 0) {
      ReleaseInLoop();
    }
  };
  void EnableInactiveReleaseInLoop(int sec) {
    //判断标志 _enable_inactive_release 是否为true
    _enable_inactive_release = true;
    if (_loop->HasTimer(_conn_id)) {
      return _loop->TimerRefresh(_conn_id);
    }
    _loop->TimerAdd(_conn_id, sec, std::bind(&Connection::ReleaseInLoop, this));
  }; //启动非活跃连接超时释放规则
  void CancelInactiveReleaseInLoop() {
    //判断标志 _enable_inactive_release 是否为false
    _enable_inactive_release = false;
    if (_loop->HasTimer(_conn_id)) {
      return _loop->TimerCancel(_conn_id);
    }
  }; //取消非活跃连接超时释放规则
  void UpgradeInLoop(const Any &context, const ConnectedCallback &conn,
                     const MessageCallback &msg, const ClosedCallback &closed,
                     const AnyEventCallback &event) {
    _context = context;
    _connected_callback = conn;
    _message_callback = msg;
    _closed_callback = closed;
    _event_callback = event;
  };

public:
  Connection(EventLoop *loop, uint64_t conn_id, int sockfd)
      : _loop(loop), _conn_id(conn_id), _socket(sockfd),
        _enable_inactive_release(false), _status(CONNECTING),
        _channel(loop, sockfd) {
    _channel.SetCloseCallback(std::bind(&Connection::HandleClose, this));
    _channel.SetEventCallback(std::bind(&Connection::HandleEvent, this));
    _channel.SetReadCallback(std::bind(&Connection::HandleRead, this));
    _channel.SetWriteCallback(std::bind(&Connection::HandleWrite, this));
    _channel.SetErrorCallback(std::bind(&Connection::HandleError, this));
  };
  ~Connection() { DBG_LOG("RELEASE CONNECTION:%p", this); };
  int Fd() { return _socket.Fd(); };
  int Id() { return _conn_id; };
  bool Connected() { return _status == CONNECTED; };
  void SetContext(const Any &context) { _context = context; }; //设置上下文
  Any *GetContext() { return &_context; };
  void SetConnectedCallback(const ConnectedCallback &cb) {
    _connected_callback = cb;
  };
  void SetMessageCallback(const MessageCallback &cb) {
    _message_callback = cb;
  };
  void SetClosedCallback(const ClosedCallback &cb) { _closed_callback = cb; };
  void SetAnyEventCallback(const AnyEventCallback &cb) {
    _event_callback = cb;
  };
  void SetServerClosedCallback(const ClosedCallback &cb) {
    _server_closed_callback = cb;
  };
  void Established() {
    _loop->RunInLoop(std::bind(&Connection::EstablishedInLoop, this));
  };
  void Send(const char *data, size_t len) {
    Buffer buf;
    buf.WriteAndPush(data, len);
    _loop->RunInLoop(std::bind(&Connection::SendInLoop, this, buf));
  }; //发送数据，将数据放入发送缓冲区，启动事件监控
  void Shutdown() {
    _loop->RunInLoop(std::bind(&Connection::ShutdownInLoop, this));
  }; //提供给组件使用者的关闭接口，实际并不真正关闭，需判断有无数据待处理
  void EnableInactiveRelease(int sec) {
    _loop->RunInLoop(
        std::bind(&Connection::EnableInactiveReleaseInLoop, this, sec));
  };
  void CancelInactiveRelease(int sec) {
    _loop->RunInLoop(std::bind(&Connection::CancelInactiveReleaseInLoop, this));
  };

  void Upgrade(const ConnectedCallback &conn, const MessageCallback &msg,
               const ClosedCallback &closed, const AnyEventCallback &event) {
    _loop
        ->AssertInLoop(); //防备新事件出发后，处理的时候，切换任务还没有被执行--会导致数据使用原协议进行
    _loop->RunInLoop(std::bind(&Connection::UpgradeInLoop, this, _context, conn,
                               msg, closed, event));
  }; //切换协议，重置上下文以及阶段性处理函数
};
class Acceptor {
private:
  Socket _socket;
  EventLoop *_loop;
  Channel _channel;

  using AcceptCallback = std::function<void(int)>;
  AcceptCallback _accept_callback;

private:
  void HandleRead() {
    int newfd = _socket.Accept();
    if (newfd < 0) {
      return;
    }
    _accept_callback(newfd);
  };
  int CreateServer(int port) {
    bool ret = _socket.CreateServer(port);
    assert(ret == true);
    return _socket.Fd();
  }

public:
  Acceptor(EventLoop *loop, int port) //               EventLoop *loop, int fd
      : _socket(CreateServer(port)), _loop(loop), _channel(loop, _socket.Fd()) {
    _channel.SetReadCallback(std::bind(&Acceptor::HandleRead, this));
    // bind 的结果大概长这样：[this]() { this->HandleRead();
    // }调用时不需要传任何参数
  };
  ~Acceptor() { _socket.Close(); };
  void SetAcceptCallback(const AcceptCallback &cb) { _accept_callback = cb; };
  void Listen() { _channel.EnableRead(); };
};
class TcpServer {
private:
  uint64_t _next_id; //自动增长的连接id
  int _port;
  int _timeout;                   //非活跃连接统计时间
  bool _enable_inactive_release; //是否开启非活跃连接释放
  EventLoop _baseloop; //主线程的EventLoop对象，负责监听事件的处理
  Acceptor _acceptor; //监听套接字的管理对象
  LoopThreadPool _pool; //线程池对象，负责创建线程以及分配EventLoop
  std::unordered_map<uint64_t, PtrConnection>
      _conns; //连接管理器，负责管理所有连接
  using ConnectedCallback = std::function<void(const PtrConnection &)>;
  using MessageCallback = std::function<void(const PtrConnection &, Buffer *)>;
  using ClosedCallback = std::function<void(const PtrConnection &)>;
  using AnyEventCallback = std::function<void(const PtrConnection &)>;
  ConnectedCallback _connected_callback;
  MessageCallback _message_callback;
  ClosedCallback _closed_callback;
  AnyEventCallback _event_callback;

  using Functor = std::function<void()>;

private:
  void NewConnection(int fd) {
    PtrConnection conn(new Connection(_pool.NextLoop(), _next_id, fd));
    conn->SetMessageCallback(_message_callback);
    conn->SetClosedCallback(_closed_callback);
    conn->SetConnectedCallback(_connected_callback);
    conn->SetAnyEventCallback(_event_callback);
    conn->SetServerClosedCallback(std::bind(&TcpServer::RemoveConnection, this, std::placeholders::_1));
    if(_enable_inactive_release)
      conn->EnableInactiveRelease(_timeout);
    conn->Established();
    _conns.insert(std::make_pair(_next_id, conn));
    _next_id++;
  };
  void RemoveConnectionInLoop(const PtrConnection &conn){
    int id = conn -> Id();
    _conns.erase(id);
  };
  void RemoveConnection(const PtrConnection &conn){
    _baseloop.RunInLoop(std::bind(&TcpServer::RemoveConnectionInLoop, this, conn));
  };
  void RunAfterInLoop(const Functor &task, int delay) {
    _next_id++;
    _baseloop.TimerAdd(_next_id, delay, task);
  };

public:
  TcpServer(int port)
      : _port(port), _next_id(0), _timeout(0), _enable_inactive_release(false),
        _baseloop(), _acceptor(&_baseloop, _port), _pool(&_baseloop){
    _acceptor.SetAcceptCallback(std::bind(&TcpServer::NewConnection, this, std::placeholders::_1)); 
    _acceptor.Listen();
  };
  void SetThreadCount(int count) { _pool.SetThreadCount(count); };
  void SetConnectedCallback(const ConnectedCallback &cb) {
    _connected_callback = cb;
  };
  void SetMessageCallback(const MessageCallback &cb) {
    _message_callback = cb;
  };
  void SetClosedCallback(const ClosedCallback &cb) { _closed_callback = cb; };
  void SetAnyEventCallback(const AnyEventCallback &cb) {
    _event_callback = cb;
  };
  void EnableInactiveRelease(int timeout) {
    _timeout = timeout;
    _enable_inactive_release = true;
  };
  void RunAfter(const Functor &task, int delay) {
    _baseloop.RunInLoop(
        std::bind(&TcpServer::RunAfterInLoop, this, task, delay));
  };
  void Start() { 
      _pool.Create();
      _baseloop.Start();
  };
};
void TimerWheel::TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb) {
  _loop->RunInLoop(std::bind(&TimerWheel::TimerAddInLoop, this, id, delay, cb));
}
void TimerWheel::TimerRefresh(uint64_t id) {
  _loop->RunInLoop(std::bind(&TimerWheel::TimerRefreshInLoop, this, id));
}
void TimerWheel::TimerCancel(uint64_t id) {
  _loop->RunInLoop(std::bind(&TimerWheel::TimerCancelInLoop, this, id));
}
//移除监控
void Channel::Remove() {
  //调用EventLoop移除监控
  return _loop->RemoveEvent(this);
};
void Channel::Update() { return _loop->UpdateEvent(this); };

class NetWork {
    public:
        NetWork() {
            DBG_LOG("SIGPIPE INIT");
            signal(SIGPIPE, SIG_IGN);
        }
};
static NetWork nw;
#endif