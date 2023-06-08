#pragma once

#include "task.hpp"
#include "../algorithm/partitioner.hpp"

/**
@file flow_builder.hpp
@brief flow builder include file
*/

namespace tf {

/**
@class FlowBuilder

@brief class to build a task dependency graph

该类提供了 构建任务依赖关系图的基本方法，tf::Taskflow 和 tf::Subflow 从中派生。
*/
class FlowBuilder {

  friend class Executor;

  public:

  // 用 graph 构造 flow builder
  FlowBuilder(Graph& graph);

  /**
  @brief creates a static task

  @tparam C callable type constructible from std::function<void()>
  @param callable callable to construct a static task

  @return a tf::Task handle
  以下示例创建一个静态任务。 详情请参考@ref StaticTasking。
  
  @code{.cpp}
  tf::Task static_task = taskflow.emplace([](){});
  @endcode
  */
  template <typename C,  std::enable_if_t<is_static_task_v<C>, void>* = nullptr>
  Task emplace(C&& callable);

  /**
  @brief creates a dynamic task

  @tparam C callable type constructible from std::function<void(tf::Subflow&)>
  @param callable callable to construct a dynamic task

  @return a tf::Task handle

  以下示例创建一个生成两个 static tasks 的 dynamic task (tf::Subflow) . 详情请参考@ref DynamicTasking。

  @code{.cpp}
  tf::Task dynamic_task = taskflow.emplace([](tf::Subflow& sf){
    tf::Task static_task1 = sf.emplace([](){});
    tf::Task static_task2 = sf.emplace([](){});
  });
  @endcode
  */
  template <typename C,  std::enable_if_t<is_dynamic_task_v<C>, void>* = nullptr >
  Task emplace(C&& callable);



  /**
  @brief creates a condition task

  @tparam C callable type constructible from std::function<int()>
  @param callable callable to construct a condition task

  @return a tf::Task handle

  以下示例使用一个 condition task 和 三个 static tasks 创建一个 if-else 块。 详情请参考@ref ConditionalTasking。

  @code{.cpp}
  tf::Taskflow taskflow;

  auto [init, cond, yes, no] = taskflow.emplace(
    [] () { },
    [] () { return 0; },
    [] () { std::cout << "yes\n"; },
    [] () { std::cout << "no\n"; }
  );

  // 如果 cond 返回 0 则执行 yes，如果 cond 返回 1 则执行 no
  cond.precede(yes, no);
  cond.succeed(init);
  @endcode 
  */
  template <typename C,  std::enable_if_t<is_condition_task_v<C>, void>* = nullptr >
  Task emplace(C&& callable);



  /**
  @brief creates a multi-condition task

  @tparam C callable type constructible from
          std::function<tf::SmallVector<int>()>

  @param callable callable to construct a multi-condition task

  @return a tf::Task handle

以下示例创建一个多条件任务，该任务有选择地跳转到两个 successor tasks.  详情请参考@ref ConditionalTasking。

  @code{.cpp}
  tf::Taskflow taskflow;

  auto [init, cond, branch1, branch2, branch3] = taskflow.emplace(
    [] () { },
    [] () { return tf::SmallVector{0, 2}; },
    [] () { std::cout << "branch1\n"; },
    [] () { std::cout << "branch2\n"; },
    [] () { std::cout << "branch3\n"; }
  );

  // executes branch1 and branch3 when cond returns 0 and 2
  cond.precede(branch1, branch2, branch3);
  cond.succeed(init);
  @endcode 
  */
  template <typename C,  std::enable_if_t<is_multi_condition_task_v<C>, void>* = nullptr >
  Task emplace(C&& callable);



  /**
  @brief creates multiple tasks from a list of callable objects

  @tparam C callable types
  @param callables one or multiple callable objects constructible from each task category

  @return a tf::Task handle

该方法返回一个任务元组，每个任务对应于给定的可调用目标。 您可以使用结构化绑定来逐个获取返回任务。
 以下示例创建四个静态任务并使用结构化绑定将它们分配给  A、  B、  C 和  D。

  @code{.cpp}
  auto [A, B, C, D] = taskflow.emplace(
    [] () { std::cout << "A"; },
    [] () { std::cout << "B"; },
    [] () { std::cout << "C"; },
    [] () { std::cout << "D"; }
  );
  @endcode
  */
  template <typename... C, std::enable_if_t<(sizeof...(C)>1), void>* = nullptr>
  auto emplace(C&&... callables);




  /**
  @brief removes a task from a taskflow

  @param task task to remove

从与 flow builder关联的g raph 中 删除 task 及其输入和输出依赖项。 如果 task 不属于 graph ，则什么也不会发生。

  @code{.cpp}
  tf::Task A = taskflow.emplace([](){ std::cout << "A"; });
  tf::Task B = taskflow.emplace([](){ std::cout << "B"; });
  tf::Task C = taskflow.emplace([](){ std::cout << "C"; });
  tf::Task D = taskflow.emplace([](){ std::cout << "D"; });
  A.precede(B, C, D);

  // 从任务流中删除 A 及其对 B、C 和 D 的依赖关系
  taskflow.erase(A);
  @endcode
  */
  void erase(Task task);



