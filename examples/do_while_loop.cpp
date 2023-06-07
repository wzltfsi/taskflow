// 该程序演示了如何使用条件任务实现 do-while 控制流。


#include <taskflow/taskflow.hpp>

int main() {

  tf::Executor executor;
  tf::Taskflow taskflow;

  int i;

  auto [init, body, cond, done] = taskflow.emplace(
    [&](){ std::cout << "i=0\n"; i=0; },
    [&](){ std::cout << "i++ => i="; i++; },
    [&](){ std::cout << i << '\n'; return i<5 ? 0 : 1; },
    [&](){ std::cout << "done\n"; }
  );

  init.name("init");
  body.name("do i++");
  cond.name("while i<5");
  done.name("done");

  init.precede(body);
  body.precede(cond);
  cond.precede(body, done);

 taskflow.dump(std::cout);

  executor.run(taskflow).wait();

  return 0;
}

