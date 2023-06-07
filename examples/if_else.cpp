// 该程序演示了如何使用条件任务创建 if-else 控制流。


#include <taskflow/taskflow.hpp>

int main() {

  tf::Executor executor;
  tf::Taskflow taskflow;

  //  创建三个静态任务和一个条件任务
  auto [init, cond, yes, no] = taskflow.emplace(
    [] () { },
    [] () { return 0; },
    [] () { std::cout << "yes\n"; },
    [] () { std::cout << "no\n"; }
  );

  init.name("init");
  cond.name("cond");
  yes.name("yes");
  no.name("no");

  cond.succeed(init);

  // 使用此命令，当 cond 返回 0 时，执行将转到 yes。 当 cond 返回 1 时，执行移至 no。
  cond.precede(yes, no);

  // dump the conditioned flow
  taskflow.dump(std::cout);

  executor.run(taskflow).wait();

  return 0;
}