  /**
  @brief creates a module task for the target object

  @tparam T target object type
  @param object a custom object that defines the method @c T::graph()

  @return a tf::Task handle

下面的示例演示了使用 @c composed_of 方法的 taskflow 组合。

  @code{.cpp}
  tf::Taskflow t1, t2;
  t1.emplace([](){ std::cout << "t1"; });

  // t2 is partially composed of t1
  tf::Task comp = t2.composed_of(t1);
  tf::Task init = t2.emplace([](){ std::cout << "t2"; });
  init.precede(comp);
  @endcode

taskflow object  t2 由另一个 taskflow object   t1 组成，前面是另一个静态任务  init。 
当 taskflow  t2 被提交给 executor 时，  init 将首先运行，然后 comp 将它的定义传播到 taskflow t1 中。 
  The target   object being composed 必须定义方法  T::graph() 返回对类型 tf::Graph 的 graph object 的引用，以便它可以与执行程序交互。 例如：
  详情请参考@ref ComposableTasking。

  @code{.cpp}
  // custom struct
  struct MyObj {
    tf::Graph graph;
    MyObj() {
      tf::FlowBuilder builder(graph);
      tf::Task task = builder.emplace([](){
        std::cout << "a task\n";  // static task
      });
    }
    Graph& graph() { return graph; }
  };

  MyObj obj;
  tf::Task comp = taskflow.composed_of(obj);
  @endcode 
  */
  template <typename T>
  Task composed_of(T& object);



  /**
  @brief creates a placeholder task

  @return a tf::Task handle

 placeholder task 映射到 taskflow graph 中的 node ，但尚未分配任何可调用工作。  placeholder task 不同于不指向 graph 中任何 node的空任务句柄。
 
  @code{.cpp}
  // 创建一个没有分配可调用目标的 placeholder task       
  tf::Task placeholder = taskflow.placeholder();
  assert(placeholder.empty() == false && placeholder.has_work() == false);

  // create an empty task handle
  tf::Task task;
  assert(task.empty() == true);

  // assign the task handle to the placeholder task
  task = placeholder;
  assert(task.empty() == false && task.has_work() == false);
  @endcode
  */
  Task placeholder();




  /**
  @brief   将相邻的依赖链接添加到任务的线性列表中

  @param tasks a vector of tasks

此成员函数在 vector of tasks  上创建线性依赖关系  

  @code{.cpp}
  tf::Task A = taskflow.emplace([](){ std::cout << "A"; });
  tf::Task B = taskflow.emplace([](){ std::cout << "B"; });
  tf::Task C = taskflow.emplace([](){ std::cout << "C"; });
  tf::Task D = taskflow.emplace([](){ std::cout << "D"; });
  std::vector<tf::Task> tasks {A, B, C, D}
  taskflow.linearize(tasks);  // A->B->C->D
  @endcode
  */
  void linearize(std::vector<Task>& tasks);



  /**
  @brief将相邻的依赖链接添加到任务的线性列表中

  @param tasks an initializer list of tasks

此成员函数创建任务列表的线性依赖关系。

  @code{.cpp}
  tf::Task A = taskflow.emplace([](){ std::cout << "A"; });
  tf::Task B = taskflow.emplace([](){ std::cout << "B"; });
  tf::Task C = taskflow.emplace([](){ std::cout << "C"; });
  tf::Task D = taskflow.emplace([](){ std::cout << "D"; });
  taskflow.linearize({A, B, C, D});  // A->B->C->D
  @endcode
  */
  void linearize(std::initializer_list<Task> tasks);





  // ------------------------------------------------------------------------
  // parallel iterations
  // ------------------------------------------------------------------------

  /**
  @brief constructs an STL-styled parallel-for task

  @tparam B beginning iterator type
  @tparam E ending iterator type
  @tparam C callable type
  @tparam P partitioner type (default tf::GuidedPartitioner)

  @param first iterator to the beginning (inclusive)
  @param last iterator to the end (exclusive)
  @param callable callable object to apply to the dereferenced iterator
  @param part partitioning algorithm to schedule parallel iterations

  @return a tf::Task handle

该任务生成异步任务，将可调用对象应用于通过取消引用范围 [first, last)  中的每个迭代器获得的每个对象。 该方法等效于并行执行以下循环：

  @code{.cpp}
  for(auto itr=first; itr!=last; itr++) {
    callable(*itr);
  }
  @endcode
  
迭代器被模板化以使用 std::reference_wrapper 启用有状态范围。 可调用对象需要采用取消引用的迭代器类型的单个参数。
详情请参考@ref ParallelIterations。 
  */
  template <typename B, typename E, typename C, typename P = GuidedPartitioner>
  Task for_each(B first, E last, C callable, P&& part = P());
  


