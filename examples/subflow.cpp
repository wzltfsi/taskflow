// 此示例演示如何使用 Taskflow 在执行期间创建动态工作负载。
//  我们先创建A、B、C、D四个任务，B在执行过程中，使用flow builder创建了另外三个任务B1、B2、B3，并添加了B1、B2对B3的依赖。
// 我们使用 dispatch 并等待图表完成。 这样做不同于“wait_for_all”，后者将清理完成的图形。 图形完成后，我们转储拓扑以供检查。
// Usage: ./subflow detach|join
//

#include <taskflow/taskflow.hpp>

const auto usage = "usage: ./subflow detach|join";

int main(int argc, char* argv[]) {

  if(argc != 2) {
    std::cerr << usage << std::endl;
    std::exit(EXIT_FAILURE);
  }

  std::string opt(argv[1]);

  if(opt != "detach" && opt != "join") {
    std::cerr << usage << std::endl;
    std::exit(EXIT_FAILURE);
  }

  auto detached = (opt == "detach") ? true : false;
  // 创建一个包含三个常规任务和一个子流任务的任务流图。
  tf::Executor executor(4);
  tf::Taskflow taskflow("Dynamic Tasking Demo");

  // Task A
  auto A = taskflow.emplace([] () { std::cout << "TaskA\n"; });
  auto B = taskflow.emplace(
    // Task B
    [cap=std::vector<int>{1,2,3,4,5,6,7,8}, detached] (tf::Subflow& subflow) {
      std::cout << "TaskB is spawning B1, B2, and B3 ...\n";

      auto B1 = subflow.emplace([&]() {
        printf("  Subtask B1: reduce sum = %d\n",   std::accumulate(cap.begin(), cap.end(), 0, std::plus<int>()));
      }).name("B1");

      auto B2 = subflow.emplace([&]() {
        printf("  Subtask B2: reduce multiply = %d\n",   std::accumulate(cap.begin(), cap.end(), 1, std::multiplies<int>()));
      }).name("B2");

      auto B3 = subflow.emplace([&]() {
        printf("  Subtask B3: reduce minus = %d\n",   std::accumulate(cap.begin(), cap.end(), 0, std::minus<int>()));
      }).name("B3");

      B1.precede(B3);
      B2.precede(B3);

      // 分离或加入子流（默认子流在 B 处加入）
      if(detached) subflow.detach();
    }
  );

  auto C = taskflow.emplace([] () { std::cout << "TaskC\n"; });
  auto D = taskflow.emplace([] () { std::cout << "TaskD\n"; });
  A.name("A");
  B.name("B");
  C.name("C");
  D.name("D");

  A.precede(B);  // B runs after A
  A.precede(C);  // C runs after A
  B.precede(D);  // D runs after B
  C.precede(D);  // D runs after C

  executor.run(taskflow).get();  // block until finished

  // examine the graph
  taskflow.dump(std::cout);

  return 0;
}



