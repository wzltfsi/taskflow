#pragma once

#include "../taskflow.hpp"

/**
@file pipeline.hpp
@brief pipeline include file
*/

namespace tf {


// ----------------------------------------------------------------------------
// Structure Definition: DeferredPipeflow
// ----------------------------------------------------------------------------
// For example: 
// 12.defer(7); 12.defer(16);
//        _____
//       |     |
//       v     |
// 7    12    16
// |     ^
// |____ |
//
// DeferredPipeflow dpf of 12 :
// dpf._token = 12;
// dpf._num_deferrals = 1;
// dpf._dependents = std::list<size_t>{7,16};
// dpf._dependent_satellites has following two entries
// {key: 7, value: dpf._dependents.begin()} 
// {key: 16, value: dpf._dependents.begin()+1}
//
/** @private */
class DeferredPipeflow {

  template <typename... Ps>
  friend class Pipeline;
  
  template <typename P>
  friend class ScalablePipeline;
  
  public:
  
    DeferredPipeflow() = default;
    DeferredPipeflow(const DeferredPipeflow&) = delete;
    DeferredPipeflow(DeferredPipeflow&&) = delete;
  
    DeferredPipeflow(size_t t, size_t n, std::unordered_set<size_t>&& dep) : 
      _token{t}, _num_deferrals{n}, _dependents{std::move(dep)} {
    }
  
    DeferredPipeflow& operator = (const DeferredPipeflow&) = delete;
    DeferredPipeflow& operator = (DeferredPipeflow&&) = delete;
  
  private:
  
    // token id
    size_t _token;
  
    // number of deferrals
    size_t _num_deferrals;  
  
    // dependents
    // For example,
    // 12.defer(7); 12.defer(16)
    // _dependents = {7, 16}
    std::unordered_set<size_t> _dependents;
};



// ----------------------------------------------------------------------------
// Class Definition: Pipeflow
// ----------------------------------------------------------------------------

/**
@class Pipeflow

@brief class to create a pipeflow object used by the pipe callable

Pipeflow 表示管道调度框架中的 调度令牌 。 管道流由管道调度程序在运行时创建以传递给管道可调用对象。 
用户可以查询该调度令牌的当前统计信息，包括线路标识、管道标识和令牌标识，并基于这些统计信息构建自己的应用算法。 
在第一阶段，用户可以显式调用 stop 方法来停止管道调度程序。

@code{.cpp}
tf::Pipe{tf::PipeType::SERIAL, [](tf::Pipeflow& pf){
  std::cout << "token id=" << pf.token()
            << " at line=" << pf.line()
            << " at pipe=" << pf.pipe()
            << '\n';
}};
@endcode

Pipeflow 只能由 tf::Pipeline 私下创建，并通过可调用管道使用。
*/
class Pipeflow {

  template <typename... Ps>
  friend class Pipeline;

  template <typename P>
  friend class ScalablePipeline;

  template <typename... Ps>
  friend class DataPipeline;

  public:
 
  Pipeflow() = default;

  //  queries the line identifier of the present token
  size_t line() const {
    return _line;
  }

  // @brief queries the pipe identifier of the present token
  size_t pipe() const {
    return _pipe;
  }

  // @brief queries the token identifier
  size_t token() const {
    return _token;
  }

  /**
  @brief stops the pipeline scheduling
 只有第一个管道可以调用此方法来停止管道。 从其他管道调用停止将抛出异常。
  */
  void stop() {
    if(_pipe != 0) {
      TF_THROW("only the first pipe can stop the token");
    }
    _stop = true;
  }

  //   @brief queries the number of deferrals
  size_t num_deferrals() const {
    return _num_deferrals;
  }

  /**
  @brief pushes token in _dependents
 只有第一个管道可以调用此方法将当前调度令牌推迟到给定令牌。
  */
  void defer(size_t token) {
    if(_pipe != 0) {
      TF_THROW("only the first pipe can defer the current scheduling token");
    }
    _dependents.insert(token);
  }
  
  private:
 
  size_t _line;
  size_t _pipe;
  size_t _token;
  bool   _stop;
  
  // 令牌依赖的数据字段
  size_t _num_deferrals; 
  std::unordered_set<size_t> _dependents; 

};



// ----------------------------------------------------------------------------
// Class Definition: PipeType
// ----------------------------------------------------------------------------
enum class PipeType : int {
  PARALLEL = 1,
  SERIAL   = 2
};



// ----------------------------------------------------------------------------
// Class Definition: Pipe
// ----------------------------------------------------------------------------

/**
@class Pipe

@brief class to create a pipe object for a pipeline stage

@tparam C callable type

pipe  代表 pipeline 的一个阶段。pipe  可以是 并行方向 或 串行方向（由 tf::PipeType 指定），并与可调用管道耦合以供 pipeline 调度程序调用。 
可调用对象必须在第一个参数中采用引用的 tf::Pipeflow 对象：

@code{.cpp}
Pipe{PipeType::SERIAL, [](tf::Pipeflow&){}}
@endcode

pipeflow 对象用于查询 pipeline 中某个调度令牌的统计信息，如管道、线路、令牌号等。
*/
template <typename C = std::function<void(tf::Pipeflow&)>>
class Pipe {

  template <typename... Ps>
  friend class Pipeline;

  template <typename P>
  friend class ScalablePipeline;

  public:
 