  /**
  @brief constructs an STL-styled index-based parallel-for task 

  @tparam B beginning index type (must be integral)
  @tparam E ending index type (must be integral)
  @tparam S step type (must be integral)
  @tparam C callable type
  @tparam P partitioner type (default tf::GuidedPartitioner)

  @param first index of the beginning (inclusive)
  @param last index of the end (exclusive)
  @param step step size
  @param callable callable object to apply to each valid index
  @param part partitioning algorithm to schedule parallel iterations

  @return a tf::Task handle

该任务生成异步任务，将可调用对象应用于  [first, last) 范围内的每个索引 。 该方法等效于并行执行以下循环：
  @code{.cpp}
  // case 1: step size is positive
  for(auto i=first; i<last; i+=step) {
    callable(i);
  }

  // case 2: step size is negative
  for(auto i=first, i>last; i+=step) {
    callable(i);
  }
  @endcode

   迭代器被模板化以使用 std::reference_wrapper 启用有状态范围。 可调用对象需要采用整数索引类型的单个参数。 详情请参考@ref ParallelIterations。
  */
  template <typename B, typename E, typename S, typename C, typename P = GuidedPartitioner>
  Task for_each_index(  B first, E last, S step, C callable, P&& part = P());

  // ------------------------------------------------------------------------
  // transform
  // ------------------------------------------------------------------------

  /**
  @brief constructs a parallel-transform task

  @tparam B beginning input iterator type
  @tparam E ending input iterator type
  @tparam O output iterator type
  @tparam C callable type
  @tparam P partitioner type (default tf::GuidedPartitioner)

  @param first1 iterator to the beginning of the first range
  @param last1 iterator to the end of the first range
  @param d_first iterator to the beginning of the output range
  @param c an unary callable to apply to dereferenced input elements
  @param part partitioning algorithm to schedule parallel iterations

  @return a tf::Task handle

该任务生成异步任务，将可调用对象应用于输入范围并将结果存储在另一个输出范围中。 该方法等效于并行执行以下循环：

  @code{.cpp}
  while (first1 != last1) {
    *d_first++ = c(*first1++);
  }
  @endcode

迭代器被模板化以使用 std::reference_wrapper 启用有状态范围。 可调用对象需要采用取消引用的迭代器类型的单个参数。 详情请参考@ref ParallelTransforms。
  */
  template <  typename B, typename E, typename O, typename C, typename P = GuidedPartitioner>
  Task transform(B first1, E last1, O d_first, C c, P&& part = P());
  


  /**
  @brief constructs a parallel-transform task

  @tparam B1 beginning input iterator type for the first input range
  @tparam E1 ending input iterator type for the first input range
  @tparam B2 beginning input iterator type for the first second range
  @tparam O output iterator type
  @tparam C callable type
  @tparam P partitioner type (default tf::GuidedPartitioner)

  @param first1 iterator to the beginning of the first input range
  @param last1 iterator to the end of the first input range
  @param first2 iterator to the beginning of the second input range
  @param d_first iterator to the beginning of the output range
  @param c a binary operator to apply to dereferenced input elements
  @param part partitioning algorithm to schedule parallel iterations

  @return a tf::Task handle

该任务生成异步任务，将可调用对象应用于两个输入范围并将结果存储在另一个输出范围中。 该方法等效于并行执行以下循环：

  @code{.cpp}
  while (first1 != last1) {
    *d_first++ = c(*first1++, *first2++);
  }
  @endcode

迭代器被模板化以使用 std::reference_wrapper 启用有状态范围。 可调用对象需要从两个输入范围中获取两个取消引用元素的参数。 详情请参考@ref ParallelTransforms。

  */
  template < typename B1, typename E1, typename B2, typename O, typename C, typename P=GuidedPartitioner, std::enable_if_t<!is_partitioner_v<std::decay_t<C>>, void>* = nullptr>
  Task transform(B1 first1, E1 last1, B2 first2, O d_first, C c, P&& part = P());
  




  // ------------------------------------------------------------------------
  // reduction
  // ------------------------------------------------------------------------

  /**
  @brief constructs an STL-styled parallel-reduce task

  @tparam B beginning iterator type
  @tparam E ending iterator type
  @tparam T result type
  @tparam O binary reducer type
  @tparam P partitioner type (default tf::GuidedPartitioner)

  @param first iterator to the beginning (inclusive)
  @param last iterator to the end (exclusive)
  @param init initial value of the reduction and the storage for the reduced result
  @param bop binary operator that will be applied
  @param part partitioning algorithm to schedule parallel iterations

  @return a tf::Task handle

该任务生成异步任务以对 init 和[first, last) 范围内的元素执行并行缩减。 减少的结果存储在  init 中。 该方法等效于并行执行以下循环：

  @code{.cpp}
  for(auto itr = first; itr != last; itr++) {
    init = bop(init, *itr);
  }
  @endcode

  迭代器被模板化以使用 std::reference_wrapper 启用有状态范围。 详情请参考@ref ParallelReduction。
  */
  template <typename B, typename E, typename T, typename O, typename P = GuidedPartitioner>
  Task reduce(B first, E last, T& init, O bop, P&& part = P());
  



  // ------------------------------------------------------------------------
  // transfrom and reduction
  // ------------------------------------------------------------------------

