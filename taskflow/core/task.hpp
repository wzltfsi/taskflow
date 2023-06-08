#pragma once

#include "graph.hpp"

/**
@file task.hpp
@brief task include file
*/

namespace tf {

// ----------------------------------------------------------------------------
// Task Types
// ----------------------------------------------------------------------------

/**
@enum TaskType

@brief enumeration of all task types
*/
enum class TaskType : int {
  /** @brief placeholder task type */
  PLACEHOLDER = 0,
  /** @brief static task type */
  STATIC,
  /** @brief dynamic (subflow) task type */
  DYNAMIC,
  /** @brief condition task type */
  CONDITION,
  /** @brief module task type */
  MODULE,
  /** @brief asynchronous task type */
  ASYNC,
  /** @brief undefined task type (for internal use only) */
  UNDEFINED
};

// 所有任务类型的数组（用于迭代任务类型）
inline constexpr std::array<TaskType, 6> TASK_TYPES = {
  TaskType::PLACEHOLDER,
  TaskType::STATIC,
  TaskType::DYNAMIC,
  TaskType::CONDITION,
  TaskType::MODULE,
  TaskType::ASYNC,
};

/**
@brief v

The name of each task type is the litte-case string of its characters.

@code{.cpp}
TaskType::PLACEHOLDER     ->  "placeholder"
TaskType::STATIC          ->  "static"
TaskType::DYNAMIC         ->  "subflow"
TaskType::CONDITION       ->  "condition"
TaskType::MODULE          ->  "module"
TaskType::ASYNC           ->  "async"
@endcode
*/
inline const char* to_string(TaskType type) {

  const char* val;

  switch(type) {
    case TaskType::PLACEHOLDER:      val = "placeholder";     break;
    case TaskType::STATIC:           val = "static";          break;
    case TaskType::DYNAMIC:          val = "subflow";         break;
    case TaskType::CONDITION:        val = "condition";       break;
    case TaskType::MODULE:           val = "module";          break;
    case TaskType::ASYNC:            val = "async";           break;
    default:                         val = "undefined";       break;
  }

  return val;
}

// ----------------------------------------------------------------------------
// Task Traits
// ----------------------------------------------------------------------------

/**
@brief 确定可调用对象是否是  dynamic task
动态任务是可从 std::function<void(Subflow&)> 构造的可调用对象。
*/
template <typename C>
constexpr bool is_dynamic_task_v =   std::is_invocable_r_v<void, C, Subflow&> && !std::is_invocable_r_v<void, C, Runtime&>;

/**
@brief 确定可调用对象是否是  condition task
条件任务是可从 std::function<int()> 或 std::function<int(tf::Runtime&)> 构造的可调用对象。
*/
template <typename C>
constexpr bool is_condition_task_v = 
  (std::is_invocable_r_v<int, C> || std::is_invocable_r_v<int, C, Runtime&>) &&  !is_dynamic_task_v<C>;

/**
@brief determines if a callable is a multi-condition task
multi-condition task  是可从 std::function<tf::SmallVector<int>()> 或 std::function<tf::SmallVector<int>(tf::Runtime&)> 构造的可调用对象。
*/
template <typename C>
constexpr bool is_multi_condition_task_v =
  (std::is_invocable_r_v<SmallVector<int>, C> ||  std::is_invocable_r_v<SmallVector<int>, C, Runtime&>) && !is_dynamic_task_v<C>;

/**
@brief determines if a callable is a static task
静态任务是可从 std::function<void()> 或 std::function<void(tf::Runtime&)> 构造的可调用对象。
*/
template <typename C>
constexpr bool is_static_task_v =
  (std::is_invocable_r_v<void, C> || std::is_invocable_r_v<void, C, Runtime&>) &&
  !is_condition_task_v<C> && !is_multi_condition_task_v<C> && !is_dynamic_task_v<C>;





// ----------------------------------------------------------------------------
// Task
// ----------------------------------------------------------------------------

/**
@class Task

@brief class to create a task handle over a node in a taskflow graph

task 是 taskflow graph 中节点的包装器。 它提供了一组方法供用户访问和修改任务流图中关联节点的属性。 
task  是非常轻量级的对象（即，只存储一个节点指针），可以被简单地复制，并且它不拥有关联节点的生命周期。
*/
class Task {

  friend class FlowBuilder;
  friend class Runtime;
  friend class Taskflow;
  friend class TaskView;
  friend class Executor;