  using callable_t = C;
 
  Pipe() = default;

  /**
  @brief constructs the pipe object

  @param d pipe type (tf::PipeType)
  @param callable callable type

   构造函数构造具有给定方向（tf::PipeType::SERIAL 或 tf::PipeType::PARALLEL）和给定可调用项的 pipe 。 
   可调用对象必须在第一个参数中采用引用的 tf::Pipeflow 对象。

  @code{.cpp}
  Pipe{PipeType::SERIAL, [](tf::Pipeflow&){}}
  @endcode

  创建管道时，第一个管道的方向必须是串行的（tf::PipeType::SERIAL）
  */
  Pipe(PipeType d, C&& callable) :
    _type{d}, _callable{std::forward<C>(callable)} {
  }

  /**
   @brief queries the type of the pipe
   返回可调用对象的类型。
  */
  PipeType type() const {
    return _type;
  }

  /**
  @brief assigns a new type to the pipe
  */
  void type(PipeType type) {
    _type = type;
  }

  /**
  @brief assigns a new callable to the pipe

  @tparam U callable type
  @param callable a callable object constructible from std::function<void(tf::Pipeflow&)>

   使用万能转发将新的可调用对象分配给 pipe 
  */
  template <typename U>
  void callable(U&& callable) {
    _callable = std::forward<U>(callable);
  }

  private:

  PipeType _type;
  C _callable;
};



// ----------------------------------------------------------------------------
// Class Definition: Pipeline
// ----------------------------------------------------------------------------

/**
@class Pipeline

@brief class to create a pipeline scheduling framework

@tparam Ps pipe types

pipeline 是一个可组合的图形对象，供用户使用 taskflow 中的模块任务创建 pipeline scheduling framework  
与传统的流水线编程框架（例如 Intel TBB）不同， Taskflow 的流水线算法不提供任何数据抽象，
这通常会限制用户优化其应用程序中的数据布局，而是一个灵活的框架，供用户在我们的流水线之上自定义其应用程序数据 调度。 
以下代码创建了一个由四个并行线组成的管道，以通过三个串行管道调度令牌： 

@code{.cpp}
tf::Taskflow taskflow;
tf::Executor executor;

const size_t num_lines = 4;  // 4 个 选择
const size_t num_pipes = 3;  // 3 个 流水线阶段

// create a custom data buffer
std::array<std::array<int, num_pipes>, num_lines> buffer;

// 创建四个并发线和三个串行管道的管道图
tf::Pipeline pipeline(num_lines,
  // 第一个管道必须定义一个串行方向
  tf::Pipe{tf::PipeType::SERIAL, [&buffer](tf::Pipeflow& pf) {
    // 仅生成 5 个调度令牌
    if(pf.token() == 5) {
      pf.stop();
    }
    // 将令牌 ID 保存到缓冲区中
    else {
      buffer[pf.line()][pf.pipe()] = pf.token();
    }
  }},
  tf::Pipe{tf::PipeType::SERIAL, [&buffer] (tf::Pipeflow& pf) {
  // 通过添加一个将先前的结果传播到此管道
    buffer[pf.line()][pf.pipe()] = buffer[pf.line()][pf.pipe()-1] + 1;
  }},
  tf::Pipe{tf::PipeType::SERIAL, [&buffer](tf::Pipeflow& pf){
   // 通过添加一个将先前的结果传播到此管道
    buffer[pf.line()][pf.pipe()] = buffer[pf.line()][pf.pipe()-1] + 1;
  }}
);

// build the pipeline graph using composition
tf::Task init = taskflow.emplace([](){ std::cout << "ready\n"; }).name("starting pipeline");
tf::Task task = taskflow.composed_of(pipeline).name("pipeline");
tf::Task stop = taskflow.emplace([](){ std::cout << "stopped\n"; }).name("pipeline stopped");
 
init.precede(task);
task.precede(stop);
 
executor.run(taskflow).wait();
@endcode

上面的示例创建了一个管道图，它以循环方式在四条平行线上调度五个令牌，如下所示：

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

在每个管道阶段，程序通过将结果添加到存储在自定义数据存储 @c 缓冲区 中的结果来将结果传播到下一个管道。 
管道调度程序将生成五个调度令牌，然后停止。 在内部，tf::Pipeline 使用 std::tuple 来存储给定的管道序列。 
每个管道的定义可以不同，完全由编译器优化对象布局决定。 管道建成后，无法更改其管道。 如果应用程序需要更改这些管道，请使用 tf::ScalablePipeline。
*/
template <typename... Ps>
class Pipeline {

  static_assert(sizeof...(Ps)>0, "must have at least one pipe");
 
  struct Line {
    std::atomic<size_t> join_counter;
  };

  struct PipeMeta {
    PipeType type;
  };

  public:

  /**
  @brief constructs a pipeline object

  @param num_lines the number of parallel lines
  @param ps a list of pipes

   构建最多 num_lines parallel lines  的 pipeline ，以通过给定的  linear chain of pipes 安排令牌。
   第一个管道必须定义串行方向 (tf::PipeType::SERIAL) 否则将抛出异常。
  */
  Pipeline(size_t num_lines, Ps&&... ps);


