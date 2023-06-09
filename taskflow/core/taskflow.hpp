#pragma once

#include "flow_builder.hpp"

/**
@file taskflow/core/taskflow.hpp
@brief taskflow include file
*/

namespace tf {

// ----------------------------------------------------------------------------

/**
@class Taskflow

@brief class to create a taskflow object

%taskflow 管理任务依赖关系图，其中每个任务代表一个可调用对象（例如，@std_lambda、@std_function），一条边代表两个任务之间的依赖关系。 任务是以下类型之一：

  1. static task         : the callable constructible from
                           @c std::function<void()>
  2. dynamic task        : the callable constructible from
                           @c std::function<void(tf::Subflow&)>
  3. condition task      : the callable constructible from
                           @c std::function<int()>
  4. multi-condition task: the callable constructible from
                           @c %std::function<tf::SmallVector<int>()>
  5. module task         : the task constructed from tf::Taskflow::composed_of
                           @c std::function<void(tf::Runtime&)>

每个任务都是一个基本的计算单元，由一个来自 executor 的工作线程运行。 
以下示例创建了一个包含四个静态任务的简单任务流图，A、 B、 C 和 D，其中 A 在 B 之前运行， C 和 D 在之后运行  B 和 C。

@code{.cpp}
tf::Executor executor;
tf::Taskflow taskflow("simple");

tf::Task A = taskflow.emplace([](){ std::cout << "TaskA\n"; });
tf::Task B = taskflow.emplace([](){ std::cout << "TaskB\n"; });
tf::Task C = taskflow.emplace([](){ std::cout << "TaskC\n"; });
tf::Task D = taskflow.emplace([](){ std::cout << "TaskD\n"; });

A.precede(B, C);  // A runs before B and C
D.succeed(B, C);  // D runs after  B and C

executor.run(taskflow).wait();
@endcode

taskflow object  对象本身不是线程安全的。 您不应该在 graph  运行时对其进行修改，例如添加新任务、添加新依赖项以及将任务流移动到另一个。 
为了最大限度地减少任务创建的开销，我们的运行时利用全局对象池以线程安全的方式回收任务。 
请参阅@ref Cookbook 以了解有关每种任务类型以及如何将任务流提交给执行者的更多信息。
*/
class Taskflow : public FlowBuilder {

  friend class Topology;
  friend class Executor;
  friend class FlowBuilder;

  struct Dumper {
    size_t id;
    std::stack<std::pair<const Node*, const Graph*>> stack;
    std::unordered_map<const Graph*, size_t> visited;
  };

  public:
 
    Taskflow(const std::string& name);
 
    Taskflow();

    Taskflow(Taskflow&& rhs);
 
    Taskflow& operator = (Taskflow&& rhs);

    /**
    @brief default destructor

当调用 destructor 时，所有 tasks 及其相关数据（例如，捕获的数据）都将被销毁。 您有责任确保此任务流的所有提交执行都已完成，然后再销毁它。 
例如，以下代码会导致未定义的行为，因为执行程序可能仍在运行任务流，而它在块后被销毁。

    @code{.cpp}  错误的
    {
      tf::Taskflow taskflow;
      executor.run(taskflow);
    }
    @endcode

要解决这个问题，我们必须在销毁任务流之前等待执行完成。

    @code{.cpp}  正确的
    {
      tf::Taskflow taskflow;
      executor.run(taskflow).wait();
    }
    @endcode
    */
    ~Taskflow() = default;
 

    void dump(std::ostream& ostream) const;

 
    std::string dump() const;
 
    size_t num_tasks() const;
 
    bool empty() const;
 
    void name(const std::string&);

     
    const std::string& name() const;
 
    void clear();

    /**
    访问者是一个可调用对象，它接受类型为 tf::Task 的参数并且不返回任何内容。 以下示例迭代任务流中的每个任务并打印其名称：
   @code{.cpp}
    taskflow.for_each_task([](tf::Task task){
      std::cout << task.name() << '\n';
    });
    @endcode
    */
    template <typename V>
    void for_each_task(V&& visitor) const;

    /**
    @brief returns a reference to the underlying graph object
    图对象（类型为 tf::Graph）是任务依赖图的最终存储，只能用作不透明的数据结构来与 executor 交互（例如，composition）。
    */
    Graph& graph();

