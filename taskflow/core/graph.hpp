#pragma once

#include "../utility/traits.hpp"
#include "../utility/iterator.hpp"
#include "../utility/object_pool.hpp"
#include "../utility/os.hpp"
#include "../utility/math.hpp"
#include "../utility/small_vector.hpp"
#include "../utility/serializer.hpp"
#include "error.hpp"
#include "declarations.hpp"
#include "semaphore.hpp"
#include "environment.hpp"
#include "topology.hpp"
#include "tsq.hpp"

/**
@file graph.hpp
@brief graph include file
*/

namespace tf {

// ----------------------------------------------------------------------------
// Class: Graph
// ----------------------------------------------------------------------------

/**
@class Graph

@brief class to create a graph object

graph 是  task dependency graph 的最终存储，是与 executor 交互的主要网关。 
graph 管理 global object pool 中的一组节点，该对象池有效地激活和回收节点对象，而无需经过重复且昂贵的内存分配和释放。
该类主要用于在自定义类中创建一个不透明的图形对象，通过  taskflow composition 与 executor 进行交互。
 
graph object 是 move-only 的。
*/
class Graph {

  friend class Node;
  friend class FlowBuilder;
  friend class Subflow;
  friend class Taskflow;
  friend class Executor;

  public:
 
    Graph() = default;
 
    Graph(const Graph&) = delete;
 
    Graph(Graph&&);

    ~Graph();
    
    Graph& operator = (const Graph&) = delete;
 
    Graph& operator = (Graph&&);

    // 查询 graph 是否为空
    bool empty() const;

    // 查询 graph 中的节点数
    size_t size() const;

    // 清除 graph
    void clear();

  private:

    std::vector<Node*> _nodes;

    void _clear();
    void _clear_detached();
    void _merge(Graph&&);
    void _erase(Node*);
    
    /**
    @private
    */
    template <typename ...ArgsT>
    Node* _emplace_back(ArgsT&&...);
};

// ----------------------------------------------------------------------------



/**
@class Runtime

@brief class to include a runtime object in a task

runtime object 允许用户与任务内的 scheduling runtime 进行交互，例如调度活动任务、生成 subflow 等。

@code{.cpp}
tf::Task A, B, C, D;
std::tie(A, B, C, D) = taskflow.emplace(
  [] () { return 0; },
  [&C] (tf::Runtime& rt) {  // C must be captured by reference
    std::cout << "B\n";
    rt.schedule(C);
  },
  [] () { std::cout << "C\n"; },
  [] () { std::cout << "D\n"; }
);
A.precede(B, C, D);
executor.run(taskflow).wait();
@endcode

 runtime object 与  runs the task  的 worker  和 executor 相关联。 
*/
class Runtime {

  friend class Executor;
  friend class FlowBuilder;

  public:

  /**
  @brief obtains the running executor
 runtime task  的 running executor  是运行该 runtime task 的  parent taskflow  的 executor 
 
  @code{.cpp}
  tf::Executor executor;
  tf::Taskflow taskflow;
  taskflow.emplace([&](tf::Runtime& rt){
    assert(&(rt.executor()) == &executor);
  });
  executor.run(taskflow).wait();
  @endcode
  */
  Executor& executor();



  /**
  @brief schedules an active task immediately to the worker's queue

  @param task the given active task to schedule immediately


  该成员函数立即将一个  active task 调度到  runtime task 中关联的 worker 的   task queue 中。 
 active task  是正在运行的 taskflow 中的 task 
 该 task 可能正在运行也可能未运行，调度该 task 会立即将该 task 放入运行  runtime task 的工作线程的 task queue 中。 考虑以下示例：
 
  @code{.cpp}
  tf::Task A, B, C, D;
  std::tie(A, B, C, D) = taskflow.emplace(
    [] () { return 0; },
    [&C] (tf::Runtime& rt) {  // C must be captured by reference
      std::cout << "B\n";
      rt.schedule(C);
    },
    [] () { std::cout << "C\n"; },
    [] () { std::cout << "D\n"; }
  );
  A.precede(B, C, D);
  executor.run(taskflow).wait();
  @endcode

执行器会先运行条件任务  A，返回  0，通知调度器去执行运行时任务  B。
 B在执行过程中，直接调度任务 C，不经过正常 任务流图调度过程。 此时，任务 C 处于活动状态，因为它的父任务流正在运行。 当任务流完成时，我们将在输出中看到 B 和 C。
  */
  void schedule(Task task);
  