  /**
  @brief constructs a pipeline object

  @param num_lines the number of parallel lines
  @param ps a tuple of pipes

 构建最多  num_lines  parallel lines  的 pipeline ，以通过给定的  linear chain of pipes 安排令牌。
 第一个管道必须定义串行方向 (tf::PipeType::SERIAL) 否则将抛出异常。
  */
  Pipeline(size_t num_lines, std::tuple<Ps...>&& ps);


  /**
  @brief queries the number of parallel lines
  该函数返回用户在构建管道时给出的parallel lines  的 pipeline。 行数表示该流水线可以达到的最大并行度。
  */
  size_t num_lines() const noexcept;


  /**
  @brief queries the number of pipes
   该函数返回用户在构建 pipeline 时给出的 pipes 数。 
  */
  constexpr size_t num_pipes() const noexcept;


  /**
  @brief resets the pipeline
 将 pipeline 重置为初始状态。 重置 pipeline 后，其令牌标识符将从零开始，就像 pipeline 刚刚构建一样。
  */
  void reset();


  /**
  @brief queries the number of generated tokens in the pipeline
  该数字表示到目前为止 pipeline 已生成的总调度令牌
  */
  size_t num_tokens() const noexcept;


  /**
  @brief obtains the graph object associated with the pipeline construct
  此方法主要用作创建此 pipeline 的模块任务的不透明数据结构。
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
  
  // queue of ready tokens (paired with their deferral times) 
  // 例如，当 12 没有任何依赖时，我们将 12 放入 _ready_tokens 队列 假设 12 的 num_deferrals 为 1，我们将 pair{12, 1} 放入队列
  std::queue<std::pair<size_t, size_t>> _ready_tokens;

  // unordered_map of token dependencies
  // 例如:  12.defer(16); 13.defer(16);  _token_dependencies 具有以下条目 {key: 16, value: std::vector{12, 13}}.
  std::unordered_map<size_t, std::vector<size_t>> _token_dependencies;
  
  // unordered_map of deferred tokens
  // 例如:  12.defer(16); 13.defer(16);
  // _deferred_tokens 有以下两个条目
  // {key: 12, DeferredPipeflow of 12} and
  // {key: 13, DeferredPipeflow of 13}
  std::unordered_map<size_t, DeferredPipeflow> _deferred_tokens;
  
  // variable to keep track of the longest deferred tokens
  // 例如: 
  // 2.defer(16)
  // 5.defer(19)
  // 5.defer(17),
  // _longest_deferral 将为 19 - 在令牌 19 之后，管道处理延迟管道流的成本几乎为零
  size_t _longest_deferral = 0;  
  

  template <size_t... I>
  auto _gen_meta(std::tuple<Ps...>&&, std::index_sequence<I...>);

  void _on_pipe(Pipeflow&, Runtime&);
  void _build();
  void _check_dependents(Pipeflow&);
  void _construct_deferred_tokens(Pipeflow&);
  void _resolve_token_dependencies(Pipeflow&); 
};


// constructor
template <typename... Ps>
Pipeline<Ps...>::Pipeline(size_t num_lines, Ps&&... ps) :
  _pipes     {std::make_tuple(std::forward<Ps>(ps)...)},
  _meta      {PipeMeta{ps.type()}...},
  _lines     (num_lines),
  _tasks     (num_lines + 1),
  _pipeflows (num_lines) {

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
Pipeline<Ps...>::Pipeline(size_t num_lines, std::tuple<Ps...>&& ps) :
  _pipes     {std::forward<std::tuple<Ps...>>(ps)},
  _meta      {_gen_meta( std::forward<std::tuple<Ps...>>(ps), std::make_index_sequence<sizeof...(Ps)>{})},
  _lines     (num_lines),
  _tasks     (num_lines + 1),
  _pipeflows (num_lines) {

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
auto Pipeline<Ps...>::_gen_meta(std::tuple<Ps...>&& ps, std::index_sequence<I...>) {
  return std::array{PipeMeta{std::get<I>(ps).type()}...};
}

// Function: num_lines
template <typename... Ps>
size_t Pipeline<Ps...>::num_lines() const noexcept {
  return _pipeflows.size();
}

// Function: num_pipes
template <typename... Ps>
constexpr size_t Pipeline<Ps...>::num_pipes() const noexcept {
  return sizeof...(Ps);
}

// Function: num_tokens
template <typename... Ps>
size_t Pipeline<Ps...>::num_tokens() const noexcept {
  return _num_tokens;
}

// Function: graph
template <typename... Ps>
Graph& Pipeline<Ps...>::graph() {
  return _graph;
}

// Function: reset
template <typename... Ps>
void Pipeline<Ps...>::reset() {

  _num_tokens = 0;

  for(size_t l = 0; l<num_lines(); l++) {
    _pipeflows[l]._pipe = 0;
    _pipeflows[l]._line = l;
    
    _pipeflows[l]._num_deferrals = 0;
    _pipeflows[l]._dependents.clear();
  }
  
  assert(_ready_tokens.empty() == true);
  _token_dependencies.clear();
  _deferred_tokens.clear();

  _lines[0][0].join_counter.store(0, std::memory_order_relaxed);

  for(size_t l = 1; l < num_lines(); l++) {
    for(size_t f = 1; f < num_pipes(); f++) {
      _lines[l][f].join_counter.store(static_cast<size_t>(_meta[f].type), std::memory_order_relaxed);
    }
  }

  for(size_t f = 1; f < num_pipes(); f++) {
    _lines[0][f].join_counter.store(1, std::memory_order_relaxed);
  }

  for(size_t l = 1; l<num_lines(); l++) {
    _lines[l][0].join_counter.store(static_cast<size_t>(_meta[0].type) - 1, std::memory_order_relaxed);
  }
}


// Procedure: _on_pipe
template <typename... Ps>
void Pipeline<Ps...>::_on_pipe(Pipeflow& pf, Runtime& rt) {
  visit_tuple([&](auto&& pipe){
    using callable_t = typename std::decay_t<decltype(pipe)>::callable_t;
    if constexpr (std::is_invocable_v<callable_t, Pipeflow&>) {
      pipe._callable(pf);
    }
    else if constexpr(std::is_invocable_v<callable_t, Pipeflow&, Runtime&>) {
      pipe._callable(pf, rt);
    }
    else {
      static_assert(dependent_false_v<callable_t>, "un-supported pipe callable type");
    }
  }, _pipes, pf._pipe);
}

// Procedure: _check_dependents
// 在 on_pipe 之后检查并移除无效的依赖。 例如，用户可能将一个 pipeflow 延迟到多个令牌，我们需要删除无效令牌。
//   12.defer(7);   // 仅当 7 被推迟时才有效，否则无效
//   12.defer(16);  // 16 有效
template <typename... Ps>
void Pipeline<Ps...>::_check_dependents(Pipeflow& pf) {
  //if (pf._dependents.size()) {
  ++pf._num_deferrals;
  
  for (auto it = pf._dependents.begin(); it != pf._dependents.end();) {
 
    // valid (e.g., 12.defer(16)) 
    if (*it >= _num_tokens) {
      _token_dependencies[*it].push_back(pf._token);
      _longest_deferral = std::max(_longest_deferral, *it);
      ++it;
    }
    // valid or invalid (e.g., 12.defer(7))
    else {
      auto pit = _deferred_tokens.find(*it);
      
      // valid (e.g., 7 is deferred)
      if (pit != _deferred_tokens.end()) {
        _token_dependencies[*it].push_back(pf._token);
        ++it;
      }

      // invalid (e.g., 7 is finished - this this 12.defer(7) is dummy)
      else {
        it = pf._dependents.erase(it);
      }
    }
  }
}

// Procedure: _construct_deferred_tokens
// 为延迟令牌构建数据结构    
// 
// 例如，  12.defer(7); 12.defer(16);
// _check_dependents之后，需要延迟 12 , 所以我们用 hashmap 为 12 构造一个数据结构：
// {key: 12, value: DeferredPipeflow of 12}
template <typename... Ps>
void Pipeline<Ps...>::_construct_deferred_tokens(Pipeflow& pf) {
  _deferred_tokens.emplace(
    std::piecewise_construct,
    std::forward_as_tuple(pf._token),
    std::forward_as_tuple( pf._token, pf._num_deferrals, std::move(pf._dependents))
  );

  //assert(res.second == true);
}

// Procedure: _resolve_token_dependencies
// 解决延迟到当前令牌的令牌的依赖关系
// 例如，  12.defer(16);  13.defer(16);    ,  _token_dependencies 将有条目
// {key: 16, value: std::vector{12, 13}} 
//
// 当 16 完成后，我们需要从 12 和 13 的 individual_dependents 中删除 16
template <typename... Ps>
void Pipeline<Ps...>::_resolve_token_dependencies(Pipeflow& pf) {

  if (auto it = _token_dependencies.find(pf._token); it != _token_dependencies.end()) {
    
    // iterate tokens that defer to pf._token  (e.g., 12 and 13)
    for(size_t target : it->second) {

      auto dpf = _deferred_tokens.find(target);

      assert(dpf != _deferred_tokens.end());

      // 从目标的 _dependents 中删除 pf._token（例如，从 12 的依赖项中删除 16）
      dpf->second._dependents.erase(pf._token);
  

      // target has no dependents
      if (dpf->second._dependents.empty()) {

        // push target into _ready_tokens queue
        _ready_tokens.emplace(dpf->second._token, dpf->second._num_deferrals);
      
        // erase target from _deferred_tokens
        _deferred_tokens.erase(dpf);
      }
    }

    // 从令牌依赖项中删除 pf.token 
    //（例如，从 _token_dependencies 中删除条目 {key: 16, value: std::vector{12, 13}}） 
    _token_dependencies.erase(it);
  }
}




// Procedure: _build
template <typename... Ps>
void Pipeline<Ps...>::_build() {

  using namespace std::literals::string_literals;

  FlowBuilder fb(_graph);

  // init task
  _tasks[0] = fb.emplace([this]() {return static_cast<int>(_num_tokens % num_lines()); }).name("cond");

  // line task
  for(size_t l = 0; l < num_lines(); l++) {

    _tasks[l + 1] = fb.emplace([this, l] (tf::Runtime& rt) mutable {

      auto pf = &_pipeflows[l];



      pipeline:

      _lines[pf->_line][pf->_pipe].join_counter.store(  static_cast<size_t>(_meta[pf->_pipe].type), std::memory_order_relaxed);
      
      // 第一个管道完成初始化和令牌依赖的所有工作   
      if (pf->_pipe == 0) {
        // _ready_tokens 队列不为空 用队列前面的令牌替换 pf
        if (!_ready_tokens.empty()) {
          pf->_token = _ready_tokens.front().first;
          pf->_num_deferrals = _ready_tokens.front().second;
          _ready_tokens.pop();
        }
        else {
          pf->_token = _num_tokens;
          pf->_num_deferrals = 0;
        }
      


      handle_token_dependency: 

        if (pf->_stop = false, _on_pipe(*pf, rt); pf->_stop == true) {
          // 在这里， pipeline  还没有停止，因为其他 lines of tasks 可能仍在运行它们的最后阶段   
          return;
        }
        
        if (_num_tokens == pf->_token) {
          ++_num_tokens;
        }
      
        if (pf->_dependents.empty() == false){ 
          //检查 pf->_dependents 是否有有效的依赖项
          _check_dependents(*pf); 
          
          // pf->_dependents 中的标记都是有效的依赖项  
          if (pf->_dependents.size()) {
            
            // 在_deferred_tokens中为pf构造一个数据结构   
            _construct_deferred_tokens(*pf);
            goto pipeline;
          }

          // pf->_dependents 中的 tokens 是无效的 dependents ,直接在同一行转到 on_pipe
          else {
            goto handle_token_dependency;
          }
        }
        
        // deferral range 内的每个令牌都需要检查它是否可以解决对其他令牌的依赖性
        if (pf->_token <= _longest_deferral) {
          _resolve_token_dependencies(*pf); 
        }
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
      //
      // a -> b
      // |    |
      // v    v
      // c -> d
      //
      // d 将由 c 或 b 生成，因此如果 c 更改 f 但 b 生成 d，则将发生 f 上的数据竞争

      std::array<int, 2> retval;
      size_t n = 0;

      // downward dependency
      if(_meta[c_f].type == PipeType::SERIAL && _lines[n_l][c_f].join_counter.fetch_sub( 1, std::memory_order_acq_rel) == 1) {
        retval[n++] = 1;
      }

      // forward dependency
      if(_lines[pf->_line][n_f].join_counter.fetch_sub( 1, std::memory_order_acq_rel) == 1 ) {
        retval[n++] = 0;
      }
      
      // 注意 task 索引从 1 开始  
      switch(n) {
        case 2: {
          rt.schedule(_tasks[n_l+1]);
          goto pipeline;
        }
        case 1: {
          // downward dependency 
          if (retval[0] == 1) {
            pf = &_pipeflows[n_l];
          }
          // forward dependency
          goto pipeline;
        }
      }
    }).name("rt-"s + std::to_string(l));

    _tasks[0].precede(_tasks[l+1]);
  }
}

// ----------------------------------------------------------------------------
// Class Definition: ScalablePipeline
// ----------------------------------------------------------------------------

/**
@class ScalablePipeline

@brief class to create a scalable pipeline object

@tparam P type of the iterator to a range of pipes

A scalable pipeline 是一个可组合的图形对象，供用户使用 taskflow 中的模块任务创建 pipeline scheduling framework 
与在构造时实例化所有 pipes 的 tf::Pipeline 不同，tf::ScalablePipeline 允许使用范围迭代器对 pipes 进行变量分配
用户还可以在运行之间将 scalable pipeline  重置为不同范围的管道。 
以下代码创建了一个由 four parallel lines 组成的  scalable pipeline ，以通过自定义存储中的 three serial pipes 来安排令牌，然后将管道重置为 five serial pipes 的新范围：
   
@code{.cpp}
tf::Taskflow taskflow("pipeline");
tf::Executor executor;

const size_t num_lines = 4;

// create data storage
std::array<int, num_lines> buffer;

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
      printf(  "stage %zu: input buffer[%zu] = %d\n", pf.pipe(), pf.line(), buffer[pf.line()]  );
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

// 根据给定的 vector of pipes 创建一条由 four parallel lines 组成的 pipeline 
tf::ScalablePipeline pl(num_lines, pipes.begin(), pipes.end());

 
tf::Task init = taskflow.emplace([](){ std::cout << "ready\n"; }).name("starting pipeline");
tf::Task task = taskflow.composed_of(pl).name("pipeline");
tf::Task stop = taskflow.emplace([](){ std::cout << "stopped\n"; }).name("pipeline stopped");
 
init.precede(task);
task.precede(stop);
 
taskflow.dump(std::cout);
 
executor.run(taskflow).wait();

// 将 pipeline  重置为 five pipes 的新范围并从初始状态开始（即令牌从零开始计数）
for(size_t i=0; i<2; i++) {
  pipes.emplace_back(tf::PipeType::SERIAL, pipe_callable);
}
pl.reset(pipes.begin(), pipes.end());

executor.run(taskflow).wait();
@endcode

上面的示例创建了一个管道图，它以循环方式在四条平行线上调度五个令牌，首先经过三个串行管道，然后经过五个串行管道：
 
@code{.shell-session}
# initial construction of three serial pipes
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

# 重置为五个串行管道的新范围
o -> o -> o -> o -> o
|    |    |    |    |
v    v    v    v    v
o -> o -> o -> o -> o
|    |    |    |    |
v    v    v    v    v
o -> o -> o -> o -> o
|    |    |    |    |
v    v    v    v    v
o -> o -> o -> o -> o
@endcode

每个管道都有相同类型的 “%tf::Pipe<%std::function<void(%tf::Pipeflow&)>>”，并保存在一个可以更改的向量中。 
我们使用指向向量开头和结尾的两个范围迭代器构建可扩展管道。 
在每个管道阶段，程序通过将结果添加到存储在自定义数据存储 @c 缓冲区 中的结果来将结果传播到下一个管道。 
管道调度程序将生成五个调度令牌，然后停止。 可扩展管道是 move-only.
*/
template <typename P>
class ScalablePipeline {
 
