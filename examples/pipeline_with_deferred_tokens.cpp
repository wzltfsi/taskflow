// 这个程序演示了如何创建一个管道调度框架，将当前调度令牌的执行推迟到 future 
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

// 调度令牌具有以下依赖项：
//    ___________
//   |           |
//   V _____     |
//   |     |     | 
//   |     V     | 
// 1 2 3 4 5 6 7 8 9 10 
//         ^   |   |
//         |___|   |
//         ^       | 
//         |_______|
//
// 2 is deferred by 8
// 5 is dieferred by 2, 7, and 9


#include <taskflow/taskflow.hpp>
#include <taskflow/algorithm/pipeline.hpp>

int main() {

  tf::Taskflow taskflow("deferred_pipeline");
  tf::Executor executor;

  const size_t num_lines = 4;

  //  管道由三个管道（串行-并行-串行）和最多四个并发调度令牌组成
  tf::Pipeline pl(num_lines,
    tf::Pipe{tf::PipeType::SERIAL, [](tf::Pipeflow& pf) {
      // 只生成 15 个调度令牌
      if(pf.token() == 15) {
        pf.stop();
      }
      else {
        // 令牌 5 被推迟
        if (pf.token() == 5) {
          switch(pf.num_deferrals()) {
            case 0:
              pf.defer(2);
              printf("1st-time: Token %zu is deferred by 2\n", pf.token());
              pf.defer(7);
              printf("1st-time: Token %zu is deferred by 7\n", pf.token());
              return;  
            break;

            case 1:
              pf.defer(9);
              printf("2nd-time: Token %zu is deferred by 9\n", pf.token());
              return;
            break;

            case 2:
              printf("3rd-time: Tokens 2, 7 and 9 resolved dependencies for token %zu\n", pf.token());
            break;
          }
        }
        else if (pf.token() == 2) {
          switch(pf.num_deferrals()) {
            case 0:
              pf.defer(8);
              printf("1st-time: Token %zu is deferred by 8\n", pf.token());
            break;
            case 1:
              printf("2nd-time: Token 8 resolved dependencies for token %zu\n", pf.token());
            break;
          }
        }
        else {
          printf("stage 1: Non-deferred token %zu\n", pf.token());
        }
      }
    }},

    tf::Pipe{tf::PipeType::SERIAL, [](tf::Pipeflow& pf) {
      printf("stage 2: input token %zu (deferrals=%zu)\n", pf.token(), pf.num_deferrals());
    }},

    tf::Pipe{tf::PipeType::SERIAL, [](tf::Pipeflow& pf) {
      printf("stage 3: input token %zu\n", pf.token());
    }}
  );
   
   
  tf::Task init = taskflow.emplace([](){ std::cout << "ready\n"; }).name("starting pipeline");
  tf::Task task = taskflow.composed_of(pl).name("deferred_pipeline");
  tf::Task stop = taskflow.emplace([](){ std::cout << "stopped\n"; }).name("pipeline stopped");
 
  init.precede(task);
  task.precede(stop);
   
  taskflow.dump(std::cout);
 
  executor.run(taskflow).wait();
  
  return 0;
}