  private:

    mutable std::mutex  _mutex;

    std::string         _name;

    Graph               _graph;

    std::queue<std::shared_ptr<Topology>> _topologies;

    std::optional<std::list<Taskflow>::iterator> _satellite;

    void _dump(std::ostream&, const Graph*) const;
    void _dump(std::ostream&, const Node*, Dumper&) const;
    void _dump(std::ostream&, const Graph*, Dumper&) const;
};

// Constructor
inline Taskflow::Taskflow(const std::string& name) :
  FlowBuilder {_graph},
  _name       {name} {
}

// Constructor
inline Taskflow::Taskflow() : FlowBuilder{_graph} {}

// Move constructor
inline Taskflow::Taskflow(Taskflow&& rhs) : FlowBuilder{_graph} {

  std::scoped_lock<std::mutex> lock(rhs._mutex);

  _name       = std::move(rhs._name);
  _graph      = std::move(rhs._graph);
  _topologies = std::move(rhs._topologies);
  _satellite  = rhs._satellite;

  rhs._satellite.reset();
}

// Move assignment
inline Taskflow& Taskflow::operator = (Taskflow&& rhs) {
  if(this != &rhs) {
    std::scoped_lock<std::mutex, std::mutex> lock(_mutex, rhs._mutex);
    _name       = std::move(rhs._name);
    _graph      = std::move(rhs._graph);
    _topologies = std::move(rhs._topologies);
    _satellite  = rhs._satellite;
    rhs._satellite.reset();
  }
  return *this;
}

// Procedure:
inline void Taskflow::clear() {
  _graph._clear();
}

// Function: num_tasks
inline size_t Taskflow::num_tasks() const {
  return _graph.size();
}

// Function: empty
inline bool Taskflow::empty() const {
  return _graph.empty();
}

// Function: name
inline void Taskflow::name(const std::string &name) {
  _name = name;
}

// Function: name
inline const std::string& Taskflow::name() const {
  return _name;
}

// Function: graph
inline Graph& Taskflow::graph() {
  return _graph;
}

// Function: for_each_task
template <typename V>
void Taskflow::for_each_task(V&& visitor) const {
  for(size_t i=0; i<_graph._nodes.size(); ++i) {
    visitor(Task(_graph._nodes[i]));
  }
}

// Procedure: dump
inline std::string Taskflow::dump() const {
  std::ostringstream oss;
  dump(oss);
  return oss.str();
}

// Function: dump
inline void Taskflow::dump(std::ostream& os) const {
  os << "digraph Taskflow {\n";
  _dump(os, &_graph);
  os << "}\n";
}

// Procedure: _dump
inline void Taskflow::_dump(std::ostream& os, const Graph* top) const {

  Dumper dumper;

  dumper.id = 0;
  dumper.stack.push({nullptr, top});
  dumper.visited[top] = dumper.id++;

  while(!dumper.stack.empty()) {

    auto [p, f] = dumper.stack.top();
    dumper.stack.pop();

    os << "subgraph cluster_p" << f << " {\nlabel=\"";

    // n-level module
    if(p) {
      os << 'm' << dumper.visited[f];
    }
    // top-level taskflow graph
    else {
      os << "Taskflow: ";
      if(_name.empty()) os << 'p' << this;
      else os << _name;
    }

    os << "\";\n";

    _dump(os, f, dumper);
    os << "}\n";
  }
}

// Procedure: _dump
inline void Taskflow::_dump( std::ostream& os, const Node* node, Dumper& dumper) const {

  os << 'p' << node << "[label=\"";
  if(node->_name.empty()) os << 'p' << node;
  else os << node->_name;
  os << "\" ";

  // shape for node
  switch(node->_handle.index()) {

    case Node::CONDITION:
    case Node::MULTI_CONDITION:
      os << "shape=diamond color=black fillcolor=aquamarine style=filled";
    break;

    default:
    break;
  }

  os << "];\n";

  for(size_t s=0; s<node->_successors.size(); ++s) {
    if(node->_is_conditioner()) {
      // case edge is dashed
      os << 'p' << node << " -> p" << node->_successors[s] << " [style=dashed label=\"" << s << "\"];\n";
    } else {
      os << 'p' << node << " -> p" << node->_successors[s] << ";\n";
    }
  }

  // subflow join node
  if(node->_parent && node->_parent->_handle.index() == Node::DYNAMIC && node->_successors.size() == 0  ) {
    os << 'p' << node << " -> p" << node->_parent << ";\n";
  }

  // node info
  switch(node->_handle.index()) {

    case Node::DYNAMIC: {
      auto& sbg = std::get_if<Node::Dynamic>(&node->_handle)->subgraph;
      if(!sbg.empty()) {
        os << "subgraph cluster_p" << node << " {\nlabel=\"Subflow: ";
        if(node->_name.empty()) os << 'p' << node;
        else os << node->_name;

        os << "\";\n" << "color=blue\n";
        _dump(os, &sbg, dumper);
        os << "}\n";
      }
    }
    break;

    default:
    break;
  }
}

// Procedure: _dump
inline void Taskflow::_dump( std::ostream& os, const Graph* graph, Dumper& dumper) const {

  for(const auto& n : graph->_nodes) {

    // regular task
    if(n->_handle.index() != Node::MODULE) {
      _dump(os, n, dumper);
    }
    // module task
    else {
      //auto module = &(std::get_if<Node::Module>(&n->_handle)->module);
      auto module = &(std::get_if<Node::Module>(&n->_handle)->graph);

      os << 'p' << n << "[shape=box3d, color=blue, label=\"";
      if(n->_name.empty()) os << 'p' << n;
      else os << n->_name;

      if(dumper.visited.find(module) == dumper.visited.end()) {
        dumper.visited[module] = dumper.id++;
        dumper.stack.push({n, module});
      }

      os << " [m" << dumper.visited[module] << "]\"];\n";

      for(const auto s : n->_successors) {
        os << 'p' << n << "->" << 'p' << s << ";\n";
      }
    }
  }
}

// ----------------------------------------------------------------------------
// class definition: Future
// ----------------------------------------------------------------------------

/**
@class Future

@brief class to access the result of an execution

tf::Future 是 std::future 的派生类，最终会保存提交的任务流的执行结果（tf::Executor::run） 
除了继承自 std::future 的基类方法外，还可以调用 tf ::Future::cancel 取消与此未来对象关联的正在运行的任务流的执行。 
以下示例取消提交包含 1000 个任务的任务流，每个任务运行一秒。
 

@code{.cpp}
tf::Executor executor;
tf::Taskflow taskflow;

for(int i=0; i<1000; i++) {
  taskflow.emplace([](){ std::this_thread::sleep_for(std::chrono::seconds(1)); });
}
 
tf::Future fu = executor.run(taskflow);
fu.cancel();
fu.get();

@endcode
*/
template <typename T>
class Future : public std::future<T>  {