  struct Line {
    std::atomic<size_t> join_counter;
  };

  public:
 
  using pipe_t = typename std::iterator_traits<P>::value_type;
 
  ScalablePipeline() = default;

  /**
  @brief constructs an empty scalable pipeline object

  @param num_lines the number of parallel lines
   空的可扩展管道没有任何管道。 在运行之前，需要将管道重置为有效的管道范围。
  */
  ScalablePipeline(size_t num_lines);

  /**
  @brief constructs a scalable pipeline object

  @param num_lines the number of parallel lines
  @param first iterator to the beginning of the range
  @param last iterator to the end of the range

使用  num_lines 平行线从 [first, last)  中指定的给定 pipeline 范围构造 pipeline
第一个 pipe 必须定义串行方向 (tf::PipeType::SERIAL) 否则将抛出异常。
在内部，可伸缩管道从指定范围复制迭代器。 
这些迭代器指向的那些 pipe 可调用对象必须在 pipeline 执行期间保持有效。 
  */
  ScalablePipeline(size_t num_lines, P first, P last);

  //  @brief disabled copy constructor
  ScalablePipeline(const ScalablePipeline&) = delete;

  /**
  @brief move constructor
  使用移动语义从给定的 rhs 构造一个管道（即将  rhs 中的数据移动到此管道中）。move之后， rhs 的状态就好像刚建好的一样。 
  如果 rhs 在移动过程中运行，则行为未定义。
  */
  ScalablePipeline(ScalablePipeline&& rhs);

