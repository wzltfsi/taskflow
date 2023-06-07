// 该示例创建了一个迭代循环的以下循环图： 
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
// - C 是一个条件任务，它与 B 循环，直到计数器达到一个中断数字
// - D 是完成结果的任务



#include <taskflow/taskflow.hpp>

int main() {

  tf::Executor executor;
  tf::Taskflow taskflow("Conditional Tasking Demo");

  int counter;

  auto A = taskflow.emplace([&](){
    std::cout << "initializes the counter to zero\n";
    counter = 0;
  }).name("A");

  auto B = taskflow.emplace([&](){
    std::cout << "loops to increment the counter\n";
    counter++;
  }).name("B");

  auto C = taskflow.emplace([&](){
    std::cout << "counter is " << counter << " -> ";
    if(counter != 5) {
      std::cout << "loops again (goes to B)\n";
      return 0;
    }
    std::cout << "breaks the loop (goes to D)\n";
    return 1;
  }).name("C");

  auto D = taskflow.emplace([&](){
    std::cout << "done with counter equal to " << counter << '\n';
  }).name("D");

  A.precede(B);
  B.precede(C);
  C.precede(B);
  C.precede(D);

  // visualizes the taskflow
  taskflow.dump(std::cout);

  // executes the taskflow
  executor.run(taskflow).wait();

  assert(counter == 5);

  return 0;
}







