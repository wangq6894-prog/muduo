#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <unistd.h>
#include <unordered_map>
#include <vector>
using TaskFunc = std::function<void()>;
using ReleaseFunc = std::function<void()>;
class TimeTask {
public:
  TimeTask(uint64_t id, uint64_t delay, TaskFunc task_cb) {
    _id = id;
    _timeout = delay;
    _canceled = false;
    _task_cb_ = task_cb;
  }
  ~TimeTask() { _task_cb_(); }
  void Cancel() { _canceled = true; }
  void SetRelease(const ReleaseFunc &cb) { _release = cb; }
  uint64_t DelayTime() { return _timeout; }

private:
  uint64_t _id;      //定时器任务对象id
  uint64_t _timeout; //定时任务超时时间
  TaskFunc _task_cb_; //定时任务回调函数/定时器对象要执行的任务
  bool _canceled;     //定时任务是否被取消
  ReleaseFunc _release; //定时任务释放回调函数
};

class TimerWheel {
private:
  using WeakTask = std::weak_ptr<TimeTask>;
  using PtrTask = std::shared_ptr<TimeTask>;
  int _tick;     //表盘秒针----当前时间点
  int _capacity; //表盘最大数量----最大定时任务数量
  std::vector<std::vector<PtrTask>> _wheel;
  std::unordered_map<uint64_t, WeakTask> _timers; //定时任务id----定时任务指针
  void RemoveTimer(uint64_t id) {
    _timers.erase(id);
    // auto it = _timers.find(id);
    // if(it != _timers.end()){
    //     _timers.erase(it);
    // }
  };

public:
  TimerWheel(int capacity = 60, int tick = 0)
      : _tick(tick), _capacity(capacity), _wheel(capacity) {}
  void TimerAdd(uint64_t id, uint64_t delay, TaskFunc task_cb) {
    PtrTask pt(new TimeTask(id, delay, task_cb));
    pt->SetRelease(std::bind(&TimerWheel::RemoveTimer, this, id));
    _timers[id] = WeakTask(pt);
    _wheel[(_tick + delay) % _capacity].push_back(pt);
  };
  void TimerRefreash(uint64_t id) {
    auto it = _timers.find(id);
    if (it == _timers.end()) {
      return;
    }
    PtrTask pt = it->second.lock();
    int delay = pt->DelayTime();
    _wheel[(_tick + delay) % _capacity].push_back(pt);
  };
  void TimerCancel(uint64_t id) {
    auto it = _timers.find(id);
    if (it == _timers.end()) {
      return;
    }
    PtrTask pt = it->second.lock();
    if (pt)
      pt->Cancel();
  }
  //每秒秒执行一次
  void RunTimerTask() {
    _tick = (_tick + 1) % _capacity;
    _wheel[_tick].clear();
  }
};

class Test {
public:
  Test() { std::cout << "构造" << std::endl; }
  ~Test() { std::cout << "析构" << std::endl; }
};

void DelTest(Test *t) { delete t; }
int main() {
  TimerWheel tw;
  Test *t = new Test();
  tw.TimerAdd(888, 5, std::bind(DelTest, t));

  for (int i = 0; i < 5; i++) {
    tw.TimerRefreash(888);
    tw.RunTimerTask();
    std::cout << "刷新了一次任务，重新需要5s后才会销毁" << std::endl;
  }
  tw.TimerCancel(888);
  std::cout << "取消了任务" << std::endl;
  while (1) {
    sleep(1);
    std::cout << "--------" << std::endl;
    tw.RunTimerTask();
  }
  return 0;
}