  //   @brief disabled copy assignment operator
  ScalablePipeline& operator = (const ScalablePipeline&) = delete;

  /**
  @brief move constructor
  使用移动语义将内容替换为 rhs 的内容（即将  rhs 中的数据移动到此管道中）。  
  move 之后， rhs 的状态就好像刚建好的一样。 如果  rhs 在移动过程中运行，则行为未定义
  */
  ScalablePipeline& operator = (ScalablePipeline&& rhs);

  /**
  @brief queries the number of parallel lines
   该函数返回用户在构建管道时给出的平行线数。 行数表示该流水线可以达到的最大并行度。
  */
  size_t num_lines() const noexcept;

  /**
  @brief queries the number of pipes
  该函数返回用户在构建 pipeline 时给出的 pipes  数。
  */
  size_t num_pipes() const noexcept;

  /**
  @brief resets the pipeline
 将 pipeline 重置为初始状态。 重置 pipeline 后，其令牌标识符将从零开始。
  */
  void reset();

  /**
  @brief resets the pipeline with a new range of pipes

  @param first iterator to the beginning of the range
  @param last iterator to the end of the range

   成员函数将管道分配给  [first, last)  中指定的新管道范围，并将管道重置为初始状态。 
   重置 pipeline 后，其令牌标识符将从零开始。 在内部，scalable pipeline  从指定范围复制迭代器。 
   这些迭代器指向的那些 pipe  可调用对象必须在 pipeline 执行期间保持有效。
  */
  void reset(P first, P last);


