#pragma once

#include "declarations.hpp"
#include "tsq.hpp"
#include "notifier.hpp"

/**
@file worker.hpp
@brief worker include file
*/

namespace tf {

// ----------------------------------------------------------------------------
// Class Definition: Worker
// ----------------------------------------------------------------------------

/**
@class Worker
class 在执行器中创建一个 worker
该类主要由执行者用来执行工作窃取算法。 
用户可以使用 tf::WorkerInterface 访问工作对象并更改其属性（例如，更改类 POSIX 系统中的线程关联）。
*/
class Worker {

  friend class Executor;
  friend class WorkerView;

  public:

    // 查询与其父执行器相关联的工作人员 ID 工作人员 ID 是 [0, N) 范围内的无符号整数，其中  N 是在 executor 的构造时产生的 workers 数量。
    inline size_t id() const { return _id; }

    // 获取对底层线程的指针访问
    inline std::thread* thread() const { return _thread; }

    // 查询与 worker 关联的队列大小（即要运行的排队任务数）
    inline size_t queue_size() const { return _wsq.size(); }
    
    // 查询队列的当前容量
    inline size_t queue_capacity() const { return static_cast<size_t>(_wsq.capacity()); }

  private:

    size_t                     _id;
    size_t                     _vtm;
    Executor*                  _executor;
    std::thread*               _thread;
    Notifier::Waiter*          _waiter;
    std::default_random_engine _rdgen { std::random_device{}() };
    TaskQueue<Node*>           _wsq;
    Node*                      _cache;
};
 
// ----------------------------------------------------------------------------
// Class Definition: WorkerView
// ----------------------------------------------------------------------------

/**
@class WorkerView
 
用于在 executor 中创建 worker 的不可变视图的类 ,executor保留一组内部 worker 线程来运行 tasks  
 worker view 为用户提供了一个不可变的接口来观察 worker 何时运行 task ，并且视图对象只能从派生自 tf::ObserverInterface 的观察者访问
*/
class WorkerView {

  friend class Executor;

  public:

    size_t id() const;

    size_t queue_size() const;

    size_t queue_capacity() const;

  private:

    WorkerView(const Worker&);
    WorkerView(const WorkerView&) = default;

    const Worker& _worker;

};

// Constructor
inline WorkerView::WorkerView(const Worker& w) : _worker{w} {}

// function: id
inline size_t WorkerView::id() const {
  return _worker._id;
}

// Function: queue_size
inline size_t WorkerView::queue_size() const {
  return _worker._wsq.size();
}

// Function: queue_capacity
inline size_t WorkerView::queue_capacity() const {
  return static_cast<size_t>(_worker._wsq.capacity());
}


// ----------------------------------------------------------------------------
// Class Definition: WorkerInterface
// ----------------------------------------------------------------------------

/**
@class WorkerInterface
在 executor 中配置 worker 行为的类 

tf::WorkerInterface 类允许用户与 executor  交互以自定义 worker 行为，例如在 worker 进入和离开循环之前和之后调用自定义方法。 
当您创建执行程序时，它会生成一组工作程序来运行任务。 executor 和它派生的 worker 之间的交互如下所示：

for(size_t n=0; n<num_workers; n++) {
  create_thread([](Worker& worker)
  
    // 预处理 executor-specific worker 信息
  
    // 进入调度循环。 在这里，WorkerInterface::scheduler_prologue 被调用，如果有的话
    
    while(1) {
      perform_work_stealing_algorithm();
      if(stop) {
        break; 
      }
    }
  
    // 离开调度循环并加入这个工作线程。在这里，WorkerInterface::scheduler_epilogue 被调用，如果有的话
  );
}

tf::WorkerInterface 中定义的方法不是线程安全的，可能会被多个 worker 同时调用。

*/
class WorkerInterface {

  public:
  virtual ~WorkerInterface() = default;
  
  /*  在 worker 进入调度循环之前调用的方法 
      该方法由 executor 的 constructor 调用*/
  virtual void scheduler_prologue(Worker& worker) = 0;
  

  /*  worker 离开调度循环后调用的方法 
      该方法由 executor 的 constructor 调用*/
  virtual void scheduler_epilogue(Worker& worker, std::exception_ptr ptr) = 0;
};

/* 创建从 tf::WorkerInterface 派生的实例的函数 
 
@tparam T type derived from tf::WorkerInterface
@tparam ArgsT argument types to construct @c T

@param args arguments to forward to the constructor of @c T
*/
template <typename T, typename... ArgsT>
std::shared_ptr<T> make_worker_interface(ArgsT&&... args) {
  static_assert(  std::is_base_of_v<WorkerInterface, T>,   "T must be derived from WorkerInterface" );
  return std::make_shared<T>(std::forward<ArgsT>(args)...);
}

}  // end of namespact tf -----------------------------------------------------


