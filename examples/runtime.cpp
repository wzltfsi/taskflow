// 这个程序演示了如何使用运行时任务来强制调度一个永远不会被调度的活动任务。  
#include <taskflow/taskflow.hpp>

int main() {

  tf::Taskflow taskflow("Runtime Tasking");
  tf::Executor executor;

  tf::Task A, B, C, D;

  std::tie(A, B, C, D) = taskflow.emplace(
    [] () { return 0; },
    [&C] (tf::Runtime& rt) {  // C must be captured by reference
      std::cout << "B\n";
      rt.schedule(C);
    },
    [] () { std::cout << "C\n"; },
    [] () { std::cout << "D\n"; }
  );

  // name tasks
  A.name("A");
  B.name("B");
  C.name("C");
  D.name("D");

 
  A.precede(B, C, D);
 
  taskflow.dump(std::cout);

  // we will see both B and C in the output
  executor.run(taskflow).wait();

  return 0;
}