  public:

    Task() = default;

    Task(const Task& other);

    Task& operator = (const Task&);

    Task& operator = (std::nullptr_t);

    bool operator == (const Task& rhs) const;

    bool operator != (const Task& rhs) const;

    // 查询 task 名称
    const std::string& name() const;

    // 查询任务的 successors 数 
    size_t num_successors() const;

    // 查询任务的 predecessors 数
    size_t num_dependents() const;

    // 查询任务的强依赖(strong dependents )数量
    size_t num_strong_dependents() const;

    // 查询任务的弱依赖(weak dependents）数量
    size_t num_weak_dependents() const;


    /**
    @brief assigns a name to the task
    @param name a @std_string acceptable string
    @return @c *this
    */
    Task& name(const std::string& name);


    /**
    @brief assigns a callable
    @tparam C callable type
    @param callable callable to construct a task
    @return @c *this
    */
    template <typename C>
    Task& work(C&& callable);


    /**
    @brief creates a module task from a taskflow
    @tparam T object type
    @param object a custom object that defines @c T::graph() method
    @return @c *this
    */
    template <typename T>
    Task& composed_of(T& object);


    /**
    @brief adds precedence links from this to other tasks
    @tparam Ts parameter pack
    @param tasks one or multiple tasks
    @return @c *this
    */
    template <typename... Ts>
    Task& precede(Ts&&... tasks);


    /**
    @brief adds precedence links from other tasks to this
    @tparam Ts parameter pack
    @param tasks one or multiple tasks
    @return @c *this
    */
    template <typename... Ts>
    Task& succeed(Ts&&... tasks);


    // 使 task release 此信号量
    Task& release(Semaphore& semaphore);

    // 让task 获得这个信号量
    Task& acquire(Semaphore& semaphore);

    /**
    @brief assigns pointer to user data

    @param data pointer to user data

    以下示例显示如何将用户数据附加到任务并在更改数据值时迭代运行任务：

    @code{.cpp}
    tf::Executor executor;
    tf::Taskflow taskflow("attach data to a task");

    int data;

    // create a task and attach it the data
    auto A = taskflow.placeholder();
    A.data(&data).work([A](){
      auto d = *static_cast<int*>(A.data());
      std::cout << "data is " << d << std::endl;
    });

    // 使用不断变化的数据迭代运行任务流
    for(data = 0; data<10; data++){
      executor.run(taskflow).wait();
    }
    @endcode

    @return @c *this
    */
    Task& data(void* data);
      
    /**
    @brief assigns a priority value to the task

   优先级值可以是以下三个级别之一，tf::TaskPriority::HIGH（数值上等于 0）、tf::TaskPriority::NORMAL（数值上等于 1）
   和 tf::TaskPriority::LOW（数值上等于 相当于2）。 优先级值越小，优先级越高。
    */
    Task& priority(TaskPriority p);
    

    // 查询 task 的优先级值
    TaskPriority priority() const;

    // 将task 句柄重置为空
    void reset();

    // 将关联的 work 重置为 placeholder 
    void reset_work();

    // 查询 task 句柄是否指向任务节点
    bool empty() const;

    // 查询 task 是否分配了work
    bool has_work() const;

    // 将可调用的访问者应用于 task 的每个 successor 
    template <typename V>
    void for_each_successor(V&& visitor) const;

    // 将可调用的访问者应用于 task 的每个 dependents 
    template <typename V>
    void for_each_dependent(V&& visitor) const;

    // 获取底层节点的哈希值
    size_t hash_value() const;

    // 返回task 类型  =
    TaskType type() const;

    // 通过输出流转储任务
    void dump(std::ostream& ostream) const;

    // 查询指向用户数据的指针
    void* data() const;


  private:

    Task(Node*);

