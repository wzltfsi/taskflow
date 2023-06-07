// 该程序演示了如何创建一个流水线调度框架，该框架传播一系列整数并在每个阶段将结果加一。 管道具有以下结构：
// 
// o -> o -> o
// |         |
// v         v
// o -> o -> o
// |         |
// v         v
// o -> o -> o
// |         |
// v         v
// o -> o -> o

#include <taskflow/taskflow.hpp>
#include <taskflow/algorithm/pipeline.hpp>

int main() {

  tf::Taskflow taskflow("pipeline");
  tf::Executor executor;

  const size_t num_lines = 4;

  // custom data storage
  std::array<size_t, num_lines> buffer;

  // 管道由三个管道（串行-并行-串行）和最多四个并发调度令牌组成
  tf::Pipeline pl(num_lines,
    tf::Pipe{tf::PipeType::SERIAL, [&buffer](tf::Pipeflow& pf) {
      // generate only 5 scheduling tokens
      if(pf.token() == 5) {
        pf.stop();
      }
      // 将此管道的结果保存到缓冲区中
      else {
        printf("stage 1: input token = %zu\n", pf.token());
        buffer[pf.line()] = pf.token();
      }
    }},

    tf::Pipe{tf::PipeType::PARALLEL, [&buffer](tf::Pipeflow& pf) {
      printf(  "stage 2: input buffer[%zu] = %zu\n", pf.line(), buffer[pf.line()]);
      // 将先前的结果传播到此管道并将其递增 1
      buffer[pf.line()] = buffer[pf.line()] + 1;
    }},

    tf::Pipe{tf::PipeType::SERIAL, [&buffer](tf::Pipeflow& pf) {
      printf( "stage 3: input buffer[%zu] = %zu\n", pf.line(), buffer[pf.line()] );
      // 将先前的结果传播到此管道并将其递增 1
      buffer[pf.line()] = buffer[pf.line()] + 1;
    }}
  );




  tf::Task init = taskflow.emplace([](){ std::cout << "ready\n"; }).name("starting pipeline");
  tf::Task task = taskflow.composed_of(pl).name("pipeline");
  tf::Task stop = taskflow.emplace([](){ std::cout << "stopped\n"; }).name("pipeline stopped");

  init.precede(task);
  task.precede(stop);

  taskflow.dump(std::cout);

  executor.run(taskflow).wait();

  return 0;
}
