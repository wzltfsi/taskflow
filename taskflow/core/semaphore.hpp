#pragma once

#include <vector>
#include <mutex>

#include "declarations.hpp"

/**
@file semaphore.hpp
@brief semaphore include file
*/

namespace tf {

// ----------------------------------------------------------------------------
// Semaphore
// ----------------------------------------------------------------------------

/**
@class Semaphore

@brief class to create a semophore object for building a concurrency constraint

信号量创建一个限制最大并发性的约束，即一组 tasks 中的 workers 数量。
您可以让 task 在执行其工作之前/之后获取/释放一个或多个信号量。
task 可以获取和释放信号量，或者只是获取或只是释放它。 tf::Semaphore 对象以初始计数开始。 
只要该计数大于 0，task 就可以获取信号量并完成它们的工作。 
如果计数为 0 或更少，则尝试获取信号量的 task 将不会运行，而是进入该信号量的等待列表。 
当另一个 task 释放信号量时，它会重新安排该等待列表中的所有 task  

 
@code{.cpp}
tf::Executor executor(8);   // create an executor of 8 workers
tf::Taskflow taskflow;

tf::Semaphore semaphore(1); // create a semaphore with initial count 1

std::vector<tf::Task> tasks {
  taskflow.emplace([](){ std::cout << "A" << std::endl; }),
  taskflow.emplace([](){ std::cout << "B" << std::endl; }),
  taskflow.emplace([](){ std::cout << "C" << std::endl; }),
  taskflow.emplace([](){ std::cout << "D" << std::endl; }),
  taskflow.emplace([](){ std::cout << "E" << std::endl; })
};

for(auto & task : tasks) {  // each task acquires and release the semaphore
  task.acquire(semaphore);
  task.release(semaphore);
}

executor.run(taskflow).wait();
@endcode

上面的示例创建了五个任务，它们之间没有依赖关系。 正常情况下，这五个任务会同时执行。 
但是，这个例子有一个初始计数为 1 的信号量，所有任务都需要在运行前获取该信号量，并在完成后释放该信号量。 
这种安排将并发运行的任务数限制为只有一个。

*/
class Semaphore {

  friend class Node;

  public:

    /**
    @brief constructs a semaphore with the given counter
   信号量创建一个限制最大并发性的约束，即一组任务中的工作人员数量。

    @code{.cpp}
    tf::Semaphore semaphore(4); // 4个worker的并发约束
    @endcode
    */
    explicit Semaphore(size_t max_workers);

    //  查询 counter 值（运行期间不是线程安全的）  
    size_t count() const;

  private:

    std::mutex _mtx;

    size_t _counter;

    std::vector<Node*> _waiters;

    bool _try_acquire_or_wait(Node*);

    std::vector<Node*> _release();
};

inline Semaphore::Semaphore(size_t max_workers) : _counter(max_workers) {}

inline bool Semaphore::_try_acquire_or_wait(Node* me) {
  std::lock_guard<std::mutex> lock(_mtx);
  if(_counter > 0) {
    --_counter;
    return true;
  } else {
    _waiters.push_back(me);
    return false;
  }
}

inline std::vector<Node*> Semaphore::_release() {
  std::lock_guard<std::mutex> lock(_mtx);
  ++_counter;
  std::vector<Node*> r{std::move(_waiters)};
  return r;
}

inline size_t Semaphore::count() const {
  return _counter;
}

}  // end of namespace tf. ---------------------------------------------------

