#pragma once

#include "graph.hpp"

/**
@file async_task.hpp
@brief asynchronous task include file
*/

namespace tf {

// ----------------------------------------------------------------------------
// AsyncTask
// ----------------------------------------------------------------------------

/**
@brief class to create a dependent asynchronous task

tf::AsyncTask 是一个轻量级句柄，它保留由 executor 创建的依赖异步任务的 共享所有权。 
这种 共享所有权 可确保异步任务在将其添加到另一个异步任务的依赖列表时保持活动状态，从而避免经典的 [ABA 问题] 

@code{.cpp}
// 主线程保留异步任务 A 的共享所有权
tf::AsyncTask A = executor.silent_dependent_async([](){});

// 任务 A 在被添加到异步任务 B 的依赖列表时保持活动状态（即，主线程至少有一个引用计数）
tf::AsyncTask B = executor.silent_dependent_async([](){}, A);
@endcode

目前，tf::AsyncTask 是基于 C++ 智能指针 std::shared_ptr 实现的，只要只有少数对象拥有它，复制或移动就被认为是廉价的。 
当 worker 完成异步任务时，它将从 executor 中删除任务，将共享所有者的数量减一。 如果该计数器达到零，任务将被销毁。
*/
class AsyncTask {
  
  friend class FlowBuilder;
  friend class Runtime;
  friend class Taskflow;
  friend class TaskView;
  friend class Executor;
  
  public:
     
    AsyncTask() = default;
     
    ~AsyncTask() = default;
     
    AsyncTask(const AsyncTask& rhs) = default;
 
    AsyncTask(AsyncTask&& rhs) = default;
     
    AsyncTask& operator = (const AsyncTask& rhs) = default;
 
    AsyncTask& operator = (AsyncTask&& rhs) = default;
    
    // 检查任务是否存储了一个非空的共享指针
    bool empty() const;
    
    // 释放所有权
    void reset();
    
    // 获取底层节点的哈希值
    size_t hash_value() const;

  private:

    AsyncTask(std::shared_ptr<Node>);

    std::shared_ptr<Node> _node;
};

// Constructor
inline AsyncTask::AsyncTask(std::shared_ptr<Node> ptr) : _node {std::move(ptr)} { }

// Function: empty
inline bool AsyncTask::empty() const {
  return _node == nullptr;
}

// Function: reset
inline void AsyncTask::reset() {
  _node.reset();
}

// Function: hash_value
inline size_t AsyncTask::hash_value() const {
  return std::hash<std::shared_ptr<Node>>{}(_node);
}

}  // end of namespace tf ----------------------------------------------------