  /**
  @brief constructs an STL-styled parallel transform-reduce task

  @tparam B beginning iterator type
  @tparam E ending iterator type
  @tparam T result type
  @tparam BOP binary reducer type
  @tparam UOP unary transformion type
  @tparam P partitioner type (default tf::GuidedPartitioner)

  @param first iterator to the beginning (inclusive)
  @param last iterator to the end (exclusive)
  @param init initial value of the reduction and the storage for the reduced result
  @param bop binary operator that will be applied in unspecified order to the results of @c uop
  @param uop unary operator that will be applied to transform each element in the range to the result type
  @param part partitioning algorithm to schedule parallel iterations

  @return a tf::Task handle

该任务生成异步任务以对 init 和[first, last) 范围内的已转换元素执行并行缩减。 减少的结果存储在 init 中。 该方法等效于并行执行以下循环：

  @code{.cpp}
  for(auto itr=first; itr!=last; itr++) {
    init = bop(init, uop(*itr));
  }
  @endcode

迭代器被模板化以使用 std::reference_wrapper 启用有状态范围。 详情请参考@ref ParallelReduction。
  */
  template <typename B, typename E, typename T, typename BOP, typename UOP, typename P = GuidedPartitioner  >
  Task transform_reduce(B first, E last, T& init, BOP bop, UOP uop, P&& part = P());

  /**
  @brief constructs an STL-styled parallel transform-reduce task
  @tparam B1 first beginning iterator type
  @tparam E1 first ending iterator type
  @tparam B2 second beginning iterator type
  @tparam T result type
  @tparam BOP_R binary reducer type
  @tparam BOP_T binary transformion type
  @tparam P partitioner type (default tf::GuidedPartitioner)
 
  @param first iterator to the beginning (inclusive)
  @param last iterator to the end (exclusive)
  @param init initial value of the reduction and the storage for the reduced result
  @param bop_r binary operator that will be applied in unspecified order to the results of @c bop_t
  @param bop_t binary operator that will be applied to transform each element in the range to the result type
  @param part partitioning algorithm to schedule parallel iterations
 
  @return a tf::Task handle
 
  该任务生成异步任务以对 init 和[first, last)范围内的已转换元素执行并行缩减。 减少的结果存储在 init 中。 该方法等效于并行执行以下循环：
 
  @code{.cpp}
  for(auto itr1=first1, itr2=first2; itr1!=last1; itr1++, itr2++) {
    init = bop_r(init, bop_t(*itr1, *itr2));
  }
  @endcode
 
 迭代器被模板化以使用 std::reference_wrapper 启用有状态范围。 详情请参考@ref ParallelReduction。
  */
  
  template <  typename B1, typename E1, typename B2, typename T, typename BOP_R, typename BOP_T, typename P = GuidedPartitioner>
  Task transform_reduce(B1 first1, E1 last1, B2 first2, T& init, BOP_R bop_r, BOP_T bop_t, P&& part = P());




  // ------------------------------------------------------------------------
  // scan
  // ------------------------------------------------------------------------
  
  /**
  @brief creates an STL-styled parallel inclusive-scan task

  @tparam B beginning iterator type
  @tparam E ending iterator type
  @tparam D destination iterator type
  @tparam BOP summation operator type

  @param first start of input range
  @param last end of input range
  @param d_first start of output range (may be the same as input range)
  @param bop function to perform summation

 执行输入范围的累积和（又名前缀和，又名扫描）并将结果写入输出范围。 输出范围的每个元素都包含使用给定二元运算符进行求和的所有早期元素的运行总和。
  此函数生成一个 包含扫描，意味着输出范围的第 N 个元素是前 N 个输入元素的总和，因此包含第 N 个输入元素。
迭代器被模板化以使用 std::reference_wrapper 启用有状态范围。 详情请参考@ref ParallelScan。

  @code{.cpp}
  std::vector<int> input = {1, 2, 3, 4, 5};
  taskflow.inclusive_scan( input.begin(), input.end(), input.begin(), std::plus<int>{});
  executor.run(taskflow).wait();
  @endcode
  */
  template <typename B, typename E, typename D, typename BOP>
  Task inclusive_scan(B first, E last, D d_first, BOP bop);
  


  /**
  @brief creates an STL-styled parallel inclusive-scan task with an initial value

  @tparam B beginning iterator type
  @tparam E ending iterator type
  @tparam D destination iterator type
  @tparam BOP summation operator type
  @tparam T initial value type

  @param first start of input range
  @param last end of input range
  @param d_first start of output range (may be the same as input range)
  @param bop function to perform summation
  @param init initial value

执行输入范围的累积和（又名前缀和，又名扫描）并将结果写入输出范围。 
输出范围的每个元素都包含使用给定二元运算符进行求和的所有早期元素（和初始值）的总和。 
此函数生成一个 包含扫描，意味着输出范围的第 N 个元素是前 N 个输入元素的总和，因此包含第 N 个输入元素。
迭代器被模板化以使用 std::reference_wrapper 启用有状态范围。 详情请参考@ref ParallelScan。
  @code{.cpp}
  std::vector<int> input = {1, 2, 3, 4, 5};
  taskflow.inclusive_scan( input.begin(), input.end(), input.begin(), std::plus<int>{}, -1);
  executor.run(taskflow).wait();

  @endcode
  */
  template <typename B, typename E, typename D, typename BOP, typename T>
  Task inclusive_scan(B first, E last, D d_first, BOP bop, T init);
  

  
  /**
  @brief creates an STL-styled parallel exclusive-scan task

  @tparam B beginning iterator type
  @tparam E ending iterator type
  @tparam D destination iterator type
  @tparam T initial value type
  @tparam BOP summation operator type

  @param first start of input range
  @param last end of input range
  @param d_first start of output range (may be the same as input range)
  @param init initial value
  @param bop function to perform summation

执行输入范围的累积和（又名前缀和，又名扫描）并将结果写入输出范围。
输出范围的每个元素都包含使用给定二元运算符进行求和的所有早期元素（和初始值）的总计。 
此函数生成 排他扫描，意味着输出范围的第 N 个元素是前 N-1 个输入元素的总和，因此不包括第 N 个输入元素。
迭代器被模板化以使用 std::reference_wrapper 启用有状态范围。 详情请参考@ref ParallelScan。
  @code{.cpp}
  std::vector<int> input = {1, 2, 3, 4, 5};
  taskflow.exclusive_scan(input.begin(), input.end(), input.begin(), -1, std::plus<int>{} );
  executor.run(taskflow).wait();

  @endcode
  */
  template <typename B, typename E, typename D, typename T, typename BOP>
  Task exclusive_scan(B first, E last, D d_first, T init, BOP bop);
  