  friend class Executor;
  friend class Subflow;
  friend class Runtime;

  using handle_t = std::variant<std::monostate, std::weak_ptr<Topology>>;

  public:
 
    Future() = default;
 
    Future(const Future&) = delete;
 
    Future(Future&&) = default;

    Future& operator = (const Future&) = delete;

    Future& operator = (Future&&) = default;

    /**
    @brief 取消与此未来对象关联的正在运行的 taskflow 的执行    
    @return @c true if the execution can be cancelled or
            @c false if the execution has already completed

   当您请求取消时，executor  将停止安排任何任务。 已经在运行的任务将继续完成（非抢占式）。 您可以调用 tf::Future::wait 等待取消完成。
    */
    bool cancel();

  private:

    handle_t _handle;

    template <typename P>
    Future(std::future<T>&&, P&&);
};

template <typename T>
template <typename P>
Future<T>::Future(std::future<T>&& fu, P&& p) : std::future<T> {std::move(fu)},   _handle   {std::forward<P>(p)} {}

// Function: cancel
template <typename T>
bool Future<T>::cancel() {
  return std::visit([](auto&& arg){
    using P = std::decay_t<decltype(arg)>;
    if constexpr(std::is_same_v<P, std::monostate>) {
      return false;
    }
    else {
      auto ptr = arg.lock();
      if(ptr) {
        ptr->_is_cancelled.store(true, std::memory_order_relaxed);
        return true;
      }
      return false;
    }
  }, _handle);
}


}  // end of namespace tf. ---------------------------------------------------
