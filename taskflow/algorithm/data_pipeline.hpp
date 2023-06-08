#pragma once

#include "pipeline.hpp"


namespace tf {

// ----------------------------------------------------------------------------
// Class Definition: DataPipe
// ----------------------------------------------------------------------------

/**
@class DataPipe

@brief class to create a stage in a data-parallel pipeline 

数据管道代表数据并行管道的一个阶段。 数据管道可以是 并行方向 或  串行方向（由 tf::PipeType 指定），并且与管道调度程序调用的可调用对象相关联。

您需要使用模板函数 tf::make_data_pipe 来创建数据管道。 tf::DataPipe 的输入和输出类型应该是衰减类型
（尽管库总是会使用 `std::decay` 为您衰减它们）以允许内部存储工作。 
数据将通过引用传递给您的可调用对象，您可以通过复制或引用来获取它。

 
@code{.cpp}
tf::make_data_pipe<int, std::string>(
  tf::PipeType::SERIAL, 
  [](int& input) {return std::to_string(input + 100);}
);
@endcode

除了数据之外，您的 callable 还可以在第二个参数中额外引用 tf::Pipeflow 来探测阶段任务的运行时信息，例如它的行号和标记号：

@code{.cpp}
tf::make_data_pipe<int, std::string>(
  tf::PipeType::SERIAL, 
  [](int& input, tf::Pipeflow& pf) {
    printf("token=%lu, line=%lu\n", pf.token(), pf.line());
    return std::to_string(input + 100);
  }
);
@endcode

*/
template <typename Input, typename Output, typename C>
class DataPipe {

  template <typename... Ps>
  friend class DataPipeline;

  public:

  // callable type of the data pipe
  using callable_t = C;

  // input type of the data pipe
  using input_t = Input;

  // output type of the data pipe
  using output_t = Output;

  // default constructor
  DataPipe() = default;


  // constructs a data pipe :您应该使用辅助函数 tf::make_data_pipe 创建 DataPipe 对象，尤其是当您需要 tf::DataPipe 自动扣除 lambda 类型时。
  DataPipe(PipeType d, callable_t&& callable) :
    _type{d}, _callable{std::forward<callable_t>(callable)} {
  }


  // 查询 data pipe 类型  ， 数据管道可以是并行的 (tf::PipeType::PARALLEL) 或串行的  (tf::PipeType::SERIAL).
  PipeType type() const {
    return _type;
  }

  // 为数据管道分配一个新类型
  void type(PipeType type) {
    _type = type;
  }

  /**
  @brief   为 data pipe 分配一个新的可调用对象   
  @tparam U callable type
  @param callable a callable object constructible from the callable type  of this data pipe
   使用通用转发将新的可调用对象分配给管道。
  */
  template <typename U>
  void callable(U&& callable) {
    _callable = std::forward<U>(callable);
  }

  private:

  PipeType _type;