  // ------------------------------------------------------------------------
  // transform scan
  // ------------------------------------------------------------------------
  
  /**
  @brief creates an STL-styled parallel transform-inclusive scan task

  @tparam B beginning iterator type
  @tparam E ending iterator type
  @tparam D destination iterator type
  @tparam BOP summation operator type
  @tparam UOP transform operator type

  @param first start of input range
  @param last end of input range
  @param d_first start of output range (may be the same as input range)
  @param bop function to perform summation
  @param uop function to transform elements of the input range

将输入范围的累积和（又名前缀和，又名扫描）写入输出范围。 
输出范围的每个元素都包含使用 uop 转换输入元素并使用 bop 求和的所有早期元素的运行总和。 
此函数生成一个@em 包含扫描，这意味着输出范围的第 N 个元素是前 N 个输入元素的总和，因此包含第 N 个输入元素。
  迭代器被模板化以使用 std::reference_wrapper 启用有状态范围。 详情请参考@ref ParallelScan。
  @code{.cpp}
  std::vector<int> input = {1, 2, 3, 4, 5};
  taskflow.transform_inclusive_scan(   input.begin(), input.end(), input.begin(), std::plus<int>{},  [] (int item) { return -item; });
  executor.run(taskflow).wait();
  @endcode
  */
  template <typename B, typename E, typename D, typename BOP, typename UOP>
  Task transform_inclusive_scan(B first, E last, D d_first, BOP bop, UOP uop);
  


  /**
  @brief creates an STL-styled parallel transform-inclusive scan task

  @tparam B beginning iterator type
  @tparam E ending iterator type
  @tparam D destination iterator type
  @tparam BOP summation operator type
  @tparam UOP transform operator type
  @tparam T initial value type

  @param first start of input range
  @param last end of input range
  @param d_first start of output range (may be the same as input range)
  @param bop function to perform summation
  @param uop function to transform elements of the input range
  @param init initial value

将输入范围的累积和（又名前缀和，又名扫描）写入输出范围。 
输出范围的每个元素包含所有早期元素（包括初始值）的运行总和，使用 uop 转换输入元素并使用 bop 求和。 
此函数生成一个 包含扫描，这意味着输出范围的第 N 个元素是前 N 个输入元素的总和，因此包含第 N 个输入元素。

  @code{.cpp}
  std::vector<int> input = {1, 2, 3, 4, 5};
  taskflow.transform_inclusive_scan( input.begin(), input.end(), input.begin(), std::plus<int>{},  [] (int item) { return -item; },  -1
  );
  executor.run(taskflow).wait();
  
  // input is {-2, -4, -7, -11, -16}
  @endcode

  迭代器被模板化以使用 std::reference_wrapper 启用有状态范围。 详情请参考@ref ParallelScan。
  */
  template <typename B, typename E, typename D, typename BOP, typename UOP, typename T>
  Task transform_inclusive_scan(B first, E last, D d_first, BOP bop, UOP uop, T init);
  

  /**
  @brief creates an STL-styled parallel transform-exclusive scan task

  @tparam B beginning iterator type
  @tparam E ending iterator type
  @tparam D destination iterator type
  @tparam BOP summation operator type
  @tparam UOP transform operator type
  @tparam T initial value type

  @param first start of input range
  @param last end of input range
  @param d_first start of output range (may be the same as input range)
  @param bop function to perform summation
  @param uop function to transform elements of the input range
  @param init initial value

将输入范围的累积和（又名前缀和，又名扫描）写入输出范围。 
输出范围的每个元素包含所有早期元素（包括初始值）的运行总和，使用 uop 转换输入元素并使用 bop 求和。 
此函数生成@ 排他扫描，意味着输出范围的第 N 个元素是前 N-1 个输入元素的和，因此不包括第 N 个输入元素。

  迭代器被模板化以使用 std::reference_wrapper 启用有状态范围。详情请参考@ref ParallelScan。
  @code{.cpp}
  std::vector<int> input = {1, 2, 3, 4, 5};
  taskflow.transform_exclusive_scan(input.begin(), input.end(), input.begin(), -1, std::plus<int>{}, [](int item) { return -item; });
  executor.run(taskflow).wait();
  @endcode

  */
  template <typename B, typename E, typename D, typename T, typename BOP, typename UOP>
  Task transform_exclusive_scan(B first, E last, D d_first, T init, BOP bop, UOP uop);