  /**
  @brief runs the given callable asynchronously

  @tparam F callable type
  @param f callable object
    

 该方法创建一个异步任务以在给定参数上启动给定函数。 与 tf::Executor::async 的区别在于创建的异步任务属于 runtime 
当 runtime joins 时，从 runtime 创建的所有异步任务都保证在 join 返回后完成。 例如：
  @code{.cpp}
  std::atomic<int> counter(0);
  taskflow.emplace([&](tf::Runtime& rt){
    auto fu1 = rt.async([&](){ counter++; });
    auto fu2 = rt.async([&](){ counter++; });
    fu1.get();
    fu2.get();
    assert(counter == 2);
    
    // 从运行时的 worker 产生 100 个异步任务
    for(int i=0; i<100; i++) {
      rt.async([&](){ counter++; });
    }
    
    // 显式加入 100 个异步任务  
    rt.join();
    assert(counter == 102);
  });
  @endcode

此方法是线程安全的，可以由持有 reference to the runtime 的多个 workers 调用。 
例如，下面的代码从一个 runtime 的 worker 中生成 100 个 tasks ，这 100 个 tasks 中的每一个都会生成另一个 将由 另一个 worker 运行的 task
   
  @code{.cpp}
  std::atomic<int> counter(0);
  taskflow.emplace([&](tf::Runtime& rt){
    for(int i=0; i<100; i++) {
      rt.async([&](){ 
        counter++; 
        rt.async([](){ counter++; });
      });
    }
    
    // 显式 join  100个异步任务
    rt.join();
    assert(counter == 200);
  });
  @endcode
  */
  template <typename F>
  auto async(F&& f);
  



  /**
  @brief 类似于 tf::Runtime::async 但为 task 分配了一个名称 

  @tparam F callable type

  @param name assigned name to the task
  @param f callable

  @code{.cpp}
  taskflow.emplace([&](tf::Runtime& rt){
    auto future = rt.async("my task", [](){});
    future.get();
  });
  @endcode
  */
  template <typename F>
  auto async(const std::string& name, F&& f);



  /**
  @brief runs the given function asynchronously without returning any future object

  @tparam F callable type
  @param f callable

该成员函数比 tf::Runtime::async 更高效，鼓励在没有数据返回时使用。这个成员函数是线程安全的。

  @code{.cpp}
  std::atomic<int> counter(0);
  taskflow.emplace([&](tf::Runtime& rt){
    for(int i=0; i<100; i++) {
      rt.silent_async([&](){ counter++; });
    }
    rt.join();
    assert(counter == 100);
  });
  @endcode
  */
  template <typename F>
  void silent_async(F&& f);
  
  /**
  @brief similar to tf::Runtime::silent_async but assigns the task a name

  @tparam F callable type
  @param name assigned name to the task
  @param f callable
  
  @code{.cpp}
  taskflow.emplace([&](tf::Runtime& rt){
    rt.silent_async("my task", [](){});
    rt.join();
  });
  @endcode
  */
  template <typename F>
  void silent_async(const std::string& name, F&& f);
  


  /**
  @brief similar to tf::Runtime::silent_async but the caller must be the worker of the runtime

  @tparam F callable type

  @param name assigned name to the task
  @param f callable

该方法绕过执行者对调用方工作线程的检查，因此只能由本 runtime 的 worker 调用。 
  @code{.cpp}
  taskflow.emplace([&](tf::Runtime& rt){
    // running by the worker of this runtime
    rt.silent_async_unchecked("my task", [](){});
    rt.join();
  });
  @endcode
  */
  template <typename F>
  void silent_async_unchecked(const std::string& name, F&& f);



  /**
  @brief co-runs the given target and waits until it completes
  
 target 可以是以下形式之一：
     + 生成 subflow 的 dynamic task 或
     + 定义了 `tf::Graph& T::graph()` 的 composable graph object  
 
  @code{.cpp}
  // co-run a subflow and wait until all tasks complete
  taskflow.emplace([](tf::Runtime& rt){
    rt.corun([](tf::Subflow& sf){
      tf::Task A = sf.emplace([](){});
      tf::Task B = sf.emplace([](){});
    }); 
  });
  
  // co-run a taskflow and wait until all tasks complete
  tf::Taskflow taskflow1, taskflow2;
  taskflow1.emplace([](){ std::cout << "running taskflow1\n"; });
  taskflow2.emplace([&](tf::Runtime& rt){
    std::cout << "running taskflow2\n";
    rt.corun(taskflow1);
  });
  executor.run(taskflow2).wait();
  @endcode

虽然 tf::Runtime::corun 会阻塞直到操作完成，但 caller thread （工作者）不会被阻塞（例如，休眠或持有任何锁）。
相反，caller thread joins  executor 的工作窃取循环，并在目标中的所有任务完成时返回。
  */
  template <typename T>
  void corun(T&& target);



