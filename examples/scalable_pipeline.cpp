// 该程序演示了如何创建一个管道调度框架，该框架使用应用程序提供的一系列管道传播一系列整数并在每个阶段将结果加一。
//
// 管道具有以下结构：
//
// o -> o -> o
// |    |    |
// v    v    v
// o -> o -> o
// |    |    |
// v    v    v
// o -> o -> o
// |    |    |
// v    v    v
// o -> o -> o
//
// 然后，程序将管道重置为新范围的五个管道。
//
// o -> o -> o -> o -> o
// |    |    |    |    |
// v    v    v    v    v
// o -> o -> o -> o -> o
// |    |    |    |    |
// v    v    v    v    v
// o -> o -> o -> o -> o
// |    |    |    |    |
// v    v    v    v    v
// o -> o -> o -> o -> o

#include <taskflow/taskflow.hpp>
#include <taskflow/algorithm/pipeline.hpp>

int main() {

  tf::Taskflow taskflow("pipeline");
  tf::Executor executor;

  const size_t num_lines = 4;

  // create data storage
  std::array<size_t, num_lines> buffer;

  // define the pipe callable
  auto pipe_callable = [&buffer] (tf::Pipeflow& pf) mutable {
    switch(pf.pipe()) {
      // 第一阶段仅生成 5 个调度令牌并将令牌编号保存到缓冲区中。
      case 0: {
        if(pf.token() == 5) {
          pf.stop();
        }
        else {
          printf("stage 1: input token = %zu\n", pf.token());
          buffer[pf.line()] = pf.token();
        }
        return;
      }
      break;

      // 其他阶段将先前的结果传播到此管道并将其递增 1
      default: {
        printf(   "stage %zu: input buffer[%zu] = %zu\n", pf.pipe(), pf.line(), buffer[pf.line()] );
        buffer[pf.line()] = buffer[pf.line()] + 1;
      }
      break;
    }
  };

  // create a vector of three pipes
  std::vector< tf::Pipe<std::function<void(tf::Pipeflow&)>> > pipes;

  for(size_t i=0; i<3; i++) {
    pipes.emplace_back(tf::PipeType::SERIAL, pipe_callable);
  }

  // create a pipeline of four parallel lines using the given vector of pipes
  tf::ScalablePipeline<decltype(pipes)::iterator> pl(num_lines, pipes.begin(), pipes.end());


  tf::Task init = taskflow.emplace([](){ std::cout << "ready\n"; }).name("starting pipeline");
  tf::Task task = taskflow.composed_of(pl).name("pipeline");
  tf::Task stop = taskflow.emplace([](){ std::cout << "stopped\n"; }).name("pipeline stopped");
 
  init.precede(task);
  task.precede(stop);
 
  taskflow.dump(std::cout);
 
  executor.run(taskflow).wait();

  // 将管道重置为五个管道的新范围并从初始状态开始（即令牌从零开始计数）
  for(size_t i=0; i<2; i++) {
    pipes.emplace_back(tf::PipeType::SERIAL, pipe_callable);
  }
  pl.reset(pipes.begin(), pipes.end());

  executor.run(taskflow).wait();

  return 0;
}