  // ------------------------------------------------------------------------
  // find
  // ------------------------------------------------------------------------
 
  /**
  @brief constructs a task to perform STL-styled find-if algorithm

  @tparam B beginning iterator type
  @tparam E ending iterator type
  @tparam T resulting iterator type
  @tparam UOP unary predicate type
  @tparam P partitioner type
  
  @param first start of the input range
  @param last end of the input range
  @param result resulting iterator to the found element in the input range
  @param predicate unary predicate which returns @c true for the required element
  @param part partitioning algorithm (default tf::GuidedPartitioner)

返回满足给定条件的 <[first, last)< 范围内第一个元素的迭代器（如果没有这样的迭代器，则返回 last ）。 该方法等效于并行执行以下循环：

  @code{.cpp}
  auto find_if(InputIt first, InputIt last, UnaryPredicate p) {
    for (; first != last; ++first) {
      if (predicate(*first)){
        return first;
      }
    }
    return last;
  }
  @endcode

例如，下面的代码从 10 个元素的输入范围中找到满足给定条件（值加一等于 23）的元素：

  @code{.cpp}
  std::vector<int> input = {1, 6, 9, 10, 22, 5, 7, 8, 9, 11};
  std::vector<int>::iterator result;
  taskflow.find_if( input.begin(), input.end(), [](int i){ return i+1 = 23; }, result);
  executor.run(taskflow).wait();
  assert(*result == 22);
  @endcode
迭代器被模板化以使用 std::reference_wrapper 启用有状态范围。
  */
  template <typename B, typename E, typename T, typename UOP, typename P = GuidedPartitioner>
  Task find_if(B first, E last, T& result, UOP predicate, P&& part = P());
  



  /**
  @brief constructs a task to perform STL-styled find-if-not algorithm

  @tparam B beginning iterator type
  @tparam E ending iterator type
  @tparam T resulting iterator type
  @tparam UOP unary predicate type
  @tparam P partitioner type
  
  @param first start of the input range
  @param last end of the input range
  @param result resulting iterator to the found element in the input range
  @param predicate unary predicate which returns @c false for the required element
  @param part partitioning algorithm (default tf::GuidedPartitioner)

返回满足给定条件的 [first, last) 范围内第一个元素的迭代器（如果没有这样的迭代器，则返回 last ）。 该方法等效于并行执行以下循环：
 
  @code{.cpp}
  auto find_if(InputIt first, InputIt last, UnaryPredicate p) {
    for (; first != last; ++first) {
      if (!predicate(*first)){
        return first;
      }
    }
    return last;
  }
  @endcode

例如，下面的代码从 10 个元素的输入范围中找到满足给定条件（值不等于 1）的元素：

  @code{.cpp}
  std::vector<int> input = {1, 1, 1, 1, 22, 1, 1, 1, 1, 1};
  std::vector<int>::iterator result;
  taskflow.find_if_not(  input.begin(), input.end(), [](int i){ return i == 1; }, result);
  executor.run(taskflow).wait();
  assert(*result == 22);
  @endcode

  迭代器被模板化以使用 std::reference_wrapper 启用有状态范围。
  */
  template <typename B, typename E, typename T, typename UOP,typename P = GuidedPartitioner>
  Task find_if_not(B first, E last, T& result, UOP predicate, P&& part = P());

  /**
  @brief constructs a task to perform STL-styled min-element algorithm

  @tparam B beginning iterator type
  @tparam E ending iterator type
  @tparam T resulting iterator type
  @tparam C comparator type
  @tparam P partitioner type
  
  @param first start of the input range
  @param last end of the input range
  @param result resulting iterator to the found element in the input range
  @param comp comparison function object
  @param part partitioning algorithm (default tf::GuidedPartitioner)

使用给定的比较函数对象查找 <[first, last)中的最小元素。 该最小元素的迭代器存储在 结果中。 该方法等效于并行执行以下循环：

  @code{.cpp}
  if (first == last) {
    return last;
  }
  auto smallest = first;
  ++first;
  for (; first != last; ++first) {
    if (comp(*first, *smallest)) {
      smallest = first;
    }
  }
  return smallest;
  @endcode

例如，下面的代码从 10 个元素的输入范围中查找最小的元素。

  @code{.cpp}
  std::vector<int> input = {1, 1, 1, 1, 1, -1, 1, 1, 1, 1};
  std::vector<int>::iterator result;
  taskflow.min_element( input.begin(), input.end(), std::less<int>(), result);
  executor.run(taskflow).wait();
  assert(*result == -1);
  @endcode

  迭代器被模板化以使用 std::reference_wrapper 启用有状态范围。
  */
  template <typename B, typename E, typename T, typename C, typename P>
  Task min_element(B first, E last, T& result, C comp, P&& part);
  

