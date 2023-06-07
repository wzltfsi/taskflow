// 此示例演示如何将数据附加到任务并使用更改的数据迭代运行任务。

#include <taskflow/taskflow.hpp>

int main(){

  tf::Executor executor;
  tf::Taskflow taskflow("attach data to a task");

  int data;

  // create a task and attach it the data
  auto A = taskflow.placeholder();
  A.data(&data).work([A](){
    auto d = *static_cast<int*>(A.data());
    std::cout << "data is " << d << std::endl;
  });

  // run the taskflow iteratively with changing data
  for(data = 0; data<10; data++){
    executor.run(taskflow).wait();
  }

  return 0;
}
