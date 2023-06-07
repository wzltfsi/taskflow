// 一个带有信号量约束的简单示例，一次只能执行一个任务。

#include <taskflow/taskflow.hpp>

void sl() {
  std::this_thread::sleep_for(std::chrono::seconds(1));
}

int main() {

  tf::Executor executor(4);
  tf::Taskflow taskflow;

  // 定义 1 个 worker 的临界区
  tf::Semaphore semaphore(1);

  // 在  taskflow 中创建 task
  std::vector<tf::Task> tasks {
    taskflow.emplace([](){ sl(); std::cout << "A" << std::endl; }),
    taskflow.emplace([](){ sl(); std::cout << "B" << std::endl; }),
    taskflow.emplace([](){ sl(); std::cout << "C" << std::endl; }),
    taskflow.emplace([](){ sl(); std::cout << "D" << std::endl; }),
    taskflow.emplace([](){ sl(); std::cout << "E" << std::endl; })
  };

  for(auto & task : tasks) {
    task.acquire(semaphore);
    task.release(semaphore);
  }

  executor.run(taskflow);
  executor.wait_for_all();

  return 0;
}