  /**
  @brief resets the pipeline to a new line number and a
         new range of pipes

  @param num_lines number of parallel lines
  @param first iterator to the beginning of the range
  @param last iterator to the end of the range

成员函数将管道重置为新数量的  parallel lines 和在  [first, last)  中指定的新 pipes 范围，就好像 pipeline 刚刚构建一样。 
重置 pipeline 后，其令牌标识符将从零开始。 在内部，scalable pipeline 从指定范围复制迭代器。 
这些迭代器指向的那些 pipe 可调用对象必须在 pipeline 执行期间保持有效。  
  */
  void reset(size_t num_lines, P first, P last);



  /**
  @brief queries the number of generated tokens in the pipeline
 该数字表示到目前为止 pipeline 已生成的总调度令牌。  
  */
  size_t num_tokens() const noexcept;

  /**
  @brief obtains the graph object associated with the pipeline construct
  此方法主要用作创建此管道的模块任务的不透明数据结构。 
  */
  Graph& graph();

  private:

  Graph _graph;

  size_t _num_tokens{0};

  std::vector<P>          _pipes;
  std::vector<Task>       _tasks;
  std::vector<Pipeflow>   _pipeflows;
  std::unique_ptr<Line[]> _lines;

  // chchiu
  std::queue<std::pair<size_t, size_t>>           _ready_tokens;
  std::unordered_map<size_t, std::vector<size_t>> _token_dependencies;
  std::unordered_map<size_t, DeferredPipeflow>    _deferred_tokens;
  size_t _longest_deferral = 0;
  
  void _check_dependents(Pipeflow&);
  void _construct_deferred_tokens(Pipeflow&);
  void _resolve_token_dependencies(Pipeflow&);
  // chchiu

  void _on_pipe(Pipeflow&, Runtime&);
  void _build();