  /**
  @brief keeps running the work-stealing loop until the predicate becomes true
  
  @tparam P predicate type
  @param predicate a boolean predicate to indicate when to stop the loop
该方法使 caller worker  在工作窃取循环中运行，直到停止谓词变为真。
  */
  template <typename P>
  void corun_until(P&& predicate);
  

  
  /**
  @brief joins all asynchronous tasks spawned by this runtime
立即 joins 所有异步任务（tf::Runtime::async、tf::Runtime::silent_async）。 与 tf::Subflow::join 不同，您可以从 tf::Runtime 对象 joins 多次。 
    
  @code{.cpp}
  std::atomic<size_t> counter{0};
  taskflow.emplace([&](tf::Runtime& rt){
    //  生成 100 个异步任务并 join 
    for(int i=0; i<100; i++) {
      rt.silent_async([&](){ counter++; });
    }
    rt.join();
    assert(counter == 100);
    
    // 生成另外 100 个异步任务并 join 
    for(int i=0; i<100; i++) {
      rt.silent_async([&](){ counter++; });
    }
    rt.join();
    assert(counter == 200);
  });
  @endcode

  @attention
  只有此 tf::Runtime 的工作程序才能  join.
  */
  inline void join();


  // 获取对底层 worker 的引用
  inline Worker& worker();

  protected:
   
  explicit Runtime(Executor&, Worker&, Node*);
   
  Executor& _executor;
   
  Worker& _worker;
   
  Node* _parent;
 
  template <typename F>
  auto _async(Worker& w, const std::string& name, F&& f);
   
  template <typename F>
  void _silent_async(Worker& w, const std::string& name, F&& f);
};
 
inline Runtime::Runtime(Executor& e, Worker& w, Node* p) :
  _executor{e},
  _worker  {w},
  _parent  {p}{
}
 
inline Executor& Runtime::executor() {
  return _executor;
}
 
inline Worker& Runtime::worker() {
  return _worker;
}




// ----------------------------------------------------------------------------
// Node
// ----------------------------------------------------------------------------

// private
class Node {

  friend class Graph;
  friend class Task;
  friend class TaskView;
  friend class Taskflow;
  friend class Executor;
  friend class FlowBuilder;
  friend class Subflow;
  friend class Runtime;

  enum class AsyncState : int {
    UNFINISHED = 0,
    LOCKED = 1,
    FINISHED = 2
  };

  TF_ENABLE_POOLABLE_ON_THIS;

  // state bit flag
  constexpr static int CONDITIONED = 1;
  constexpr static int DETACHED    = 2;
  constexpr static int ACQUIRED    = 4;
  constexpr static int READY       = 8;

  using Placeholder = std::monostate;


  // static work handle
  struct Static {

    template <typename C>
    Static(C&&);

    std::variant<  std::function<void()>, std::function<void(Runtime&)>  > work;
  };


  // dynamic work handle
  struct Dynamic {

    template <typename C>
    Dynamic(C&&);

    std::function<void(Subflow&)> work;
    Graph subgraph;
  };


  // condition work handle
  struct Condition {

    template <typename C>
    Condition(C&&);
    
    std::variant< std::function<int()>, std::function<int(Runtime&)> > work;
  };


  // multi-condition work handle
  struct MultiCondition {

    template <typename C>
    MultiCondition(C&&);

    std::variant< std::function<SmallVector<int>()>, std::function<SmallVector<int>(Runtime&)> > work;
  };


  // module work handle
  struct Module {

    template <typename T>
    Module(T&);

    Graph& graph;
  };


  // Async work
  struct Async {

    template <typename T>
    Async(T&&);

    std::function<void()> work;
  };
  

  // silent dependent async
  struct DependentAsync {
    
    template <typename C>
    DependentAsync(C&&);
    
    std::function<void()> work;
    
    std::atomic<AsyncState> state {AsyncState::UNFINISHED};
  };

  using handle_t = std::variant<
    Placeholder,      // placeholder
    Static,           // static tasking
    Dynamic,          // dynamic tasking
    Condition,        // conditional tasking
    MultiCondition,   // multi-conditional tasking
    Module,           // composable tasking
    Async,            // async tasking
    DependentAsync    // dependent async tasking (no future)
  >;

  struct Semaphores {
    SmallVector<Semaphore*> to_acquire;
    SmallVector<Semaphore*> to_release;
  };