  callable_t _callable;
};

/**
@brief 构造数据管道的函数 (tf::DataPipe)

@tparam Input input data type
@tparam Output output data type
@tparam C callable type

tf::make_data_pipe 是一个辅助函数，用于在数据并行管道 (tf::DataPipeline) 中创建数据管道 (tf::DataPipe)。 
第一个参数指定数据管道的方向，tf::PipeType::SERIAL 或 tf::PipeType::PARALLEL，第二个参数是管道调度程序调用的可调用对象。 
输入和输出数据类型通过模板参数指定，库将始终将其衰减为其原始形式以进行存储。 可调用对象必须在其第一个参数中采用输入数据类型并返回输出数据类型的值。

@code{.cpp}
tf::make_data_pipe<int, std::string>(
  tf::PipeType::SERIAL, 
  [](int& input) {
    return std::to_string(input + 100);
  }
);
@endcode

可调用对象还可以引用 tf::Pipeflow，它允许您查询阶段任务的运行时信息，例如它的行号和标记号。

@code{.cpp}
tf::make_data_pipe<int, std::string>(
  tf::PipeType::SERIAL, 
  [](int& input, tf::Pipeflow& pf) {
    printf("token=%lu, line=%lu\n", pf.token(), pf.line());
    return std::to_string(input + 100);
  }
);
@endcode

*/
template <typename Input, typename Output, typename C>
auto make_data_pipe(PipeType d, C&& callable) {
  return DataPipe<Input, Output, C>(d, std::forward<C>(callable));
}




// ----------------------------------------------------------------------------
// Class Definition: DataPipeline
// ----------------------------------------------------------------------------

/**
@class DataPipeline

@brief class to create a data-parallel pipeline scheduling framework

@tparam Ps data pipe types

与 tf::Pipeline 类似，tf::DataPipeline 是一个可组合的图形对象，供用户使用任务流中的模块任务创建 数据并行管道调度框架
 唯一不同的是，tf::DataPipeline 为用户提供了一种数据抽象，可以快速表达管道中的数据流。
 以下示例创建了一个包含三个阶段的数据并行管道，这些管道生成从“void”到“int”、“std::string”、“float”和“void”的数据流。

@code{.cpp}
#include <taskflow/taskflow.hpp>
#include <taskflow/algorithm/data_pipeline.hpp>

int main() {

  // data flow => void -> int -> std::string -> float -> void 
  tf::Taskflow taskflow("pipeline");
  tf::Executor executor;

  const size_t num_lines = 4;

  tf::DataPipeline pl(num_lines,
    tf::make_data_pipe<void, int>(tf::PipeType::SERIAL, [&](tf::Pipeflow& pf) -> int{
      if(pf.token() == 5) {
        pf.stop();
        return 0;
      }
      else {
        return pf.token();
      }
    }),
    tf::make_data_pipe<int, std::string>(tf::PipeType::SERIAL, [](int& input) {
      return std::to_string(input + 100);
    }),
    tf::make_data_pipe<std::string, void>(tf::PipeType::SERIAL, [](std::string& input) {
      std::cout << input << std::endl;
    })
  );

  taskflow.composed_of(pl).name("pipeline");
  taskflow.dump(std::cout);
  executor.run(taskflow).wait();

  return 0;
}
@endcode

管道以循环方式在四条平行线上调度五个令牌，如下所示

@code{.shell-session}
o -> o -> o
|    |    |
v    v    v
o -> o -> o
|    |    |
v    v    v
o -> o -> o
|    |    |
v    v    v
o -> o -> o
@endcode
*/
template <typename... Ps>
class DataPipeline {

  static_assert(sizeof...(Ps)>0, "must have at least one pipe");

   // private
  struct Line {
    std::atomic<size_t> join_counter;
  };

  // private
  struct PipeMeta {
    PipeType type;
  };


  public:
  
  //  每个数据令牌的内部存储类型（默认 std::variant）
  using data_t = unique_variant_t<std::variant<std::conditional_t<
    std::is_void_v<typename Ps::output_t>, 
    std::monostate, 
    std::decay_t<typename Ps::output_t>>...
  >>;


  /**
  @brief constructs a data-parallel pipeline object

  @param num_lines the number of parallel lines
  @param ps a list of pipes

    构造一个最多  num_lines 平行线的数据并行管道，以通过给定的线性管道链安排令牌。 
    第一个管道必须定义串行方向 (tf::PipeType::SERIAL) 否则将抛出异常。
  */
  DataPipeline(size_t num_lines, Ps&&... ps);


  /**
  @brief constructs a data-parallel pipeline object

  @param num_lines the number of parallel lines
  @param ps a tuple of pipes

  构造最多 num_lines 条并行线的数据并行管道，以通过存储在 std::tuple 中的给定线性管道链来调度令牌。 
  第一个管道必须定义串行方向 (tf::PipeType::SERIAL) 否则将抛出异常。
  */
  DataPipeline(size_t num_lines, std::tuple<Ps...>&& ps);


  /**
  @brief queries the number of parallel lines
  该函数返回用户在构建管道时给出的平行线数。 行数表示该流水线可以达到的最大并行度。
  */
  size_t num_lines() const noexcept;


  /**
  @brief queries the number of pipes
   该函数返回用户在构建管道时给出的管道数。
  */
  constexpr size_t num_pipes() const noexcept;


  /**
  @brief resets the pipeline
   将管道重置为初始状态。 重置管道后，其令牌标识符将从零开始，就像管道刚刚构建一样。
  */
  void reset();



  /**
  @brief queries the number of generated tokens in the pipeline
  该数字表示到目前为止管道已生成的总调度令牌。
  */
  size_t num_tokens() const noexcept;

  /**
  @brief obtains the graph object associated with the pipeline construct
   此方法主要用作创建此管道的模块任务的不透明数据结构。
  */
  Graph& graph();



