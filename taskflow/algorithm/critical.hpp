#pragma once

#include "../core/task.hpp"

/**
@file critical.hpp
@brief critical include file
*/

namespace tf {

// ----------------------------------------------------------------------------
// CriticalSection
// ----------------------------------------------------------------------------

/**
@class CriticalSection

@brief class to create a critical region of limited workers to run tasks

tf::CriticalSection 是 tf::Semaphore 的变形器，专门用于限制一组任务的最大并发性。 
临界区以表示该限制的初始计数开始。 当一个任务被添加到临界区时，该任务获取并释放临界区内部的信号量。 
这种设计避免了显式调用 tf::Task::acquire 和 tf::Task::release。 下面的示例创建一个 worker 的临界区，并将五个 tasks 添加到临界区。
 

@code{.cpp}
tf::Executor executor(8);   // create an executor of 8 workers
tf::Taskflow taskflow;

// create a critical section of 1 worker
tf::CriticalSection critical_section(1);

tf::Task A = taskflow.emplace([](){ std::cout << "A" << std::endl; });
tf::Task B = taskflow.emplace([](){ std::cout << "B" << std::endl; });
tf::Task C = taskflow.emplace([](){ std::cout << "C" << std::endl; });
tf::Task D = taskflow.emplace([](){ std::cout << "D" << std::endl; });
tf::Task E = taskflow.emplace([](){ std::cout << "E" << std::endl; });

critical_section.add(A, B, C, D, E);

executor.run(taskflow).wait();
@endcode

*/
class CriticalSection : public Semaphore {

  public:

    /**
    @brief constructs a critical region of a limited number of workers
    */
    explicit CriticalSection(size_t max_workers = 1);

    /**
    @brief adds a task into the critical region
    */
    template <typename... Tasks>
    void add(Tasks...tasks);
};

inline CriticalSection::CriticalSection(size_t max_workers) :
  Semaphore {max_workers} {
}

template <typename... Tasks>
void CriticalSection::add(Tasks... tasks) {
  (tasks.acquire(*this), ...);
  (tasks.release(*this), ...);
}


}  // end of namespace tf. ---------------------------------------------------


