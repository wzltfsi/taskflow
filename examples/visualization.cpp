// 此示例演示如何使用“转储”方法以 DOT 格式可视化任务流图 

#include <taskflow/taskflow.hpp>

int main(){

  tf::Taskflow taskflow("Visualization Demo");

  // ------------------------------------------------------
  // Static Tasking
  // ------------------------------------------------------
  auto A = taskflow.emplace([] () { std::cout << "Task A\n"; });
  auto B = taskflow.emplace([] () { std::cout << "Task B\n"; });
  auto C = taskflow.emplace([] () { std::cout << "Task C\n"; });
  auto D = taskflow.emplace([] () { std::cout << "Task D\n"; });
  auto E = taskflow.emplace([] () { std::cout << "Task E\n"; });

  A.precede(B, C, E);
  C.precede(D);
  B.precede(D, E);

  std::cout << "[dump without name assignment]\n";
  taskflow.dump(std::cout);

  std::cout << "[dump with name assignment]\n";
  A.name("A");
  B.name("B");
  C.name("C");
  D.name("D");
  E.name("E");

  // 如果图表仅包含静态任务，您可以在不运行图表的情况下简单地转储它们
  taskflow.dump(std::cout);

  // ------------------------------------------------------
  // Dynamic Tasking
  // ------------------------------------------------------
  taskflow.emplace([](tf::Subflow& sf){
    sf.emplace([](){ std::cout << "subflow task1"; }).name("s1");
    sf.emplace([](){ std::cout << "subflow task2"; }).name("s2");
    sf.emplace([](){ std::cout << "subflow task3"; }).name("s3");
  });

  // 为了可视化子流任务，您需要先运行任务流来生成动态任务
  tf::Executor executor;
  executor.run(taskflow).wait();

  taskflow.dump(std::cout);


  return 0;
}