    Node* _node {nullptr};
};

// Constructor
inline Task::Task(Node* node) : _node {node} {
}

// Constructor
inline Task::Task(const Task& rhs) : _node {rhs._node} {
}

// Function: precede
template <typename... Ts>
Task& Task::precede(Ts&&... tasks) {
  (_node->_precede(tasks._node), ...);
  return *this;
}

// Function: succeed
template <typename... Ts>
Task& Task::succeed(Ts&&... tasks) {
  (tasks._node->_precede(_node), ...);
  return *this;
}

// Function: composed_of
template <typename T>
Task& Task::composed_of(T& object) {
  _node->_handle.emplace<Node::Module>(object);
  return *this;
}

// Operator =
inline Task& Task::operator = (const Task& rhs) {
  _node = rhs._node;
  return *this;
}

// Operator =
inline Task& Task::operator = (std::nullptr_t ptr) {
  _node = ptr;
  return *this;
}

// Operator ==
inline bool Task::operator == (const Task& rhs) const {
  return _node == rhs._node;
}

// Operator !=
inline bool Task::operator != (const Task& rhs) const {
  return _node != rhs._node;
}

// Function: name
inline Task& Task::name(const std::string& name) {
  _node->_name = name;
  return *this;
}

// Function: acquire
inline Task& Task::acquire(Semaphore& s) {
  if(!_node->_semaphores) {
    _node->_semaphores = std::make_unique<Node::Semaphores>();
  }
  _node->_semaphores->to_acquire.push_back(&s);
  return *this;
}

// Function: release
inline Task& Task::release(Semaphore& s) {
  if(!_node->_semaphores) {
    _node->_semaphores = std::make_unique<Node::Semaphores>();
  }
  _node->_semaphores->to_release.push_back(&s);
  return *this;
}

// Procedure: reset
inline void Task::reset() {
  _node = nullptr;
}

// Procedure: reset_work
inline void Task::reset_work() {
  _node->_handle.emplace<std::monostate>();
}

// Function: name
inline const std::string& Task::name() const {
  return _node->_name;
}

// Function: num_dependents
inline size_t Task::num_dependents() const {
  return _node->num_dependents();
}

// Function: num_strong_dependents
inline size_t Task::num_strong_dependents() const {
  return _node->num_strong_dependents();
}

// Function: num_weak_dependents
inline size_t Task::num_weak_dependents() const {
  return _node->num_weak_dependents();
}

// Function: num_successors
inline size_t Task::num_successors() const {
  return _node->num_successors();
}

// Function: empty
inline bool Task::empty() const {
  return _node == nullptr;
}

// Function: has_work
inline bool Task::has_work() const {
  return _node ? _node->_handle.index() != 0 : false;
}

// Function: task_type
inline TaskType Task::type() const {
  switch(_node->_handle.index()) {
    case Node::PLACEHOLDER:     return TaskType::PLACEHOLDER;
    case Node::STATIC:          return TaskType::STATIC;
    case Node::DYNAMIC:         return TaskType::DYNAMIC;
    case Node::CONDITION:       return TaskType::CONDITION;
    case Node::MULTI_CONDITION: return TaskType::CONDITION;
    case Node::MODULE:          return TaskType::MODULE;
    case Node::ASYNC:           return TaskType::ASYNC;
    case Node::DEPENDENT_ASYNC: return TaskType::ASYNC;
    default:                    return TaskType::UNDEFINED;
  }
}

// Function: for_each_successor
template <typename V>
void Task::for_each_successor(V&& visitor) const {
  for(size_t i=0; i<_node->_successors.size(); ++i) {
    visitor(Task(_node->_successors[i]));
  }
}

// Function: for_each_dependent
template <typename V>
void Task::for_each_dependent(V&& visitor) const {
  for(size_t i=0; i<_node->_dependents.size(); ++i) {
    visitor(Task(_node->_dependents[i]));
  }
}

// Function: hash_value
inline size_t Task::hash_value() const {
  return std::hash<Node*>{}(_node);
}

// Procedure: dump
inline void Task::dump(std::ostream& os) const {
  os << "task ";
  if(name().empty()) os << _node;
  else os << name();
  os << " [type=" << to_string(type()) << ']';
}

// Function: work
template <typename C>
Task& Task::work(C&& c) {

  if constexpr(is_static_task_v<C>) {
    _node->_handle.emplace<Node::Static>(std::forward<C>(c));
  }
  else if constexpr(is_dynamic_task_v<C>) {
    _node->_handle.emplace<Node::Dynamic>(std::forward<C>(c));
  }
  else if constexpr(is_condition_task_v<C>) {
    _node->_handle.emplace<Node::Condition>(std::forward<C>(c));
  }
  else if constexpr(is_multi_condition_task_v<C>) {
    _node->_handle.emplace<Node::MultiCondition>(std::forward<C>(c));
  }
  else {
    static_assert(dependent_false_v<C>, "invalid task callable");
  }
  return *this;
}

// Function: data
inline void* Task::data() const {
  return _node->_data;
}

// Function: data
inline Task& Task::data(void* data) {
  _node->_data = data;
  return *this;
}

// Function: priority
inline Task& Task::priority(TaskPriority p) {
  _node->_priority = static_cast<unsigned>(p);
  return *this;
}

// Function: priority
inline TaskPriority Task::priority() const {
  return static_cast<TaskPriority>(_node->_priority);
}

// ----------------------------------------------------------------------------
// global ostream
// ----------------------------------------------------------------------------

// 任务的 ostream 插入器运算符的重载
inline std::ostream& operator << (std::ostream& os, const Task& task) {
  task.dump(os);
  return os;
}

// ----------------------------------------------------------------------------
// Task View
// ----------------------------------------------------------------------------

/**
@class TaskView
从observer interface 访问  task information  的类
*/
class TaskView {