  private:

  Graph _graph;

  size_t _num_tokens;

  std::tuple<Ps...> _pipes;
  std::array<PipeMeta, sizeof...(Ps)> _meta;
  std::vector<std::array<Line, sizeof...(Ps)>> _lines;
  std::vector<Task> _tasks;
  std::vector<Pipeflow> _pipeflows;
  std::vector<CachelineAligned<data_t>> _buffer;

  template <size_t... I>
  auto _gen_meta(std::tuple<Ps...>&&, std::index_sequence<I...>);

  void _on_pipe(Pipeflow&, Runtime&);
  void _build();
};


// constructor
template <typename... Ps>
DataPipeline<Ps...>::DataPipeline(size_t num_lines, Ps&&... ps) :
  _pipes     {std::make_tuple(std::forward<Ps>(ps)...)},
  _meta      {PipeMeta{ps.type()}...},
  _lines     (num_lines),
  _tasks     (num_lines + 1),
  _pipeflows (num_lines),
  _buffer    (num_lines) {

  if(num_lines == 0) {
    TF_THROW("must have at least one line");
  }

  if(std::get<0>(_pipes).type() != PipeType::SERIAL) {
    TF_THROW("first pipe must be serial");
  }

  reset();
  _build();
}


// constructor
template <typename... Ps>
DataPipeline<Ps...>::DataPipeline(size_t num_lines, std::tuple<Ps...>&& ps) :
  _pipes     {std::forward<std::tuple<Ps...>>(ps)},
  _meta      {_gen_meta( std::forward<std::tuple<Ps...>>(ps), std::make_index_sequence<sizeof...(Ps)>{} )},
  _lines     (num_lines),
  _tasks     (num_lines + 1),
  _pipeflows (num_lines),
  _buffer    (num_lines) {

  if(num_lines == 0) {
    TF_THROW("must have at least one line");
  }

  if(std::get<0>(_pipes).type() != PipeType::SERIAL) {
    TF_THROW("first pipe must be serial");
  }

  reset();
  _build();
}


// Function: _get_meta
template <typename... Ps>
template <size_t... I>
auto DataPipeline<Ps...>::_gen_meta(std::tuple<Ps...>&& ps, std::index_sequence<I...>) {
  return std::array{PipeMeta{std::get<I>(ps).type()}...};
}


// Function: num_lines
template <typename... Ps>
size_t DataPipeline<Ps...>::num_lines() const noexcept {
  return _pipeflows.size();
}

// Function: num_pipes
template <typename... Ps>
constexpr size_t DataPipeline<Ps...>::num_pipes() const noexcept {
  return sizeof...(Ps);
}

// Function: num_tokens
template <typename... Ps>
size_t DataPipeline<Ps...>::num_tokens() const noexcept {
  return _num_tokens;
}

// Function: graph
template <typename... Ps>
Graph& DataPipeline<Ps...>::graph() {
  return _graph;
}

// Function: reset
template <typename... Ps>
void DataPipeline<Ps...>::reset() {

  _num_tokens = 0;

  for(size_t l = 0; l < num_lines(); l++) {
    _pipeflows[l]._pipe = 0;
    _pipeflows[l]._line = l;
  }

  _lines[0][0].join_counter.store(0, std::memory_order_relaxed);

  for(size_t l=1; l<num_lines(); l++) {
    for(size_t f=1; f<num_pipes(); f++) {
      _lines[l][f].join_counter.store(
        static_cast<size_t>(_meta[f].type), std::memory_order_relaxed
      );
    }
  }

  for(size_t f=1; f<num_pipes(); f++) {
    _lines[0][f].join_counter.store(1, std::memory_order_relaxed);
  }

  for(size_t l=1; l<num_lines(); l++) {
    _lines[l][0].join_counter.store(
      static_cast<size_t>(_meta[0].type) - 1, std::memory_order_relaxed
    );
  }
}

// Procedure: _on_pipe
template <typename... Ps>
void DataPipeline<Ps...>::_on_pipe(Pipeflow& pf, Runtime&) {

  visit_tuple([&](auto&& pipe){

    using data_pipe_t = std::decay_t<decltype(pipe)>;
    using callable_t  = typename data_pipe_t::callable_t;
    using input_t     = std::decay_t<typename data_pipe_t::input_t>;
    using output_t    = std::decay_t<typename data_pipe_t::output_t>;
    
    // 第一个管道
    if constexpr (std::is_invocable_v<callable_t, Pipeflow&>) {
      // [](tf::Pipeflow&) -> void {}, i.e., we only have one pipe
      if constexpr (std::is_void_v<output_t>) {
        pipe._callable(pf);
      // [](tf::Pipeflow&) -> output_t {}
      } else {
        _buffer[pf._line].data = pipe._callable(pf);
      }
    }
    // 第二个参数中没有 pipeflow 的其他管道
    else if constexpr (std::is_invocable_v<callable_t, std::add_lvalue_reference_t<input_t> >) {
      // [](input_t&) -> void {}, i.e., the last pipe
      if constexpr (std::is_void_v<output_t>) {
        pipe._callable(std::get<input_t>(_buffer[pf._line].data));
      // [](input_t&) -> output_t {}
      } else {
        _buffer[pf._line].data = pipe._callable( std::get<input_t>(_buffer[pf._line].data)   );
      }
    }
    // 第二个参数中带有 pipeflow 的其他管道
    else if constexpr (std::is_invocable_v<callable_t, input_t&, Pipeflow&>) {
      // [](input_t&, tf::Pipeflow&) -> void {}
      if constexpr (std::is_void_v<output_t>) {
        pipe._callable(std::get<input_t>(_buffer[pf._line].data), pf);
      // [](input_t&, tf::Pipeflow&) -> output_t {}
      } else {
        _buffer[pf._line].data = pipe._callable(std::get<input_t>(_buffer[pf._line].data), pf );
      }
    }
    else {
      static_assert(dependent_false_v<callable_t>, "un-supported pipe callable type");
    }
  }, _pipes, pf._pipe);
}



// Procedure: _build
template <typename... Ps>
void DataPipeline<Ps...>::_build() {

  using namespace std::literals::string_literals;

  FlowBuilder fb(_graph);

  // init task
  _tasks[0] = fb.emplace([this]() {
    return static_cast<int>(_num_tokens % num_lines());
  }).name("cond");

  // line task
  for(size_t l = 0; l < num_lines(); l++) {

    _tasks[l + 1] = fb.emplace([this, l] (tf::Runtime& rt) mutable {

      auto pf = &_pipeflows[l];

      pipeline:

      _lines[pf->_line][pf->_pipe].join_counter.store( static_cast<size_t>(_meta[pf->_pipe].type), std::memory_order_relaxed );

      if (pf->_pipe == 0) {
        pf->_token = _num_tokens;
        if (pf->_stop = false, _on_pipe(*pf, rt); pf->_stop == true) {
          // 在这里，管道还没有停止，因为其他任务线可能仍在运行它们的最后阶段
          return;
        }
        ++_num_tokens;
      }
      else {
        _on_pipe(*pf, rt);
      }

      size_t c_f = pf->_pipe;
      size_t n_f = (pf->_pipe + 1) % num_pipes();
      size_t n_l = (pf->_line + 1) % num_lines();

      pf->_pipe = n_f;

      // ---- scheduling starts here ----
      // 请注意，在此之后不得更改共享变量 f，因为它可能会由于以下情况导致数据竞争： 
      // a -> b
      // |    |
      // v    v
      // c -> d
      //
      // d 将由 c 或 b 生成，因此如果 c 更改 f 但 b 生成 d，则将发生 f 上的数据竞争

      std::array<int, 2> retval;
      size_t n = 0;

      // downward dependency
      if(_meta[c_f].type == PipeType::SERIAL &&  _lines[n_l][c_f].join_counter.fetch_sub(1, std::memory_order_acq_rel) == 1 ) {
        retval[n++] = 1;
      }

      // forward dependency
      if(_lines[pf->_line][n_f].join_counter.fetch_sub(1, std::memory_order_acq_rel) == 1 ) {
        retval[n++] = 0;
      }

      //  注意 task 索引从 1 开始
      switch(n) {
        case 2: {
          rt.schedule(_tasks[n_l+1]);
          goto pipeline;
        }
        case 1: {
          if (retval[0] == 1) {
            pf = &_pipeflows[n_l];
          }
          goto pipeline;
        }
      }
    }).name("rt-"s + std::to_string(l));

    _tasks[0].precede(_tasks[l+1]);
  }
}


}  // end of namespace tf -----------------------------------------------------





