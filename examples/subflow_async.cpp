// 该程序演示了如何从正在运行的子流创建异步任务

#include <taskflow/taskflow.hpp>

int main() {

  tf::Taskflow taskflow("Subflow Async");
  tf::Executor executor;

  std::atomic<int> counter{0};

  taskflow.emplace([&](tf::Subflow& sf){
    for(int i=0; i<10; i++) {
      // 在这里，我们使用“silent_async”而不是“async”，因为我们不关心返回值。 与“async”相比，“silent_async”方法给我们带来的开销更少。 10 个异步任务同时运行。
      sf.silent_async([&](){
        std::cout << "async task from the subflow\n";
        counter.fetch_add(1, std::memory_order_relaxed);
      });
    }
    sf.join();
    std::cout << counter << " = 10\n";
  });

  executor.run(taskflow).wait();

  return 0;
}

