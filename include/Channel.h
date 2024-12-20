#pragma once
#include "Timestamp.h"
#include "noncopyable.h"
#include <functional>
#include <memory>
class EventLoop;
// 在循环引用中，前置声明的类型只能用于指针和引用，不能用于值传递。这是因为前置声明只告诉编译器某个类型的名称存在，
// 但并未提供该类型的具体定义，编译器无法知道它的大小和布局。
// 因此，尝试按值传递或按值存储一个前置声明的类型会导致编译错误。
class Channel : public muduo::noncopyable {
public:
  using EventCallback = std::function<void()>; //事件回调
  using ReadEventCallback = std::function<void(TimeStamp)>;

  Channel(EventLoop loop, int fd);
  ~Channel();

  void HandlerEvent(TimeStamp receive_time);
  //这个TimeStamp作为函数参数类型，编译的时候是需要知道他的大小的，所以必须要include
  // Tmiestamp.h而不能仅仅是声明 TimeStamp类。

  //设置回调函数对象
  void setReadCallback(ReadEventCallback cb) {
    read_callback_ = std::move(cb);
  } //将cb这个左值转换为右值引用

  void tie(const std::shared_ptr<void>
               &); //防止当channel被手动remove掉，channel还正在执行回调操作
  int fd() const { return fd_; }
  int events() const { return events_; }

  int set_revents(int revt) {
    revents_ = revt;
  } //通过这个设置得知当前channel的fd发生了什么事件类型（可读、可写、错误、关闭）

  void enableReading() {
    events_ |= kReadEvent;
    update();
  } //设置fd相应的事件状态，比如让这个fd监听可读事件，你就必须要用epoll_ctl设置它！
  void disableReading() {
    events_ &= ~kReadEvent;
    update();
  }
  void enableWriting() {
    events_ |= kWriteEvent;
    update();
  }
  void disableWriting() {
    events_ &= ~kWriteEvent;
    update();
  }
  void disableAll() {
    events_ = kNoneEvent;
    update();
  }
  bool isWriting() const {
    return events_ & kWriteEvent;
  } //当前感兴趣的事件是否包含可写事件
  bool isReading() const {
    return events_ & kReadEvent;
  } //当前感兴趣的事件是否包含可读事件
  bool isNoneEvent() const { return events_ == kNoneEvent; }

  int index() { return index_; }
  void set_index(int idx) { index_ = idx; }

  // one loop per thread
  EventLoop *ownerLoop() { return loop_; } //当前这个channel属于哪一个event loop
  void remove();

private:
  void update();
  void HandleEventWithGuard(
      TimeStamp receiveTime); //和上面的handleEvent差不多，处理受保护的事件

  //下面三个变量代表这个fd对哪些事件类型感兴趣
  static const int kNoneEvent; // 0
  static const int kReadEvent; // = EPOLLIN | EPOLLPRI;
                               // EPOLLPRI带外数据，和select的异常事件集合对应
  static const int kWriteEvent; // = EPOLLOUT

  EventLoop *loop_; //这个channel属于哪个EventLoop对象
  const int fd_;    // fd, poller监听的对象
  int events_;      //注册fd感兴趣的事件
  int revents_;     // poller返回的具体发生的事件。
  int index_; //这个index_其实表示的是这个channel的状态，是kNew还是kAdded还是kDeleted
              // kNew代表这个channel未添加到poller中，kAdded表示已添加到poller中，kDeleted表示从poller中删除了

  std::weak_ptr<void> tie_; //这个tie_ 绑定了....
  bool tied_;
  // channel通道里面能够获知fd最终发生的具体事件revents，所以它负责调用具体事件的回调操作。
  ReadEventCallback read_callback_;
  EventCallback write_callback_;
  EventCallback close_callback_;
  EventCallback error_callback_;
};