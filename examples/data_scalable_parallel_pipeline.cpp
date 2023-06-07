// 该程序演示了如何创建一个管道调度框架，该框架使用应用程序提供的一系列管道传播一系列整数并在每个阶段将结果加一。 管道具有以下结构：
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

    //1. How can I put a placeholder in the first pipe, i.e. [] (void, tf::Pipeflow&) in order to match the pipe vector?
    auto pipe_callable1 = [] (tf::Pipeflow& pf) mutable -> int {
        if(pf.token() == 5) {
          pf.stop();
          return 0;
        }
        else {
          printf("stage 1: input token = %zu\n", pf.token());
          return pf.token();
        }
    };
    auto pipe_callable2 = [] (int input, tf::Pipeflow& pf) mutable -> float {
        return input + 1.0;
    };
    auto pipe_callable3 = [] (float input, tf::Pipeflow& pf) mutable -> int {
        return input + 1;
    };



  //2. 当向量定义中的类型与放置元素的确切类型不同时，这可以吗？
  std::vector< ScalableDataPipeBase* > pipes;
  pipes.emplace_back(tf::make_scalable_datapipe<void, int>(tf::PipeType::SERIAL, pipe_callable1));
  pipes.emplace_back(tf::make_scalable_datapipe<int, float>(tf::PipeType::SERIAL, pipe_callable2));
  pipes.emplace_back(tf::make_scalable_datapipe<float, int>(tf::PipeType::SERIAL, pipe_callable3));

  // 使用给定的管道向量创建一条由四条平行线组成的管道   
  tf::ScalablePipeline<decltype(pipes)::iterator> pl(num_lines, pipes.begin(), pipes.end());

  // 使用组合构建管道图
  tf::Task init = taskflow.emplace([](){ std::cout << "ready\n"; }).name("starting pipeline");
  tf::Task task = taskflow.composed_of(pl).name("pipeline");
  tf::Task stop = taskflow.emplace([](){ std::cout << "stopped\n"; }).name("pipeline stopped");

  // 创建任务依赖
  init.precede(task);
  task.precede(stop);

  // / dump 流水线图结构（带组合）
  taskflow.dump(std::cout);

  // 运行 pipeline
  executor.run(taskflow).wait();

  // 将管道重置为五个管道的新范围并从初始状态开始（即令牌从零开始计数）
  pipes.emplace_back(tf::make_scalable_datapipe<int, float>(tf::PipeType::SERIAL, pipe_callable1));
  pipes.emplace_back(tf::make_scalable_datapipe<float, int>(tf::PipeType::SERIAL, pipe_callable1));
  pl.reset(pipes.begin(), pipes.end());

  executor.run(taskflow).wait();

  return 0;
}



