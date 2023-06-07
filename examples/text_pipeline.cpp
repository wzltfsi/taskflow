// 该程序演示了如何创建一个流水线调度框架来计算每个输入字符串的字符的最大出现次数。 管道具有以下结构：
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
// o -> o -> o  (string -> unordered_map<char, size_t> -> pair<char, size_t>)
//
// Input:
//   abade
//   ddddf
//   eefge
//   xyzzd
//   ijjjj
//   jiiii
//   kkijk
//
// Output:
//   a:2
//   d:4
//   e:3
//   z:2
//   j:4
//   i:4
//   k:3

#include <taskflow/taskflow.hpp>
#include <taskflow/algorithm/pipeline.hpp>

// Function: format the map
std::string format_map(const std::unordered_map<char, size_t>& map) {
  std::ostringstream oss;
  for(const auto& [i, j] : map) {
    oss << i << ':' << j << ' ';
  }
  return oss.str();
}

int main() {

  tf::Taskflow taskflow("text-processing pipeline");
  tf::Executor executor;

  const size_t num_lines = 2;

  // input data
  std::vector<std::string> input = {
    "abade",
    "ddddf",
    "eefge",
    "xyzzd",
    "ijjjj",
    "jiiii",
    "kkijk"
  };

  // custom data storage
  using data_type = std::variant< std::string, std::unordered_map<char, size_t>, std::pair<char, size_t> >;
  std::array<data_type, num_lines> buffer;

  // 管道由三个管道（串行-并行-串行）和最多两个并发调度令牌组成
  tf::Pipeline pl(num_lines,

    // 第一个管道处理输入数据
    tf::Pipe{tf::PipeType::SERIAL, [&](tf::Pipeflow& pf) {
      if(pf.token() == input.size()) {
        pf.stop();
      }
      else {
        buffer[pf.line()] = input[pf.token()];
        printf("stage 1: input token = %s\n", input[pf.token()].c_str());
      }
    }},

    // 第二个管道计算每个字符的频率
    tf::Pipe{tf::PipeType::PARALLEL, [&](tf::Pipeflow& pf) {
      std::unordered_map<char, size_t> map;
      for(auto c : std::get<std::string>(buffer[pf.line()])) {
        map[c]++;
      }
      buffer[pf.line()] = map;
      printf("stage 2: map = %s\n", format_map(map).c_str());
    }},

    // 第三个管道减少了最频繁的字符 
    tf::Pipe{tf::PipeType::SERIAL, [&buffer](tf::Pipeflow& pf) {
      auto& map = std::get<std::unordered_map<char, size_t>>(buffer[pf.line()]);
      auto sol = std::max_element(map.begin(), map.end(), [](auto& a, auto& b){
        return a.second < b.second;
      });
      printf("stage 3: %c:%zu\n", sol->first, sol->second);
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
