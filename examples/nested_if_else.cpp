// 该程序演示了如何使用条件任务创建嵌套的 if-else 控制流
#include <taskflow/taskflow.hpp>

int main() {

  tf::Executor executor;
  tf::Taskflow taskflow;

  int i;

  // 为嵌套控制流创建三个条件任务
  auto initi = taskflow.emplace([&](){ i=3; });
  auto cond1 = taskflow.emplace([&](){ return i>1 ? 1 : 0; });
  auto cond2 = taskflow.emplace([&](){ return i>2 ? 1 : 0; });
  auto cond3 = taskflow.emplace([&](){ return i>3 ? 1 : 0; });
  auto equl1 = taskflow.emplace([&](){ std::cout << "i=1\n"; });
  auto equl2 = taskflow.emplace([&](){ std::cout << "i=2\n"; });
  auto equl3 = taskflow.emplace([&](){ std::cout << "i=3\n"; });
  auto grtr3 = taskflow.emplace([&](){ std::cout << "i>3\n"; });

  initi.precede(cond1);
  cond1.precede(equl1, cond2);
  cond2.precede(equl2, cond3);
  cond3.precede(equl3, grtr3);

  // dump the conditioned flow
  taskflow.dump(std::cout);

  executor.run(taskflow).wait();

  return 0;
}