  public:

  // variant index
  constexpr static auto PLACEHOLDER     = get_index_v<Placeholder, handle_t>;
  constexpr static auto STATIC          = get_index_v<Static, handle_t>;
  constexpr static auto DYNAMIC         = get_index_v<Dynamic, handle_t>;
  constexpr static auto CONDITION       = get_index_v<Condition, handle_t>;
  constexpr static auto MULTI_CONDITION = get_index_v<MultiCondition, handle_t>;
  constexpr static auto MODULE          = get_index_v<Module, handle_t>;
  constexpr static auto ASYNC           = get_index_v<Async, handle_t>;
  constexpr static auto DEPENDENT_ASYNC = get_index_v<DependentAsync, handle_t>;

  Node() = default;

  template <typename... Args>
  Node(const std::string&, unsigned, Topology*, Node*, size_t, Args&&... args);

  ~Node();

  size_t num_successors() const;
  size_t num_dependents() const;
  size_t num_strong_dependents() const;
  size_t num_weak_dependents() const;

  const std::string& name() const;

  private:

  std::string         _name;
  unsigned            _priority {0};
  Topology*           _topology {nullptr};
  Node*               _parent {nullptr};
  void*               _data {nullptr};

  SmallVector<Node*>  _successors;
  SmallVector<Node*>  _dependents;

  std::atomic<int>    _state {0};
  std::atomic<size_t> _join_counter {0};

  std::unique_ptr<Semaphores> _semaphores;
  
  handle_t _handle;

  void _precede(Node*);
  void _set_up_join_counter();

  bool _is_cancelled() const;
  bool _is_conditioner() const;
  bool _acquire_all(SmallVector<Node*>&);