  /**
  @brief constructs a task to perform STL-styled max-element algorithm

  @tparam B beginning iterator type
  @tparam E ending iterator type
  @tparam T resulting iterator type
  @tparam C comparator type
  @tparam P partitioner type
  
  @param first start of the input range
  @param last end of the input range
  @param result resulting iterator to the found element in the input range
  @param comp comparison function object
  @param part partitioning algorithm (default tf::GuidedPartitioner)

使用给定的比较函数对象查找 [first, last) 中的最大元素。 该最大元素的迭代器存储在 结果中。 该方法等效于并行执行以下循环：

  @code{.cpp}
  if (first == last){
    return last;
  }
  auto largest = first;
  ++first;
  for (; first != last; ++first) {
    if (comp(*largest, *first)) {
      largest = first;
    }
  }
  return largest;
  @endcode

  例如，下面的代码从 10 个元素的输入范围中查找最大的元素。

  @code{.cpp}
  std::vector<int> input = {1, 1, 1, 1, 1, 2, 1, 1, 1, 1};
  std::vector<int>::iterator result;
  taskflow.max_element(  input.begin(), input.end(), std::less<int>(), result);
  executor.run(taskflow).wait();
  assert(*result == 2);
  @endcode

  迭代器被模板化以使用 std::reference_wrapper 启用有状态范围。
  */
  template <typename B, typename E, typename T, typename C, typename P>
  Task max_element(B first, E last, T& result, C comp, P&& part);

  // ------------------------------------------------------------------------
  // sort
  // ------------------------------------------------------------------------

  /**
  @brief constructs a dynamic task to perform STL-styled parallel sort

  @tparam B beginning iterator type (random-accessible)
  @tparam E ending iterator type (random-accessible)
  @tparam C comparator type

  @param first iterator to the beginning (inclusive)
  @param last iterator to the end (exclusive)
  @param cmp comparison operator

该任务生成异步任务以并行对 [first, last) 范围内的元素进行排序。 迭代器被模板化以使用 std::reference_wrapper 启用有状态范围。 详情请参考@ref ParallelSort。
  */
  template <typename B, typename E, typename C>
  Task sort(B first, E last, C cmp);

  /**
  @brief constructs a dynamic task to perform STL-styled parallel sort using
         the @c std::less<T> comparator, where @c T is the element type

  @tparam B beginning iterator type (random-accessible)
  @tparam E ending iterator type (random-accessible)

  @param first iterator to the beginning (inclusive)
  @param last iterator to the end (exclusive)

该任务生成异步任务以使用  std::less<T> 比较器对 [first, last) 范围内的元素进行并行排序，其中  T 是取消引用的迭代器类型。 
迭代器被模板化以使用 std::reference_wrapper 启用有状态范围。 详情请参考@ref ParallelSort。
   */
  template <typename B, typename E>
  Task sort(B first, E last);



  protected:

  // associated graph object
  Graph& _graph;

  private:

  template <typename L>
  void _linearize(L&);
};

// Constructor
inline FlowBuilder::FlowBuilder(Graph& graph) :
  _graph {graph} {
}

// Function: emplace
template <typename C, std::enable_if_t<is_static_task_v<C>, void>*>
Task FlowBuilder::emplace(C&& c) {
  return Task(_graph._emplace_back("", 0, nullptr, nullptr, 0,
    std::in_place_type_t<Node::Static>{}, std::forward<C>(c)
  ));
}

// Function: emplace
template <typename C, std::enable_if_t<is_dynamic_task_v<C>, void>*>
Task FlowBuilder::emplace(C&& c) {
  return Task(_graph._emplace_back("", 0, nullptr, nullptr, 0,
    std::in_place_type_t<Node::Dynamic>{}, std::forward<C>(c)
  ));
}

// Function: emplace
template <typename C, std::enable_if_t<is_condition_task_v<C>, void>*>
Task FlowBuilder::emplace(C&& c) {
  return Task(_graph._emplace_back("", 0, nullptr, nullptr, 0,
    std::in_place_type_t<Node::Condition>{}, std::forward<C>(c)
  ));
}

// Function: emplace
template <typename C, std::enable_if_t<is_multi_condition_task_v<C>, void>*>
Task FlowBuilder::emplace(C&& c) {
  return Task(_graph._emplace_back("", 0, nullptr, nullptr, 0,
    std::in_place_type_t<Node::MultiCondition>{}, std::forward<C>(c)
  ));
}

// Function: emplace
template <typename... C, std::enable_if_t<(sizeof...(C)>1), void>*>
auto FlowBuilder::emplace(C&&... cs) {
  return std::make_tuple(emplace(std::forward<C>(cs))...);
}

// Function: erase
inline void FlowBuilder::erase(Task task) {

  if (!task._node) {
    return;
  }

  task.for_each_dependent([&] (Task dependent) {
    auto& S = dependent._node->_successors;
    if(auto I = std::find(S.begin(), S.end(), task._node); I != S.end()) {
      S.erase(I);
    }
  });

  task.for_each_successor([&] (Task dependent) {
    auto& D = dependent._node->_dependents;
    if(auto I = std::find(D.begin(), D.end(), task._node); I != D.end()) {
      D.erase(I);
    }
  });

  _graph._erase(task._node);
}

// Function: composed_of
template <typename T>
Task FlowBuilder::composed_of(T& object) {
  auto node = _graph._emplace_back("", 0, nullptr, nullptr, 0,
    std::in_place_type_t<Node::Module>{}, object
  );
  return Task(node);
}

// Function: placeholder
inline Task FlowBuilder::placeholder() {
  auto node = _graph._emplace_back("", 0, nullptr, nullptr, 0,
    std::in_place_type_t<Node::Placeholder>{}
  );
  return Task(node);
}

// Procedure: _linearize
template <typename L>
void FlowBuilder::_linearize(L& keys) {

  auto itr = keys.begin();
  auto end = keys.end();

  if(itr == end) {
    return;
  }

  auto nxt = itr;

  for(++nxt; nxt != end; ++nxt, ++itr) {
    itr->_node->_precede(nxt->_node);
  }
}

// Procedure: linearize
inline void FlowBuilder::linearize(std::vector<Task>& keys) {
  _linearize(keys);
}

// Procedure: linearize
inline void FlowBuilder::linearize(std::initializer_list<Task> keys) {
  _linearize(keys);
}

// ----------------------------------------------------------------------------

/**
@class Subflow

@brief class to construct a subflow graph from the execution of a dynamic task

tf::Subflow 是 tf::Runtime 的派生类，具有专门的机制来管理 child graph 的执行。 默认情况下， subflow自动  加入其父节点。 
您可以分别通过调用 tf::Subflow::join 或 tf::Subflow::detach 显式 join or detach a subflow。
 以下示例创建了一个 taskflow graph ，该图从任务  B 的执行中生成一个 subflow ，该 subflow 包含三个任务：  
  B1、 B2 和  B3，其中  B3 在 B1 和  B1 之后运行  B2。




@code{.cpp}
// create three static tasks
tf::Task A = taskflow.emplace([](){}).name("A");
tf::Task C = taskflow.emplace([](){}).name("C");
tf::Task D = taskflow.emplace([](){}).name("D");

// create a subflow graph (dynamic tasking)
tf::Task B = taskflow.emplace([] (tf::Subflow& subflow) {
  tf::Task B1 = subflow.emplace([](){}).name("B1");
  tf::Task B2 = subflow.emplace([](){}).name("B2");
  tf::Task B3 = subflow.emplace([](){}).name("B3");
  B1.precede(B3);
  B2.precede(B3);
}).name("B");

A.precede(B);  // B runs after A
A.precede(C);  // C runs after A
B.precede(D);  // D runs after B
C.precede(D);  // D runs after C
@endcode

*/
class Subflow : public FlowBuilder,
                public Runtime {

  friend class Executor;
  friend class FlowBuilder;
  friend class Runtime;

  public:

    /**
    @brief enables the subflow to join its parent task
 
立即执行操作以加入 subflow。  子流程joined后，即视为完成，您不能再修改subflow

    @code{.cpp}
    taskflow.emplace([](tf::Subflow& sf){
      sf.emplace([](){});
      sf.join();  // join the subflow of one task
    });
    @endcode

只有生成此 subflow 的 worker 才能  join 它。 
    */
    void join();



    /**
    @brief 使 subflow 能够与其  parent task 分离  

    执行立即操作以  detach the subflow 。 分离子流后，它被认为已完成，您不能再修改 subflow  

    @code{.cpp}
    taskflow.emplace([](tf::Subflow& sf){
      sf.emplace([](){});
      sf.detach();
    });
    @endcode

    只有生成此 subflow  的 worker  才能将其 detach  
    */
    void detach();



    /**
    @brief  将 subflow 重置为 joinable 状态  

    @param clear_graph specifies whether to clear the associated graph (default @c true)

    根据给定变量 clear_graph（默认 true）清除 underlying task graph  ，然后将 subflow  更新为 joinable  状态。
    */
    void reset(bool clear_graph = true);



    /**
    @brief queries if the subflow is joinable
    此成员函数查询 subflow 是否 joinable 。 当一个 subflow 被  joined or detached, ，它变得 not joinable.
    @code{.cpp}
    taskflow.emplace([](tf::Subflow& sf){
      sf.emplace([](){});
      std::cout << sf.joinable() << '\n';  // true
      sf.join();
      std::cout << sf.joinable() << '\n';  // false
    });
    @endcode
    */
    bool joinable() const noexcept;

  private:

    bool _joinable {true};

    Subflow(Executor&, Worker&, Node*, Graph&);
};

// Constructor
inline Subflow::Subflow(Executor& executor, Worker& worker, Node* parent, Graph& graph) :
  FlowBuilder {graph},
  Runtime {executor, worker, parent} {
  // assert(_parent != nullptr);
}

// Function: joined
inline bool Subflow::joinable() const noexcept {
  return _joinable;
}

// Procedure: reset
inline void Subflow::reset(bool clear_graph) {
  if(clear_graph) {
    _graph._clear();
  }
  _joinable = true;
}

}  // end of namespace tf. ---------------------------------------------------










