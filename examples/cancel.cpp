// The program demonstrates how to cancel a submitted taskflow
// graph and wait until the cancellation completes.

#include <taskflow/taskflow.hpp>

int main() {

  tf::Executor executor;
  tf::Taskflow taskflow("cancel");

  // 我们创建了一个每 1 秒包含 1000 个任务的任务流图。 理想情况下，任务流在 1000/P 秒内完成，其中 P 是工作人员的数量。
  for(int i=0; i<1000; i++) {
    taskflow.emplace([](){
      std::this_thread::sleep_for(std::chrono::seconds(1));
    });
  }

  // submit the taskflow
  auto beg = std::chrono::steady_clock::now();
  tf::Future fu = executor.run(taskflow);

  // submit a cancel request to cancel all 1000 tasks.
  fu.cancel();

  // wait until the cancellation finishes
  fu.get();
  auto end = std::chrono::steady_clock::now();

  // the duration should be much less than 1000 seconds
  std::cout << "taskflow completes in "
            << std::chrono::duration_cast<std::chrono::milliseconds>(end-beg).count()
            << " milliseconds\n";

  return 0;
}