  friend class Executor;

  public:

    // 查询任务名称
    const std::string& name() const;

    // 查询任务的successors 数
    size_t num_successors() const;

    // 查询任务的predecessors数
    size_t num_dependents() const;

    // 查询任务的强依赖(strong dependents )数量
    size_t num_strong_dependents() const;

    // 查询任务的弱依赖 (  weak dependents )数量 
    size_t num_weak_dependents() const;

    // 将可调用的访问者应用于任务的每个 successor 
    template <typename V>
    void for_each_successor(V&& visitor) const;

    // 将可调用的访问者应用于任务的每个 dependents
    template <typename V>
    void for_each_dependent(V&& visitor) const;

    // 查询任务类型
    TaskType type() const;
    
    // 获取底层节点的哈希值
    size_t hash_value() const;

  private:

    TaskView(const Node&);
    TaskView(const TaskView&) = default;

    const Node& _node;
};

// Constructor
inline TaskView::TaskView(const Node& node) : _node {node} {
}

// Function: name
inline const std::string& TaskView::name() const {
  return _node._name;
}

// Function: num_dependents
inline size_t TaskView::num_dependents() const {
  return _node.num_dependents();
}

// Function: num_strong_dependents
inline size_t TaskView::num_strong_dependents() const {
  return _node.num_strong_dependents();
}

// Function: num_weak_dependents
inline size_t TaskView::num_weak_dependents() const {
  return _node.num_weak_dependents();
}

// Function: num_successors
inline size_t TaskView::num_successors() const {
  return _node.num_successors();
}

// Function: type
inline TaskType TaskView::type() const {
  switch(_node._handle.index()) {
    case Node::PLACEHOLDER:     return TaskType::PLACEHOLDER;
    case Node::STATIC:          return TaskType::STATIC;
    case Node::DYNAMIC:         return TaskType::DYNAMIC;
    case Node::CONDITION:       return TaskType::CONDITION;
    case Node::MULTI_CONDITION: return TaskType::CONDITION;
    case Node::MODULE:          return TaskType::MODULE;
    case Node::ASYNC:           return TaskType::ASYNC;
    case Node::DEPENDENT_ASYNC: return TaskType::ASYNC;
    default:                    return TaskType::UNDEFINED;
  }
}

// Function: hash_value
inline size_t TaskView::hash_value() const {
  return std::hash<const Node*>{}(&_node);
}

// Function: for_each_successor
template <typename V>
void TaskView::for_each_successor(V&& visitor) const {
  for(size_t i=0; i<_node._successors.size(); ++i) {
    visitor(TaskView(*_node._successors[i]));
  }
}

// Function: for_each_dependent
template <typename V>
void TaskView::for_each_dependent(V&& visitor) const {
  for(size_t i=0; i<_node._dependents.size(); ++i) {
    visitor(TaskView(*_node._dependents[i]));
  }
}

}  // end of namespace tf. ---------------------------------------------------

namespace std {

/**
@struct hash
std::hash<tf::Task> 的散列特化
*/
template <>
struct hash<tf::Task> {
  auto operator() (const tf::Task& task) const noexcept {
    return task.hash_value();
  }
};

/**
@struct hash
std::hash<tf::TaskView> 的散列特化
*/
template <>
struct hash<tf::TaskView> {
  auto operator() (const tf::TaskView& task_view) const noexcept {
    return task_view.hash_value();
  }
};

}  // end of namespace std ----------------------------------------------------