  Line& _line(size_t, size_t);
};


// constructor
template <typename P>
ScalablePipeline<P>::ScalablePipeline(size_t num_lines) :
  _tasks     (num_lines + 1),
  _pipeflows (num_lines) {

  if(num_lines == 0) {
    TF_THROW("must have at least one line");
  }

  _build();
}

// constructor
template <typename P>
ScalablePipeline<P>::ScalablePipeline(size_t num_lines, P first, P last) :
  _tasks     (num_lines + 1),
  _pipeflows (num_lines) {

  if(num_lines == 0) {
    TF_THROW("must have at least one line");
  }

  reset(first, last);
  _build();
}

// move constructor
template <typename P>
ScalablePipeline<P>::ScalablePipeline(ScalablePipeline&& rhs) :
  _graph              {std::move(rhs._graph)},
  _num_tokens         {rhs._num_tokens},
  _pipes              {std::move(rhs._pipes)},
  _tasks              {std::move(rhs._tasks)},
  _pipeflows          {std::move(rhs._pipeflows)},
  _lines              {std::move(rhs._lines)},
  _ready_tokens       {std::move(rhs._ready_tokens)},
  _token_dependencies {std::move(rhs._token_dependencies)},
  _deferred_tokens    {std::move(rhs._deferred_tokens)},
  _longest_deferral   {rhs._longest_deferral}{

  rhs._longest_deferral = 0;
  rhs._num_tokens       = 0;
}

// move assignment operator
template <typename P>
ScalablePipeline<P>& ScalablePipeline<P>::operator = (ScalablePipeline&& rhs) {
  _graph                = std::move(rhs._graph);
  _num_tokens           = rhs._num_tokens;
  _pipes                = std::move(rhs._pipes);
  _tasks                = std::move(rhs._tasks);
  _pipeflows            = std::move(rhs._pipeflows);
  _lines                = std::move(rhs._lines);
  rhs._num_tokens       = 0;
  _ready_tokens         = std::move(rhs._ready_tokens);
  _token_dependencies   = std::move(rhs._token_dependencies);
  _deferred_tokens      = std::move(rhs._deferred_tokens);
  _longest_deferral     = rhs._longest_deferral;
  rhs._longest_deferral = 0;
  return *this;
}

// Function: num_lines
template <typename P>
size_t ScalablePipeline<P>::num_lines() const noexcept {
  return _pipeflows.size();
}

// Function: num_pipes
template <typename P>
size_t ScalablePipeline<P>::num_pipes() const noexcept {
  return _pipes.size();
}

// Function: num_tokens
template <typename P>
size_t ScalablePipeline<P>::num_tokens() const noexcept {
  return _num_tokens;
}

// Function: graph
template <typename P>
Graph& ScalablePipeline<P>::graph() {
  return _graph;
}


// Function: _line
template <typename P>
typename ScalablePipeline<P>::Line& ScalablePipeline<P>::_line(size_t l, size_t p) {
  return _lines[l*num_pipes() + p];
}

template <typename P>
void ScalablePipeline<P>::reset(size_t num_lines, P first, P last) {

  if(num_lines == 0) {
    TF_THROW("must have at least one line");
  }

  _graph.clear();
  _tasks.resize(num_lines + 1);
  _pipeflows.resize(num_lines);

  reset(first, last);

  _build();
}


// Function: reset
template <typename P>
void ScalablePipeline<P>::reset(P first, P last) {

  size_t num_pipes = static_cast<size_t>(std::distance(first, last));

  if(num_pipes == 0) {
    TF_THROW("pipeline cannot be empty");
  }

  if(first->type() != PipeType::SERIAL) {
    TF_THROW("first pipe must be serial");
  }

  _pipes.resize(num_pipes);

  size_t i=0;
  for(auto itr = first; itr != last; itr++) {
    _pipes[i++] = itr;
  }

  _lines = std::make_unique<Line[]>(num_lines() * _pipes.size());

  reset();
}


// Function: reset
template <typename P>
void ScalablePipeline<P>::reset() {

  _num_tokens = 0;

  for(size_t l = 0; l<num_lines(); l++) {
    _pipeflows[l]._pipe = 0;
    _pipeflows[l]._line = l;
    _pipeflows[l]._num_deferrals = 0;
    _pipeflows[l]._dependents.clear();
  }

  _line(0, 0).join_counter.store(0, std::memory_order_relaxed);

  for(size_t l=1; l<num_lines(); l++) {
    for(size_t f=1; f<num_pipes(); f++) {
      _line(l, f).join_counter.store( static_cast<size_t>(_pipes[f]->type()), std::memory_order_relaxed );
    }
  }

  for(size_t f=1; f<num_pipes(); f++) {
    _line(0, f).join_counter.store(1, std::memory_order_relaxed);
  }

  for(size_t l=1; l<num_lines(); l++) {
    _line(l, 0).join_counter.store( static_cast<size_t>(_pipes[0]->type()) - 1, std::memory_order_relaxed);
  }
  
  assert(_ready_tokens.empty() == true);
  _token_dependencies.clear();
  _deferred_tokens.clear();
}


// Procedure: _on_pipe
template <typename P>
void ScalablePipeline<P>::_on_pipe(Pipeflow& pf, Runtime& rt) {
    
  using callable_t = typename pipe_t::callable_t;

  if constexpr (std::is_invocable_v<callable_t, Pipeflow&>) {
    _pipes[pf._pipe]->_callable(pf);
  }
  else if constexpr(std::is_invocable_v<callable_t, Pipeflow&, Runtime&>) {
    _pipes[pf._pipe]->_callable(pf, rt);
  }
  else {
    static_assert(dependent_false_v<callable_t>, "un-supported pipe callable type");
  }
}


template <typename P>
void ScalablePipeline<P>::_check_dependents(Pipeflow& pf) {
  ++pf._num_deferrals;
  
  for (auto it = pf._dependents.begin(); it != pf._dependents.end();) {
 
    // valid (e.g., 12.defer(16)) 
    if (*it >= _num_tokens) {
      _token_dependencies[*it].push_back(pf._token);
      _longest_deferral = std::max(_longest_deferral, *it);
      ++it;
    }
    // valid or invalid (e.g., 12.defer(7))
    else {
      auto pit = _deferred_tokens.find(*it);
      
      // valid (e.g., 7 is deferred)
      if (pit != _deferred_tokens.end()) {
        _token_dependencies[*it].push_back(pf._token);
        ++it;
      }
      else {
        it = pf._dependents.erase(it);
      }
    }
  }
}

// Procedure: _construct_deferred_tokens
// 为延迟令牌构建数据结构 
template <typename P>
void ScalablePipeline<P>::_construct_deferred_tokens(Pipeflow& pf) {
  // 构造零拷贝的延迟管道流 
  _deferred_tokens.emplace(
    std::piecewise_construct,
    std::forward_as_tuple(pf._token),
    std::forward_as_tuple( pf._token, pf._num_deferrals, std::move(pf._dependents) )
  );
}

// Procedure: _resolve_token_dependencies
// 解决延迟到当前令牌的令牌的依赖关系
template <typename P>
void ScalablePipeline<P>::_resolve_token_dependencies(Pipeflow& pf) {

  if (auto it = _token_dependencies.find(pf._token);
      it != _token_dependencies.end()) {
    
    // iterate tokens that defer to pf._token
    for(size_t target : it->second) {

      auto dpf = _deferred_tokens.find(target);

      assert(dpf != _deferred_tokens.end());

      // erase pf._token from target's _dependents
      dpf->second._dependents.erase(pf._token);
      
      // target has no dependents
      if (dpf->second._dependents.empty()) {
        _ready_tokens.emplace(dpf->second._token, dpf->second._num_deferrals);
        _deferred_tokens.erase(dpf);
      }
    }

    _token_dependencies.erase(it);
  }
}

// Procedure: _build
template <typename P>
void ScalablePipeline<P>::_build() {

  using namespace std::literals::string_literals;

  FlowBuilder fb(_graph);

  // init task
  _tasks[0] = fb.emplace([this]() {return static_cast<int>(_num_tokens % num_lines()); }).name("cond");

  // line task
  for(size_t l = 0; l < num_lines(); l++) {

    _tasks[l + 1] = fb.emplace([this, l] (tf::Runtime& rt) mutable {

      auto pf = &_pipeflows[l];

      pipeline:

      _line(pf->_line, pf->_pipe).join_counter.store( static_cast<size_t>(_pipes[pf->_pipe]->type()), std::memory_order_relaxed);

      // 第一个 pipe 完成初始化和令牌依赖的所有工作
      if (pf->_pipe == 0) {
        // _ready_tokens 队列不为空 用队列前面的令牌替换 pf  
        if (!_ready_tokens.empty()) {
          pf->_token = _ready_tokens.front().first;
          pf->_num_deferrals = _ready_tokens.front().second;
          _ready_tokens.pop();
        }
        else {
          pf->_token = _num_tokens;
          pf->_num_deferrals = 0;
        }
      
      handle_token_dependency: 

        if (pf->_stop = false, _on_pipe(*pf, rt); pf->_stop == true) {
          // 在这里，pipeline  还没有停止，因为其他 lines of tasks  可能仍在运行它们的最后阶段
          return;
        }
        
        if (_num_tokens == pf->_token) {
          ++_num_tokens;
        }
      
        if (pf->_dependents.empty() == false){ 
          // 检查 pf->_dependents 是否有有效的依赖
          _check_dependents(*pf); 
          
          // pf->_dependents 中的标记都是有效的依赖项
          if (pf->_dependents.size()) {
            // 在 _deferred_tokens 中为 pf 构造一个数据结构 
            _construct_deferred_tokens(*pf);
            goto pipeline;
          }

          // pf->_dependents中的 tokens 是无效的依赖直接在同一行转到 on_pipe
          else {
            goto handle_token_dependency;
          }
        }
        
        // 延迟范围内的每个令牌都需要检查它是否可以解决对其他令牌的依赖性。
        if (pf->_token <= _longest_deferral) {
          _resolve_token_dependencies(*pf); 
        }
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
      //
      // a -> b
      // |    |
      // v    v
      // c -> d
      // 
      // d 将由 c 或 b 生成，因此如果 c 更改 f 但 b 生成 d，则将发生 f 上的数据竞争 

      std::array<int, 2> retval;
      size_t n = 0;

      // downward dependency
      if(_pipes[c_f]->type() == PipeType::SERIAL &&  _line(n_l, c_f).join_counter.fetch_sub(1, std::memory_order_acq_rel) == 1 ) {
        retval[n++] = 1;
      }

      // forward dependency
      if(_line(pf->_line, n_f).join_counter.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        retval[n++] = 0;
      }

      // 注意 task 索引从1开始 
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





