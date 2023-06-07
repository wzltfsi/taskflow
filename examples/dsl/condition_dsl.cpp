// 2020/08/28 - Created by netcan: https://github.com/netcan
// Task DSL demo
// 该示例创建了以下循环图：
//
//       A
//       |
//       v
//       B<---|
//       |    |
//       v    |
//       C----|
//       |
//       v
//       D
//
// - A 是一个将计数器初始化为零的任务
// - B 是一个递增计数器的任务
// - C 是一个条件任务，它会在 B 周围循环，直到计数器达到一个突破数字
// - D 是一个包含结果的任务

#include <taskflow/taskflow.hpp>
#include <taskflow/dsl.hpp>

int main() {
  tf::Executor executor;
  tf::Taskflow taskflow("Conditional Tasking Demo");

  int counter; // owner

  // 使用context传递参数, context必须可复制  
  struct Context {
    int &rcounter; // use counter(borrow)
  } context{counter};

  make_task((A, Context), {
    std::cout << "initializes the counter to zero\n";
    rcounter = 0;
  });
  make_task((B, Context), {
    std::cout << "loops to increment the counter\n";
    rcounter++;
  });
  make_task((C, Context), {
    std::cout << "counter is " << rcounter << " -> ";
    if (rcounter != 5) {
      std::cout << "loops again (goes to B)\n";
      return 0;
    }
    std::cout << "breaks the loop (goes to D)\n";
    return 1;
  });
  make_task((D, Context), {
    std::cout << "done with counter equal to " << rcounter << '\n';
  });

  auto tasks = build_taskflow(
    task(A)-> task(B) -> task(C),
    task(C)-> fork_tasks(B, D)
  )(taskflow, context);

  tasks.get_task<A>().name("A");
  tasks.get_task<B>().name("B");
  tasks.get_task<C>().name("C");
  tasks.get_task<D>().name("D");


  taskflow.dump(std::cout);
 
  executor.run(taskflow).wait();

  assert(counter == 5);

  return 0;
}