  SmallVector<Node*> _release_all();
};

 
// private    Node Object Pool
inline ObjectPool<Node> node_pool;

  
// Constructor  Node::Static
template <typename C>
Node::Static::Static(C&& c) : work {std::forward<C>(c)} {}

 
// Constructor   Node::Dynamic
template <typename C>
Node::Dynamic::Dynamic(C&& c) : work {std::forward<C>(c)} {}
 

// Constructor  Node::Condition
template <typename C>
Node::Condition::Condition(C&& c) : work {std::forward<C>(c)} {}                                        



// Constructor  Node::MultiCondition
template <typename C>
Node::MultiCondition::MultiCondition(C&& c) : work {std::forward<C>(c)} {}

 

// Constructor   Node::Module
template <typename T>
inline Node::Module::Module(T& obj) : graph{ obj.graph() } {}



// Constructor   Node::Async
template <typename C>
Node::Async::Async(C&& c) : work {std::forward<C>(c)} {}
 

// Constructor  Node::DependentAsync
template <typename C>
Node::DependentAsync::DependentAsync(C&& c) : work {std::forward<C>(c)} {}
 


// Constructor   Node
template <typename... Args>
Node::Node(
  const std::string& name, 
  unsigned priority,
  Topology* topology, 
  Node* parent, 
  size_t join_counter,
  Args&&... args
) :
  _name         {name},
  _priority     {priority},
  _topology     {topology},
  _parent       {parent},
  _join_counter {join_counter},
  _handle       {std::forward<Args>(args)...} {
}

 
// Destructor
inline Node::~Node() {
  //这是为了避免 stack overflow

  if(_handle.index() == DYNAMIC) {
    // 使用 std::get_if 而不是 std::get 使其与旧的 macOS 版本兼容由于上面的索引检查，std::get_if 的结果保证为非空
    auto& subgraph = std::get_if<Dynamic>(&_handle)->subgraph;
    std::vector<Node*> nodes;
    nodes.reserve(subgraph.size());

    std::move(  subgraph._nodes.begin(), subgraph._nodes.end(), std::back_inserter(nodes) );
    subgraph._nodes.clear();

    size_t i = 0;

    while(i < nodes.size()) {

      if(nodes[i]->_handle.index() == DYNAMIC) {
        auto& sbg = std::get_if<Dynamic>(&(nodes[i]->_handle))->subgraph;
        std::move( sbg._nodes.begin(), sbg._nodes.end(), std::back_inserter(nodes) );
        sbg._nodes.clear();
      }

      ++i;
    }
 
    for(i=0; i<nodes.size(); ++i) {
      node_pool.recycle(nodes[i]);
    }
  }
}

// Procedure: _precede
inline void Node::_precede(Node* v) {
  _successors.push_back(v);
  v->_dependents.push_back(this);
}

// Function: num_successors
inline size_t Node::num_successors() const {
  return _successors.size();
}

// Function: dependents
inline size_t Node::num_dependents() const {
  return _dependents.size();
}

// Function: num_weak_dependents
inline size_t Node::num_weak_dependents() const {
  size_t n = 0;
  for(size_t i=0; i<_dependents.size(); i++) {
    if(_dependents[i]->_is_conditioner()) {
      n++;
    }
  }
  return n;
}

// Function: num_strong_dependents
inline size_t Node::num_strong_dependents() const {
  size_t n = 0;
  for(size_t i=0; i<_dependents.size(); i++) {
    if(!_dependents[i]->_is_conditioner()) {
      n++;
    }
  }
  return n;
}

// Function: name
inline const std::string& Node::name() const {
  return _name;
}

// Function: _is_conditioner
inline bool Node::_is_conditioner() const {
  return _handle.index() == Node::CONDITION ||
         _handle.index() == Node::MULTI_CONDITION;
}

// Function: _is_cancelled
inline bool Node::_is_cancelled() const {
  return _topology && _topology->_is_cancelled.load(std::memory_order_relaxed);
}

// Procedure: _set_up_join_counter
inline void Node::_set_up_join_counter() {
  size_t c = 0;
  for(auto p : _dependents) {
    if(p->_is_conditioner()) {
      _state.fetch_or(Node::CONDITIONED, std::memory_order_relaxed);
    }
    else {
      c++;
    }
  }
  _join_counter.store(c, std::memory_order_release);
}


// Function: _acquire_all
inline bool Node::_acquire_all(SmallVector<Node*>& nodes) {

  auto& to_acquire = _semaphores->to_acquire;

  for(size_t i = 0; i < to_acquire.size(); ++i) {
    if(!to_acquire[i]->_try_acquire_or_wait(this)) {
      for(size_t j = 1; j <= i; ++j) {
        auto r = to_acquire[i-j]->_release();
        nodes.insert(std::end(nodes), std::begin(r), std::end(r));
      }
      return false;
    }
  }
  return true;
}

// Function: _release_all
inline SmallVector<Node*> Node::_release_all() {

  auto& to_release = _semaphores->to_release;

  SmallVector<Node*> nodes;
  for(const auto& sem : to_release) {
    auto r = sem->_release();
    nodes.insert(std::end(nodes), std::begin(r), std::end(r));
  }

  return nodes;
}

// ----------------------------------------------------------------------------
// Node Deleter
// ----------------------------------------------------------------------------

// private
struct NodeDeleter {
  void operator ()(Node* ptr) {
    node_pool.recycle(ptr);
  }
};

// ----------------------------------------------------------------------------
// Graph definition
// ----------------------------------------------------------------------------

// Destructor
inline Graph::~Graph() {
  _clear();
}

// Move constructor
inline Graph::Graph(Graph&& other) :
  _nodes {std::move(other._nodes)} {
}

// Move assignment
inline Graph& Graph::operator = (Graph&& other) {
  _clear();
  _nodes = std::move(other._nodes);
  return *this;
}

// Procedure: clear
inline void Graph::clear() {
  _clear();
}

// Procedure: clear
inline void Graph::_clear() {
  for(auto node : _nodes) {
    node_pool.recycle(node);
  }
  _nodes.clear();
}

// Procedure: clear_detached
inline void Graph::_clear_detached() {

  auto mid = std::partition(_nodes.begin(), _nodes.end(), [] (Node* node) {
    return !(node->_state.load(std::memory_order_relaxed) & Node::DETACHED);
  });

  for(auto itr = mid; itr != _nodes.end(); ++itr) {
    node_pool.recycle(*itr);
  }
  _nodes.resize(std::distance(_nodes.begin(), mid));
}

// Procedure: merge
inline void Graph::_merge(Graph&& g) {
  for(auto n : g._nodes) {
    _nodes.push_back(n);
  }
  g._nodes.clear();
}

// Function: erase
inline void Graph::_erase(Node* node) {
  if(auto I = std::find(_nodes.begin(), _nodes.end(), node); I != _nodes.end()) {
    _nodes.erase(I);
    node_pool.recycle(node);
  }
}

// Function: size
inline size_t Graph::size() const {
  return _nodes.size();
}

// Function: empty
inline bool Graph::empty() const {
  return _nodes.empty();
}

// private
template <typename ...ArgsT>
Node* Graph::_emplace_back(ArgsT&&... args) {
  _nodes.push_back(node_pool.animate(std::forward<ArgsT>(args)...));
  return _nodes.back();
}

}  // end of namespace tf. ---------------------------------------------------
