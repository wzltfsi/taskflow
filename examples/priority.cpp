//  将先前的结果传播到此管道并将其递增 1 
// 该程序演示了如何设置任务的优先级。
// 
//   + tf::TaskPriority::HIGH   (numerical value = 0)
//   + tf::TaskPriority::NORMAL (numerical value = 1)
//   + tf::TaskPriority::LOW    (numerical value = 2)
// 
// 基于优先级的执行是非抢占式的。 一旦一个任务开始执行，它就会一直执行到完成，即使一个更高优先级的任务已经被派生或入队。

#include <taskflow/taskflow.hpp>

int main() {
  
  // 创建只有一个 worker 的 executor 以启用确定性行为 
  tf::Executor executor(1);

  tf::Taskflow taskflow;

  int counter {0};
  
  // 在这里，我们创建了五个任务并打印了它们的执行顺序，这些顺序应该与分配的优先级保持一致
  auto [A, B, C, D, E] = taskflow.emplace(
    [] () { },
    [&] () { std::cout << "Task B: " << counter++ << '\n'; },  // 0  
    [&] () { std::cout << "Task C: " << counter++ << '\n'; },  // 2  
    [&] () { std::cout << "Task D: " << counter++ << '\n'; },  // 1  
    [] () { }
  );

  A.precede(B, C, D); 
  E.succeed(B, C, D);
  
  // 默认情况下，所有任务都是 tf::TaskPriority::HIGH
  B.priority(tf::TaskPriority::HIGH);
  C.priority(tf::TaskPriority::LOW);
  D.priority(tf::TaskPriority::NORMAL);

  assert(B.priority() == tf::TaskPriority::HIGH);
  assert(C.priority() == tf::TaskPriority::LOW);
  assert(D.priority() == tf::TaskPriority::NORMAL);
  
  // we should see B, D, and C in their priority order
  executor.run(taskflow).wait();
}

