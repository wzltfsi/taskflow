#pragma once

#include "observer.hpp"
#include "taskflow.hpp"
#include "async_task.hpp"

/**
@file executor.hpp
@brief executor include file
*/

namespace tf {

// ----------------------------------------------------------------------------
// Executor Definition
// ----------------------------------------------------------------------------

/** @class Executor

@brief class to create an executor for running a taskflow graph

executor 使用高效的工作窃取调度算法管理一组工作线程来运行一个或多个任务流。
 
@code{.cpp}
// Declare an executor and a taskflow
tf::Executor executor;
tf::Taskflow taskflow;

// Add three tasks into the taskflow
tf::Task A = taskflow.emplace([] () { std::cout << "This is TaskA\n"; });
tf::Task B = taskflow.emplace([] () { std::cout << "This is TaskB\n"; });
tf::Task C = taskflow.emplace([] () { std::cout << "This is TaskC\n"; });

// Build precedence between tasks
A.precede(B, C);

tf::Future<void> fu = executor.run(taskflow);
fu.wait();                // block until the execution completes

executor.run(taskflow, [](){ std::cout << "end of 1 run"; }).wait();
executor.run_n(taskflow, 4);
executor.wait_for_all();  // block until all associated executions finish
executor.run_n(taskflow, 4, [](){ std::cout << "end of 4 runs"; }).wait();
executor.run_until(taskflow, [cnt=0] () mutable { return ++cnt == 10; });
@endcode

所有  run 方法都是 线程安全的。你可以从不同的线程同时向一个 executor 提交多个任务流。
*/
class Executor {

  friend class FlowBuilder;
  friend class Subflow;
  friend class Runtime;

  public:

  /**
  @brief constructs the executor with @c N worker threads

  @param N number of workers (default std::thread::hardware_concurrency)
  @param wix worker interface class to alter worker (thread) behaviors
  
  构造函数 生成 N 个工作线程以在工作窃取循环中运行任务。 worker的数量必须大于零，否则将抛出异常。 
  默认情况下，工作线程数等于 std::thread::hardware_concurrency 返回的最大硬件并发数。
   用户可以通过从 tf::WorkerInterface 派生一个实例来改变 worker 的行为，例如改变线程关联。
  */
  explicit Executor( size_t N = std::thread::hardware_concurrency(), std::shared_ptr<WorkerInterface> wix = nullptr );

  /**
  @brief destructs the executor
   析构函数调用 Executor::wait_for_all 等待所有提交的任务流完成，然后通知所有工作线程停止并 join 这些线程。
  */
  ~Executor();

  /**
  @brief runs a taskflow once

  @param taskflow a tf::Taskflow object

  @return a tf::Future that holds the result of the execution

该成员函数执行一次给定的任务流，并返回一个最终保存执行结果的 tf::Future 对象。这个成员函数是线程安全的。
executor不拥有给定的任务流。 您有责任确保任务流在执行期间保持活动状态。
  @code{.cpp}
  tf::Future<void> future = executor.run(taskflow);
  // do something else
  future.wait();
  @endcode
  */
  tf::Future<void> run(Taskflow& taskflow);


  /**
  @brief runs a moved taskflow once

  @param taskflow a moved tf::Taskflow object

  @return a tf::Future that holds the result of the execution
   该成员函数执行一次移动的任务流，并返回一个最终保存执行结果的 tf::Future 对象。  executor 将负责移动任务流的生命周期。
这个成员函数是线程安全的。
  @code{.cpp}
  tf::Future<void> future = executor.run(std::move(taskflow));
  // do something else
  future.wait();
  @endcode
  */
  tf::Future<void> run(Taskflow&& taskflow);

  /**
  @brief runs a taskflow once and invoke a callback upon completion

  @param taskflow a tf::Taskflow object
  @param callable a callable object to be invoked after this run

  @return a tf::Future that holds the result of the execution

此成员函数执行一次给定的任务流，并在执行完成时调用给定的可调用对象。 该成员函数返回一个 tf::Future 对象，该对象最终保存执行结果。
该成员函数是线程安全的。 executor 不拥有给定的任务流。 您有责任确保任务流在执行期间保持活动状态。
 
  @code{.cpp}
  tf::Future<void> future = executor.run(taskflow, [](){ std::cout << "done"; });
  // do something else
  future.wait();
  @endcode
  */
  template<typename C>
  tf::Future<void> run(Taskflow& taskflow, C&& callable);



  /**
  @brief runs a moved taskflow once and invoke a callback upon completion

  @param taskflow a moved tf::Taskflow object
  @param callable a callable object to be invoked after this run

  @return a tf::Future that holds the result of the execution

   此成员函数执行一次移动的任务流，并在执行完成时调用给定的可调用对象。 
   该成员函数返回一个 tf::Future 对象，该对象最终保存执行结果。  executor 将负责移动任务流的生命周期。
   这个成员函数是线程安全的。
  @code{.cpp}
  tf::Future<void> future = executor.run(std::move(taskflow), [](){ std::cout << "done"; });
  // do something else
  future.wait();
  @endcode
  */
  template<typename C>
  tf::Future<void> run(Taskflow&& taskflow, C&& callable);

  /**
  @brief runs a taskflow for @c N times

  @param taskflow a tf::Taskflow object
  @param N number of runs

  @return a tf::Future that holds the result of the execution

该成员函数执行给定的taskflow  N次，返回一个tf::Future对象，最终保存执行结果。该成员函数是线程安全的。 
executor 不拥有给定的任务流。 您有责任确保任务流在执行期间保持活动状态。
  
  @code{.cpp}
  tf::Future<void> future = executor.run_n(taskflow, 2);  // run taskflow 2 times
  // do something else
  future.wait();
  @endcode
  */
  tf::Future<void> run_n(Taskflow& taskflow, size_t N);

  /**
  @brief runs a moved taskflow for @c N times

  @param taskflow a moved tf::Taskflow object
  @param N number of runs

  @return a tf::Future that holds the result of the execution

该成员函数执行移动的任务流  N 次，并返回一个最终保存执行结果的 tf::Future 对象。 
executor 将负责移动任务流的生命周期。 这个成员函数是线程安全的。

  @code{.cpp}
  tf::Future<void> future = executor.run_n( std::move(taskflow), 2 );
  // do something else
  future.wait();
  @endcode
  */
  tf::Future<void> run_n(Taskflow&& taskflow, size_t N);



  /**
  @brief runs a taskflow for @c N times and then invokes a callback

  @param taskflow a tf::Taskflow
  @param N number of runs
  @param callable a callable object to be invoked after this run

  @return a tf::Future that holds the result of the execution
    此成员函数执行给定的任务流  N 次，并在执行完成时调用给定的可调用对象。
    该成员函数返回一个 tf::Future 对象，该对象最终保存执行结果。 这个成员函数是线程安全的。
     executor 不拥有给定的任务流。 您有责任确保任务流在执行期间保持活动状态。
  @code{.cpp}
  tf::Future<void> future = executor.run(  taskflow, 2, [](){ std::cout << "done"; }    );
  // do something else
  future.wait();
  @endcode
  */
  template<typename C>
  tf::Future<void> run_n(Taskflow& taskflow, size_t N, C&& callable);



  /**
  @brief runs a moved taskflow for @c N times and then invokes a callback

  @param taskflow a moved tf::Taskflow
  @param N number of runs
  @param callable a callable object to be invoked after this run

  @return a tf::Future that holds the result of the execution
   此成员函数执行移动的任务流 N 次，并在执行完成时调用给定的可调用对象。 
   该成员函数返回一个 tf::Future 对象，该对象最终保存执行结果。这个成员函数是线程安全的。

  @code{.cpp}
  tf::Future<void> future = executor.run_n( std::move(taskflow), 2, [](){ std::cout << "done"; });
  // do something else
  future.wait();
  @endcode
  */
  template<typename C>
  tf::Future<void> run_n(Taskflow&& taskflow, size_t N, C&& callable);


  /**
  @brief runs a taskflow multiple times until the predicate becomes true

  @param taskflow a tf::Taskflow
  @param pred a boolean predicate to return @c true for stop

  @return a tf::Future that holds the result of the execution

  此成员函数多次执行给定的任务流，直到谓词返回  true。 该成员函数返回一个 tf::Future 对象，该对象最终持有执行的结果。
  该成员函数是线程安全的。 executor 不拥有给定的任务流。 您有责任确保任务流在执行期间保持活动状态。

  @code{.cpp}
  tf::Future<void> future = executor.run_until(taskflow, [](){ return rand()%10 == 0 });
  // do something else
  future.wait();
  @endcode
  */
  template<typename P>
  tf::Future<void> run_until(Taskflow& taskflow, P&& pred);



  /**
  @brief runs a moved taskflow and keeps running it
         until the predicate becomes true

  @param taskflow a moved tf::Taskflow object
  @param pred a boolean predicate to return @c true for stop

  @return a tf::Future that holds the result of the execution

此成员函数多次执行移动的任务流，直到谓词返回 @c true。 该成员函数返回一个 tf::Future 对象，该对象最终保存执行结果。 
executor 将负责移动任务流的生命周期。 这个成员函数是线程安全的。

  @code{.cpp}
  tf::Future<void> future = executor.run_until(  std::move(taskflow), [](){ return rand()%10 == 0 });
  // do something else
  future.wait();
  @endcode
  */
  template<typename P>
  tf::Future<void> run_until(Taskflow&& taskflow, P&& pred);

  /**
  @brief runs a taskflow multiple times until the predicate becomes true and
         then invokes the callback

  @param taskflow a tf::Taskflow
  @param pred a boolean predicate to return @c true for stop
  @param callable a callable object to be invoked after this run completes

  @return a tf::Future that holds the result of the execution

此成员函数多次执行给定的任务流，直到谓词返回  true，然后在执行完成时调用给定的可调用对象。 该成员函数返回一个 tf::Future 对象，该对象最终保存执行结果。
 这个成员函数是线程安全的。 executor  不拥有给定的任务流。 您有责任确保任务流在执行期间保持活动状态。

  @code{.cpp}
  tf::Future<void> future = executor.run_until(  taskflow, [](){ return rand()%10 == 0 }, [](){ std::cout << "done"; });
  // do something else
  future.wait();
  @endcode
  */
  template<typename P, typename C>
  tf::Future<void> run_until(Taskflow& taskflow, P&& pred, C&& callable);

  /**
  @brief runs a moved taskflow and keeps running
         it until the predicate becomes true and then invokes the callback

  @param taskflow a moved tf::Taskflow
  @param pred a boolean predicate to return @c true for stop
  @param callable a callable object to be invoked after this run completes

  @return a tf::Future that holds the result of the execution

  此成员函数多次执行移动的任务流，直到谓词返回c true，然后在执行完成时调用给定的可调用对象。 
  该成员函数返回一个 tf::Future 对象，该对象最终保存执行结果。  executor 将负责移动任务流的生命周期。 这个成员函数是线程安全的。

  @code{.cpp}
  tf::Future<void> future = executor.run_until(   std::move(taskflow),  [](){ return rand()%10 == 0 }, [](){ std::cout << "done"; } );
  // do something else
  future.wait();
  @endcode
  */
  template<typename P, typename C>
  tf::Future<void> run_until(Taskflow&& taskflow, P&& pred, C&& callable);

  /**
  @brief runs a target graph and waits until it completes using an internal worker of this executor
  
  @tparam T target type which has `tf::Graph& T::graph()` defined
  @param target the target task graph object


该方法运行一个 target graph ，该图定义了“tf::Graph& T::graph()”并等待执行完成。 
与调用 `tf::Executor::run` 系列并等待结果的典型流程不同，此方法必须由该 executor 的内部工作者调用。 
caller worker 将参与调度器的工作窃取循环，从而避免阻塞等待导致的潜在死锁。

  @code{.cpp}
  tf::Executor executor(2);
  tf::Taskflow taskflow;
  std::array<tf::Taskflow, 1000> others;
  
  std::atomic<size_t> counter{0};
  
  for(size_t n=0; n<1000; n++) {
    for(size_t i=0; i<1000; i++) {
      others[n].emplace([&](){ counter++; });
    }
    taskflow.emplace([&executor, &tf=others[n]](){
      executor.corun(tf);     // executor.run(tf).wait();  <- 阻塞 worker 什么都不做会引入死锁
    });
  }
  executor.run(taskflow).wait();
  @endcode 

  只要目标不是由两个或多个线程同时运行，该方法就是线程安全的。
  您必须从调用 executor 的工作程序中调用 tf::Executor::corun ，否则将抛出异常。
  */
  template <typename T>
  void corun(T& target);



  /**
  @brief 继续运行工作窃取循环，直到谓词变为真 
  
  @tparam P predicate type
  @param predicate a boolean predicate to indicate when to stop the loop

该方法使调用方工作程序在工作窃取循环中运行，直到停止谓词变为真。 
您必须从调用 executor 的工作程序中调用 tf::Executor::corun_until，否则将抛出异常。

  @code{.cpp}
  taskflow.emplace([&](){
    std::future<void> fu = std::async([](){ std::sleep(100s); });
    executor.corun_until([](){
      return fu.wait_for(std::chrono::seconds(0)) == future_status::ready;
    });
  });
  @endcode
  */
  template <typename P>
  void corun_until(P&& predicate);


  /**
  @brief waits for all tasks to complete

 此成员函数等待所有提交的任务（例如，任务流、异步任务）完成。

  @code{.cpp}
  executor.run(taskflow1);
  executor.run_n(taskflow2, 10);
  executor.run_n(taskflow3, 100);
  executor.wait_for_all();  // wait until the above submitted taskflows finish
  @endcode
  */
  void wait_for_all();



  /**
  @brief queries the number of worker threads
每个 worker 代表一个由 executor 在其构建时间生成的唯一线程。
 
  @code{.cpp}
  tf::Executor executor(4);
  std::cout << executor.num_workers();    // 4
  @endcode
  */
  size_t num_workers() const noexcept;


  /**
  @brief  查询本次调用时正在运行的拓扑数量   
 
  将任务流提交给 executor 时，会创建一个拓扑来存储正在运行的任务流的运行时元数据。 当提交的 taskflow  执行完成时，其对应的拓扑将从执行器中删除。

  @code{.cpp}
  executor.run(taskflow);
  std::cout << executor.num_topologies();  // 0 or 1 (taskflow still running)
  @endcode
  */
  size_t num_topologies() const;



  /**
  @brief queries the number of running taskflows with moved ownership

  @code{.cpp}
  executor.run(std::move(taskflow));
  std::cout << executor.num_taskflows();  // 0 or 1 (taskflow still running)
  @endcode
  */
  size_t num_taskflows() const;
  

  
  /**
  @brief queries the id of the caller thread in this executor

  每个 worker 都有一个唯一的 id，在   0 到   N-1 的范围内，与其  parent executor 相关联。 如果调用者线程不属于 executor ，则返回@c -1。

  @code{.cpp}
  tf::Executor executor(4);   // 4 workers in the executor
  executor.this_worker_id();  // -1 (main thread is not a worker)

  taskflow.emplace([&](){
    std::cout << executor.this_worker_id();  // 0, 1, 2, or 3
  });
  executor.run(taskflow);
  @endcode
  */
  int this_worker_id() const;
 


  // --------------------------------------------------------------------------
  // Observer methods
  // --------------------------------------------------------------------------

  /**
  @brief constructs an observer to inspect the activities of worker threads

  @tparam Observer observer type derived from tf::ObserverInterface
  @tparam ArgsT argument parameter pack

  @param args arguments to forward to the constructor of the observer

  @return a shared pointer to the created observer

每个 executor 管理一个与调用者共享所有权的观察者列表。 对于这些观察者中的每一个，
两个成员函数 tf::ObserverInterface::on_entry 和 tf::ObserverInterface::on_exit 将在任务执行之前和之后被调用。
 此成员函数不是线程安全的。 
  */
  template <typename Observer, typename... ArgsT>
  std::shared_ptr<Observer> make_observer(ArgsT&&... args);



  //  从 executor 中移除观察者 , 此成员函数不是线程安全的。 
  template <typename Observer>
  void remove_observer(std::shared_ptr<Observer> observer);

  // 查询观察者数量
  size_t num_observers() const noexcept;



  // --------------------------------------------------------------------------
  // Async Task Methods
  // --------------------------------------------------------------------------

  /**
  @brief runs a given function asynchronously

  @tparam F callable type

  @param func callable object

  @return a @std_future that will hold the result of the execution

   该方法创建一个异步任务来运行给定的函数并返回一个  std_future 对象，该对象最终将保存返回值的结果。

  @code{.cpp}
  std::future<int> future = executor.async([](){
    std::cout << "create an asynchronous task and returns 1\n";
    return 1;
  });
  future.get();
  @endcode

 这个成员函数是线程安全的。 
  */
  template <typename F>
  auto async(F&& func);

  /**
  @brief runs a given function asynchronously and gives a name to this task

  @tparam F callable type

  @param name name of the asynchronous task
  @param func callable object

  @return a @std_future that will hold the result of the execution
  
  该方法创建并为异步任务分配一个名称以运行给定的函数，返回 @std_future 对象，该对象最终将保存结果分配的任务名称将出现在 executor 的观察者中。

  @code{.cpp}
  std::future<int> future = executor.async("name", [](){
    std::cout << "create an asynchronous task with a name and returns 1\n";
    return 1;
  });
  future.get();
  @endcode

这个成员函数是线程安全的。
  */
  template <typename F>
  auto async(const std::string& name, F&& func);



  /**
  @brief similar to tf::Executor::async but does not return a future object
  
  @tparam F callable type
  
  @param func callable object

此成员函数比 tf::Executor::async 更高效，建议在您不希望 @std_future 获取结果或同步执行时使用。
 这个成员函数是线程安全的。
  @code{.cpp}
  executor.silent_async([](){
    std::cout << "create an asynchronous task with no return\n";
  });
  executor.wait_for_all();
  @endcode 
  */
  template <typename F>
  void silent_async(F&& func);




  /**
  @brief similar to tf::Executor::async but does not return a future object

  @tparam F callable type

  @param name assigned name to the task
  @param func callable object

此成员函数比 tf::Executor::async 更高效，建议在您不希望 @std_future 获取结果或同步执行时使用。 分配的任务名称将出现在 executor的观察者中。
 
  @code{.cpp}
  executor.silent_async("name", [](){ std::cout << "create an asynchronous task with a name and no return\n"; });
  executor.wait_for_all();
  @endcode
 这个成员函数是线程安全的。
  */
  template <typename F>
  void silent_async(const std::string& name, F&& func);

  // --------------------------------------------------------------------------
  // Silent Dependent Async Methods
  // --------------------------------------------------------------------------
  
  /**
  @brief runs the given function asynchronously 
         when the given dependents finish

  @tparam F callable type
  @tparam Tasks task types convertible to tf::AsyncTask

  @param func callable object
  @param tasks asynchronous tasks on which this execution depends
  
  @return a tf::AsyncTask handle 
  
此成员函数比 tf::Executor::dependent_async 更高效，并且在您不希望 @std_future 获取结果或同步执行时鼓励使用。 
下面的示例创建了三个异步任务， A、 B 和 C，其中任务  C 在任务  A 和任务  B 之后运行。这个成员函数是线程安全的。

  @code{.cpp}
  tf::AsyncTask A = executor.silent_dependent_async([](){ printf("A\n"); });
  tf::AsyncTask B = executor.silent_dependent_async([](){ printf("B\n"); });
  executor.silent_dependent_async([](){ printf("C runs after A and B\n"); }, A, B);
  executor.wait_for_all();
  @endcode
  */
  template <typename F, typename... Tasks,  std::enable_if_t<all_same_v<AsyncTask, std::decay_t<Tasks>...>, void>* = nullptr>
  tf::AsyncTask silent_dependent_async(F&& func, Tasks&&... tasks);
  
  /**
  @brief names and runs the given function asynchronously     when the given dependents finish
  
  @tparam F callable type
  @tparam Tasks task types convertible to tf::AsyncTask

  @param name assigned name to the task
  @param func callable object
  @param tasks asynchronous tasks on which this execution depends
  
  @return a tf::AsyncTask handle 

  此成员函数比 tf::Executor::dependent_async 更高效，并且在您不希望 @std_future 获取结果或同步执行时鼓励使用。 
  下面的例子创建了 A、  B、  C三个异步任务，其中任务  C在任务 A 和任务 B 之后运行。分配的任务名称会出现在 executor 的观察者中 .
  这个成员函数是线程安全的。

  @code{.cpp}
  tf::AsyncTask A = executor.silent_dependent_async("A", [](){ printf("A\n"); });
  tf::AsyncTask B = executor.silent_dependent_async("B", [](){ printf("B\n"); });
  executor.silent_dependent_async(
    "C", [](){ printf("C runs after A and B\n"); }, A, B
  );
  executor.wait_for_all();
  @endcode
  */
  template <typename F, typename... Tasks, std::enable_if_t<all_same_v<AsyncTask, std::decay_t<Tasks>...>, void>* = nullptr>
  tf::AsyncTask silent_dependent_async(const std::string& name, F&& func, Tasks&&... tasks);
  


  /**
  @brief runs the given function asynchronously  when the given range of dependents finish
  
  @tparam F callable type
  @tparam I iterator type 

  @param func callable object
  @param first iterator to the beginning (inclusive)
  @param last iterator to the end (exclusive)
  
  @return a tf::AsyncTask handle 

  此成员函数比 tf::Executor::dependent_async 更高效，并且在您不希望 @std_future 获取结果或同步执行时鼓励使用。 
  下面的示例创建了三个异步任务， A、 B 和 C，其中任务 C 在任务  A 和任务  B 之后运行。这个成员函数是线程安全的。

  @code{.cpp}
  std::array<tf::AsyncTask, 2> array {
    executor.silent_dependent_async([](){ printf("A\n"); }),
    executor.silent_dependent_async([](){ printf("B\n"); })
  };
  executor.silent_dependent_async(
    [](){ printf("C runs after A and B\n"); }, array.begin(), array.end()
  );
  executor.wait_for_all();
  @endcode 
  */
  template <typename F, typename I,  std::enable_if_t<!std::is_same_v<std::decay_t<I>, AsyncTask>, void>* = nullptr>
  tf::AsyncTask silent_dependent_async(F&& func, I first, I last);
  


  /**
  @brief names and runs the given function asynchronously  when the given range of dependents finish
  
  @tparam F callable type
  @tparam I iterator type 

  @param name assigned name to the task
  @param func callable object
  @param first iterator to the beginning (inclusive)
  @param last iterator to the end (exclusive)

  @return a tf::AsyncTask handle 
  
  此成员函数比 tf::Executor::dependent_async 更高效，并且在您不希望 @std_future 获取结果或同步执行时鼓励使用。
  下面的例子创建了 A、  B、  C三个异步任务，其中任务 C 在任务 A 和任务  B之后运行。分配的任务名称会出现在 executor 的观察者中 
  这个成员函数是线程安全的。

  @code{.cpp}
  std::array<tf::AsyncTask, 2> array {
    executor.silent_dependent_async("A", [](){ printf("A\n"); }),
    executor.silent_dependent_async("B", [](){ printf("B\n"); })
  };
  executor.silent_dependent_async(
    "C", [](){ printf("C runs after A and B\n"); }, array.begin(), array.end()
  );
  executor.wait_for_all();
  @endcode
  */
  template <typename F, typename I, std::enable_if_t<!std::is_same_v<std::decay_t<I>, AsyncTask>, void>* = nullptr>
  tf::AsyncTask silent_dependent_async(const std::string& name, F&& func, I first, I last);
  
  // --------------------------------------------------------------------------
  // Dependent Async Methods
  // --------------------------------------------------------------------------
  
  /**
  @brief runs the given function asynchronously   when the given dependents finish
  
  @tparam F callable type
  @tparam Tasks task types convertible to tf::AsyncTask

  @param func callable object
  @param tasks asynchronous tasks on which this execution depends
  
  @return a pair of a tf::AsyncTask handle and    a @std_future that holds the result of the execution
  
  下面的示例创建了三个异步任务， A、 B 和 C，其中任务 C 在任务 A 和任务 B 之后运行。任务 C 返回一对它的 tf： :AsyncTask 句柄和一个最终将保存执行结果的 std::future<int>

  @code{.cpp}
  tf::AsyncTask A = executor.silent_dependent_async([](){ printf("A\n"); });
  tf::AsyncTask B = executor.silent_dependent_async([](){ printf("B\n"); });
  auto [C, fuC] = executor.dependent_async(
    [](){ 
      printf("C runs after A and B\n"); 
      return 1;
    }, 
    A, B
  );
  fuC.get();  // C 完成，这反过来意味着 A 和 B 都完成   
  @endcode

在指定任务依赖项时，您可以混合使用由 Executor::dependent_async 和 Executor::silent_dependent_async 返回的 tf::AsyncTask 句柄。 这个成员函数是线程安全的。
  */
  template <typename F, typename... Tasks, std::enable_if_t<all_same_v<AsyncTask, std::decay_t<Tasks>...>, void>* = nullptr >
  auto dependent_async(F&& func, Tasks&&... tasks);
  
  /**
  @brief names and runs the given function asynchronously  when the given dependents finish
  
  @tparam F callable type
  @tparam Tasks task types convertible to tf::AsyncTask
  
  @param name assigned name to the task
  @param func callable object
  @param tasks asynchronous tasks on which this execution depends
  
  @return a pair of a tf::AsyncTask handle and  a @std_future that holds the result of the execution
  
下面的示例创建了三个命名的异步任务， A、 B 和 C，其中任务 C 在任务 A 和任务 B 之后运行。
任务 C 返回一对它的 tf ::AsyncTask 句柄和最终将保存执行结果的 std::future<int>。 分配的任务名称将出现在 executor 的观察者中。

  @code{.cpp}
  tf::AsyncTask A = executor.silent_dependent_async("A", [](){ printf("A\n"); });
  tf::AsyncTask B = executor.silent_dependent_async("B", [](){ printf("B\n"); });
  auto [C, fuC] = executor.dependent_async(
    "C",
    [](){ 
      printf("C runs after A and B\n"); 
      return 1;
    }, 
    A, B
  );
  assert(fuC.get()==1);  // C 完成，这反过来意味着 A 和 B 都完成
  @endcode

在指定任务依赖项时，您可以混合使用由 Executor::dependent_async 和 Executor::silent_dependent_async 返回的 tf::AsyncTask 句柄。 这个成员函数是线程安全的。
  */
  template <typename F, typename... Tasks, std::enable_if_t<all_same_v<AsyncTask, std::decay_t<Tasks>...>, void>* = nullptr >
  auto dependent_async(const std::string& name, F&& func, Tasks&&... tasks);
  
  /**
  @brief runs the given function asynchronously  when the given range of dependents finish
  
  @tparam F callable type
  @tparam I iterator type 

  @param func callable object
  @param first iterator to the beginning (inclusive)
  @param last iterator to the end (exclusive)
  
  @return a pair of a tf::AsyncTask handle and   a @std_future that holds the result of the execution
  
  下面的示例创建了三个异步任务， A、 B 和 C，其中任务 C 在任务 A 和任务 B 之后运行。任务 C 返回一对它的 tf： :AsyncTask 句柄和一个最终将保存执行结果的 std::future<int>

  @code{.cpp}
  std::array<tf::AsyncTask, 2> array {
    executor.silent_dependent_async([](){ printf("A\n"); }),
    executor.silent_dependent_async([](){ printf("B\n"); })
  };
  auto [C, fuC] = executor.dependent_async(
    [](){ 
      printf("C runs after A and B\n"); 
      return 1;
    }, 
    array.begin(), array.end()
  );
  assert(fuC.get()==1);  // C 完成，这反过来意味着 A 和 B 都完成
  @endcode

  在指定任务依赖时，可以混合使用 Executor::dependent_async 和 Executor::silent_dependent_async 返回的 tf::AsyncTask 句柄。该成员函数是线程安全的。
  */
  template <typename F, typename I, std::enable_if_t<!std::is_same_v<std::decay_t<I>, AsyncTask>, void>* = nullptr>
  auto dependent_async(F&& func, I first, I last);
  
  /**
  @brief names and runs the given function asynchronously   when the given range of dependents finish
  
  @tparam F callable type
  @tparam I iterator type 
  
  @param name assigned name to the task
  @param func callable object
  @param first iterator to the beginning (inclusive)
  @param last iterator to the end (exclusive)
  
  @return a pair of a tf::AsyncTask handle and  a @std_future that holds the result of the execution
  
  下面的示例创建了三个命名的异步任务， A、 B 和 C，其中任务 C 在任务 A 和任务 B 之后运行。
  任务 C 返回一对它的 tf ::AsyncTask 句柄和最终将保存执行结果的 std::future<int>。 
  分配的任务名称将出现在 executor 的观察者中。
  
  @code{.cpp}
  std::array<tf::AsyncTask, 2> array {
    executor.silent_dependent_async("A", [](){ printf("A\n"); }),
    executor.silent_dependent_async("B", [](){ printf("B\n"); })
  };
  auto [C, fuC] = executor.dependent_async(
    "C",
    [](){ 
      printf("C runs after A and B\n"); 
      return 1;
    }, 
    array.begin(), array.end()
  );
  assert(fuC.get()==1);  // C finishes, which in turns means both A and B finish
  @endcode

  在指定任务依赖项时，您可以混合使用 Executor::dependent_async 和 Executor::silent_dependent_async 返回的 tf::AsyncTask 句柄。 这个成员函数是线程安全的。
  */
  template <typename F, typename I, std::enable_if_t<!std::is_same_v<std::decay_t<I>, AsyncTask>, void>* = nullptr >
  auto dependent_async(const std::string& name, F&& func, I first, I last);

  private:
    
  const size_t _MAX_STEALS;

  std::condition_variable    _topology_cv;
  std::mutex                 _taskflows_mutex;
  std::mutex                 _topology_mutex;
  std::mutex                 _wsq_mutex;
  std::mutex                 _asyncs_mutex;

  size_t _num_topologies {0};
  
  std::unordered_map<std::thread::id, size_t>   _wids;
  std::vector<std::thread>                      _threads;
  std::vector<Worker>                           _workers;
  std::list<Taskflow>                           _taskflows;

  std::unordered_set<std::shared_ptr<Node>>     _asyncs;

  Notifier           _notifier;

  TaskQueue<Node*>   _wsq;

  std::atomic<bool>  _done {0};

  std::shared_ptr<WorkerInterface>                       _worker_interface;
  std::unordered_set<std::shared_ptr<ObserverInterface>> _observers;

  Worker* _this_worker();

  bool _wait_for_task(Worker&, Node*&);

  void _observer_prologue(Worker&, Node*);
  void _observer_epilogue(Worker&, Node*);
  void _spawn(size_t);
  void _exploit_task(Worker&, Node*&);
  void _explore_task(Worker&, Node*&);
  void _schedule(Worker&, Node*);
  void _schedule(Node*);
  void _schedule(Worker&, const SmallVector<Node*>&);
  void _schedule(const SmallVector<Node*>&);
  void _set_up_topology(Worker*, Topology*);
  void _tear_down_topology(Worker&, Topology*);
  void _tear_down_async(Node*);
  void _tear_down_dependent_async(Worker&, Node*);
  void _tear_down_invoke(Worker&, Node*);
  void _increment_topology();
  void _decrement_topology();
  void _decrement_topology_and_notify();
  void _invoke(Worker&, Node*);
  void _invoke_static_task(Worker&, Node*);
  void _invoke_dynamic_task(Worker&, Node*);
  void _consume_graph(Worker&, Node*, Graph&);
  void _detach_dynamic_task(Worker&, Node*, Graph&);
  void _invoke_condition_task(Worker&, Node*, SmallVector<int>&);
  void _invoke_multi_condition_task(Worker&, Node*, SmallVector<int>&);
  void _invoke_module_task(Worker&, Node*);
  void _invoke_async_task(Worker&, Node*);
  void _invoke_dependent_async_task(Worker&, Node*);
  void _process_async_dependent(Node*, tf::AsyncTask&, size_t&);
  void _schedule_async_task(Node*);
  
  template <typename P>
  void _corun_until(Worker&, P&&);
  
  template <typename R, typename F>
  auto _make_promised_async(std::promise<R>&&, F&&);
};

// Constructor
inline Executor::Executor(size_t N, std::shared_ptr<WorkerInterface> wix) :
  _MAX_STEALS {((N+1) << 1)},
  _threads    {N},
  _workers    {N},
  _notifier   {N},
  _worker_interface {std::move(wix)} {

  if(N == 0) {
    TF_THROW("no cpu workers to execute taskflows");
  }

  _spawn(N);

  // 如果需要，实例化 default observer 
  if(has_env(TF_ENABLE_PROFILER)) {
    TFProfManager::get()._manage(make_observer<TFProfObserver>());
  }
}

// Destructor
inline Executor::~Executor() {

  // 等待所有 topologies 完成   
  wait_for_all();

  // shut down the scheduler
  _done = true;

  _notifier.notify(true);

  for(auto& t : _threads){
    t.join();
  }
}

// Function: num_workers
inline size_t Executor::num_workers() const noexcept {
  return _workers.size();
}

// Function: num_topologies
inline size_t Executor::num_topologies() const {
  return _num_topologies;
}

// Function: num_taskflows
inline size_t Executor::num_taskflows() const {
  return _taskflows.size();
}

// Function: _this_worker
inline Worker* Executor::_this_worker() {
  auto itr = _wids.find(std::this_thread::get_id());
  return itr == _wids.end() ? nullptr : &_workers[itr->second];
}

// Function: this_worker_id
inline int Executor::this_worker_id() const {
  auto i = _wids.find(std::this_thread::get_id());
  return i == _wids.end() ? -1 : static_cast<int>(_workers[i->second]._id);
}

// Procedure: _spawn
inline void Executor::_spawn(size_t N) {

  std::mutex mutex;
  std::condition_variable cond;
  size_t n=0;

  for(size_t id=0; id<N; ++id) {

    _workers[id]._id = id;
    _workers[id]._vtm = id;
    _workers[id]._executor = this;
    _workers[id]._waiter = &_notifier._waiters[id];

    _threads[id] = std::thread([this] (
      Worker& w, std::mutex& mutex, std::condition_variable& cond, size_t& n
    ) -> void {
      
      // assign the thread
      w._thread = &_threads[w._id];

      // enables the mapping
      {
        std::scoped_lock lock(mutex);
        _wids[std::this_thread::get_id()] = w._id;
        if(n++; n == num_workers()) {
          cond.notify_one();
        }
      }

      Node* t = nullptr;
      
      // 在进入调度程序（工作窃取循环）之前，调用用户指定的prologue function
      if(_worker_interface) {
        _worker_interface->scheduler_prologue(w);
      }
      
      // 必须使用 1 作为条件而不是 !done 因为前面的 worker 可能会停止而后面的 worker 仍在准备进入调度循环
      std::exception_ptr ptr{nullptr};
      try {
        while(1) {

          // execute the tasks.
          _exploit_task(w, t);

          // wait for tasks
          if(_wait_for_task(w, t) == false) {
            break;
          }
        }
      } 
      catch(...) {
        ptr = std::current_exception();
      }
      
      // 调用用户指定的 epilogue function
      if(_worker_interface) {
        _worker_interface->scheduler_epilogue(w, ptr);
      }

    }, std::ref(_workers[id]), std::ref(mutex), std::ref(cond), std::ref(n));
  }

  std::unique_lock<std::mutex> lock(mutex);
  cond.wait(lock, [&](){ return n==N; });
}

// Function: _corun_until
template <typename P>
void Executor::_corun_until(Worker& w, P&& stop_predicate) {
  
  std::uniform_int_distribution<size_t> rdvtm(0, _workers.size()-1);

  exploit:

  while(!stop_predicate()) {
 
    if(auto t = w._wsq.pop(); t) {
      _invoke(w, t);
    }
    else {
      size_t num_steals = 0;

      explore:

      t = (w._id == w._vtm) ? _wsq.steal() : _workers[w._vtm]._wsq.steal();

      if(t) {
        _invoke(w, t);
        goto exploit;
      }
      else if(!stop_predicate()) {
        if(num_steals++ > _MAX_STEALS) {
          std::this_thread::yield();
        }
        w._vtm = rdvtm(w._rdgen);
        goto explore;
      }
      else {
        break;
      }
    }
  }
}

// Function: _explore_task
inline void Executor::_explore_task(Worker& w, Node*& t) {

  //assert(_workers[w].wsq.empty());
  //assert(!t);

  size_t num_steals = 0;
  size_t num_yields = 0;

  std::uniform_int_distribution<size_t> rdvtm(0, _workers.size()-1);
  
  // 在这里，我们编写 do-while 来让工作人员立即从指定的受害者那里窃取。
  do {
    t = (w._id == w._vtm) ? _wsq.steal() : _workers[w._vtm]._wsq.steal();

    if(t) {
      break;
    }

    if(num_steals++ > _MAX_STEALS) {
      std::this_thread::yield();
      if(num_yields++ > 100) {
        break;
      }
    }

    w._vtm = rdvtm(w._rdgen);
  } while(!_done);

}

// Procedure: _exploit_task
inline void Executor::_exploit_task(Worker& w, Node*& t) {
  while(t) {
    _invoke(w, t);
    t = w._wsq.pop();
  }
}

// Function: _wait_for_task
inline bool Executor::_wait_for_task(Worker& worker, Node*& t) {

  explore_task:

  _explore_task(worker, t);
  
  // 最后一个成功偷到 task 的小偷会叫醒另一个 thief worke 避免饿死。   
  if(t) {
    _notifier.notify(false);
    return true;
  }

  // ---- 2PC guard ----
  _notifier.prepare_wait(worker._waiter);

  if(!_wsq.empty()) {
    _notifier.cancel_wait(worker._waiter);
    worker._vtm = worker._id;
    goto explore_task;
  }
  
  if(_done) {
    _notifier.cancel_wait(worker._waiter);
    _notifier.notify(true);
    return false;
  }
  
  // 我们需要使用基于索引的扫描来避免与 _spawn 的数据竞争，这可能会同时初始化一个 worker。
  for(size_t vtm = 0; vtm < _workers.size(); vtm++) {
    if(!_workers[vtm]._wsq.empty()) {
      _notifier.cancel_wait(worker._waiter);
      worker._vtm = vtm;
      goto explore_task;
    }
  }
  
  // Now I really need to relinguish my self to others
  _notifier.commit_wait(worker._waiter);

  goto explore_task;
}



// Function: make_observer
template<typename Observer, typename... ArgsT>
std::shared_ptr<Observer> Executor::make_observer(ArgsT&&... args) {

  static_assert(std::is_base_of_v<ObserverInterface, Observer>, "Observer must be derived from ObserverInterface");

  // 使用局部变量来模拟构造函数
  auto ptr = std::make_shared<Observer>(std::forward<ArgsT>(args)...);

  ptr->set_up(_workers.size());

  _observers.emplace(std::static_pointer_cast<ObserverInterface>(ptr));

  return ptr;
}

// Procedure: remove_observer
template <typename Observer>
void Executor::remove_observer(std::shared_ptr<Observer> ptr) {

  static_assert( std::is_base_of_v<ObserverInterface, Observer>, "Observer must be derived from ObserverInterface" );

  _observers.erase(std::static_pointer_cast<ObserverInterface>(ptr));
}

// Function: num_observers
inline size_t Executor::num_observers() const noexcept {
  return _observers.size();
}


// Procedure: _schedule
inline void Executor::_schedule(Worker& worker, Node* node) {
  
  // 我们需要在释放之前获取 p，以便读取操作与其他线程正确同步以避免数据竞争。    
  auto p = node->_priority;

  node->_state.fetch_or(Node::READY, std::memory_order_release);

  // caller 是这个池的工作人员——从 v3.5 开始，我们不使用任何复杂的通知机制，因为实验结果没有显示出明显的优势。
  if(worker._executor == this) {
    worker._wsq.push(node, p);
    _notifier.notify(false);
    return;
  }

  {
    std::lock_guard<std::mutex> lock(_wsq_mutex);
    _wsq.push(node, p);
  }

  _notifier.notify(false);
}

// Procedure: _schedule
inline void Executor::_schedule(Node* node) {
  
  // 我们需要在释放之前获取 p，以便读取操作与其他线程正确同步，从而避免数据竞争。
  auto p = node->_priority;

  node->_state.fetch_or(Node::READY, std::memory_order_release);

  {
    std::lock_guard<std::mutex> lock(_wsq_mutex);
    _wsq.push(node, p);
  }

  _notifier.notify(false);
}

// Procedure: _schedule
inline void Executor::_schedule(Worker& worker, const SmallVector<Node*>& nodes) {

  // 我们需要捕获节点计数以避免在删除 parent topology 时访问节点向量！   
  const auto num_nodes = nodes.size();

  if(num_nodes == 0) {
    return;
  }

  // caller 是这个池的工作人员——从 v3.5 开始，我们不使用任何复杂的通知机制，因为实验结果没有显示出明显的优势。
  if(worker._executor == this) {
    for(size_t i=0; i<num_nodes; ++i) {
      // 我们需要在释放之前获取 p，以便读取操作与其他线程正确同步以避免数据竞争。
      auto p = nodes[i]->_priority;
      nodes[i]->_state.fetch_or(Node::READY, std::memory_order_release);
      worker._wsq.push(nodes[i], p);
      _notifier.notify(false);
    }
    return;
  }

  {
    std::lock_guard<std::mutex> lock(_wsq_mutex);
    for(size_t k=0; k<num_nodes; ++k) {
      auto p = nodes[k]->_priority;
      nodes[k]->_state.fetch_or(Node::READY, std::memory_order_release);
      _wsq.push(nodes[k], p);
    }
  }

  _notifier.notify_n(num_nodes);
}

// Procedure: _schedule
inline void Executor::_schedule(const SmallVector<Node*>& nodes) {

  // parent topology 可能会被移除！
  const auto num_nodes = nodes.size();

  if(num_nodes == 0) {
    return;
  }

  // 我们需要在释放之前获取 p，以便读取操作与其他线程正确同步，从而避免数据竞争。
  {
    std::lock_guard<std::mutex> lock(_wsq_mutex);
    for(size_t k=0; k<num_nodes; ++k) {
      auto p = nodes[k]->_priority;
      nodes[k]->_state.fetch_or(Node::READY, std::memory_order_release);
      _wsq.push(nodes[k], p);
    }
  }

  _notifier.notify_n(num_nodes);
}


// Procedure: _invoke
inline void Executor::_invoke(Worker& worker, Node* node) {

  // 同步由重新排序引起的所有未完成的内存操作  
  while(!(node->_state.load(std::memory_order_acquire) & Node::READY));

  begin_invoke:

  //  如果取消拓扑，则无需做其他事情  
  if(node->_is_cancelled()) {
    _tear_down_invoke(worker, node);
    return;
  }

  // 如果 acquiring 信号量存在，首先获取它们  
  if(node->_semaphores && !node->_semaphores->to_acquire.empty()) {
    SmallVector<Node*> nodes;
    if(!node->_acquire_all(nodes)) {
      _schedule(worker, nodes);
      return;
    }
    node->_state.fetch_or(Node::ACQUIRED, std::memory_order_release);
  }

 
  SmallVector<int> conds;

  //由于跳转表，switch 比嵌套的 if-else 更快
  switch(node->_handle.index()) {
    // static task
    case Node::STATIC:{
      _invoke_static_task(worker, node);
    }
    break;

    // dynamic task
    case Node::DYNAMIC: {
      _invoke_dynamic_task(worker, node);
    }
    break;

    // condition task
    case Node::CONDITION: {
      _invoke_condition_task(worker, node, conds);
    }
    break;

    // multi-condition task
    case Node::MULTI_CONDITION: {
      _invoke_multi_condition_task(worker, node, conds);
    }
    break;

    // module task
    case Node::MODULE: {
      _invoke_module_task(worker, node);
    }
    break;

    // async task
    case Node::ASYNC: {
      _invoke_async_task(worker, node);
      _tear_down_async(node);
      return ;
    }
    break;

    // dependent async task
    case Node::DEPENDENT_ASYNC: {
      _invoke_dependent_async_task(worker, node);
      _tear_down_dependent_async(worker, node);
      if(worker._cache) {
        node = worker._cache;
        goto begin_invoke;
      }
      return;
    }
    break;

    // monostate (placeholder)
    default:
    break;
  }

  // 如果存在 releasing 信号量，则释放它们  
  if(node->_semaphores && !node->_semaphores->to_release.empty()) {
    _schedule(worker, node->_release_all());
  }
  

   // 重置 join counter 以支持 循环控制流。
   // + 我们必须在安排 successors 之前执行此操作以避免_dependents 上的竞争条件。
   // + 我们必须使用 fetch_add 而不是直接分配，因为对 “invoke” 的用户空间调用可能会再次显式安排此任务（例如，管道），它可以访问 join_counter。
  if((node->_state.load(std::memory_order_relaxed) & Node::CONDITIONED)) {
    node->_join_counter.fetch_add(node->num_strong_dependents(), std::memory_order_relaxed);
  }
  else {
    node->_join_counter.fetch_add(node->num_dependents(), std::memory_order_relaxed);
  }

  // 获取 parent flow counter
  auto& j = (node->_parent) ? node->_parent->_join_counter :  node->_topology->_join_counter;

  // 在这里，我们要缓存最高优先级的 latest (最新) successor  
  worker._cache = nullptr;
  auto max_p = static_cast<unsigned>(TaskPriority::MAX);

  // Invoke the task based on the corresponding type
  switch(node->_handle.index()) {

    // condition and multi-condition tasks
    case Node::CONDITION:
    case Node::MULTI_CONDITION: {
      for(auto cond : conds) {
        if(cond >= 0 && static_cast<size_t>(cond) < node->_successors.size()) {
          auto s = node->_successors[cond];
          // zeroing the join counter for invariant
          s->_join_counter.store(0, std::memory_order_relaxed);
          j.fetch_add(1, std::memory_order_relaxed);
          if(s->_priority <= max_p) {
            if(worker._cache) {
              _schedule(worker, worker._cache);
            }
            worker._cache = s;
            max_p = s->_priority;
          }
          else {
            _schedule(worker, s);
          }
        }
      }
    }
    break;

    // non-condition task
    default: {
      for(size_t i=0; i<node->_successors.size(); ++i) {
        if(auto s = node->_successors[i]; 
          s->_join_counter.fetch_sub(1, std::memory_order_acq_rel) == 1) {
          j.fetch_add(1, std::memory_order_relaxed);
          if(s->_priority <= max_p) {
            if(worker._cache) {
              _schedule(worker, worker._cache);
            }
            worker._cache = s;
            max_p = s->_priority;
          }
          else {
            _schedule(worker, s);
          }
        }
      }
    }
    break;
  }

  // tear_down the invoke
  _tear_down_invoke(worker, node);

  // 对最右边的孩子执行尾部递归消除，以减少通过任务队列进行昂贵的弹出/压入操作的次数
  if(worker._cache) {
    node = worker._cache;
    goto begin_invoke;
  }
}

// Proecdure: _tear_down_invoke
inline void Executor::_tear_down_invoke(Worker& worker, Node* node) {
  // 我们必须在减去 join counter 之前先检查父级，否则它会引入数据竞争    
  if(node->_parent == nullptr) {
    if(node->_topology->_join_counter.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      _tear_down_topology(worker, node->_topology);
    }
  }
  // joined subflow
  else {  
    node->_parent->_join_counter.fetch_sub(1, std::memory_order_release);
  }
}

// Procedure: _observer_prologue
inline void Executor::_observer_prologue(Worker& worker, Node* node) {
  for(auto& observer : _observers) {
    observer->on_entry(WorkerView(worker), TaskView(*node));
  }
}

// Procedure: _observer_epilogue
inline void Executor::_observer_epilogue(Worker& worker, Node* node) {
  for(auto& observer : _observers) {
    observer->on_exit(WorkerView(worker), TaskView(*node));
  }
}

// Procedure: _invoke_static_task
inline void Executor::_invoke_static_task(Worker& worker, Node* node) {
  _observer_prologue(worker, node);
  auto& work = std::get_if<Node::Static>(&node->_handle)->work;
  switch(work.index()) {
    case 0:
      std::get_if<0>(&work)->operator()();
    break;

    case 1:
      Runtime rt(*this, worker, node);
      std::get_if<1>(&work)->operator()(rt);
    break;
  }
  _observer_epilogue(worker, node);
}

// Procedure: _invoke_dynamic_task
inline void Executor::_invoke_dynamic_task(Worker& w, Node* node) {

  _observer_prologue(w, node);

  auto handle = std::get_if<Node::Dynamic>(&node->_handle);

  handle->subgraph._clear();

  Subflow sf(*this, w, node, handle->subgraph);

  handle->work(sf);

  if(sf._joinable) {
    _consume_graph(w, node, handle->subgraph);
  }

  _observer_epilogue(w, node);
}

// Procedure: _detach_dynamic_task
inline void Executor::_detach_dynamic_task(Worker& w, Node* p, Graph& g) {

  // graph is empty and has no async tasks
  if(g.empty() && p->_join_counter.load(std::memory_order_acquire) == 0) {
    return;
  }

  SmallVector<Node*> src;

  for(auto n : g._nodes) {

    n->_state.store(Node::DETACHED, std::memory_order_relaxed);
    n->_set_up_join_counter();
    n->_topology = p->_topology;
    n->_parent = nullptr;

    if(n->num_dependents() == 0) {
      src.push_back(n);
    }
  }

  {
    std::lock_guard<std::mutex> lock(p->_topology->_taskflow._mutex);
    p->_topology->_taskflow._graph._merge(std::move(g));
  }

  p->_topology->_join_counter.fetch_add(src.size(), std::memory_order_relaxed);
  _schedule(w, src);
}



// Procedure: _consume_graph
inline void Executor::_consume_graph(Worker& w, Node* p, Graph& g) {

  // graph is empty and has no async tasks
  if(g.empty() && p->_join_counter.load(std::memory_order_acquire) == 0) {
    return;
  }

  SmallVector<Node*> src;

  for(auto n : g._nodes) {
    n->_state.store(0, std::memory_order_relaxed);
    n->_set_up_join_counter();
    n->_topology = p->_topology;
    n->_parent = p;
    if(n->num_dependents() == 0) {
      src.push_back(n);
    }
  }
  p->_join_counter.fetch_add(src.size(), std::memory_order_relaxed);
  
  _schedule(w, src);
  _corun_until(w, [p] () -> bool { return p->_join_counter.load(std::memory_order_acquire) == 0; });
}


// Procedure: _invoke_condition_task
inline void Executor::_invoke_condition_task(Worker& worker, Node* node, SmallVector<int>& conds) {
  _observer_prologue(worker, node);
  auto& work = std::get_if<Node::Condition>(&node->_handle)->work;
  switch(work.index()) {
    case 0:
      conds = { std::get_if<0>(&work)->operator()() };
    break;

    case 1:
      Runtime rt(*this, worker, node);
      conds = { std::get_if<1>(&work)->operator()(rt) };
    break;
  }
  _observer_epilogue(worker, node);
}


// Procedure: _invoke_multi_condition_task
inline void Executor::_invoke_multi_condition_task( Worker& worker, Node* node, SmallVector<int>& conds) {
  _observer_prologue(worker, node);
  auto& work = std::get_if<Node::MultiCondition>(&node->_handle)->work;
  switch(work.index()) {
    case 0:
      conds = std::get_if<0>(&work)->operator()();
    break;

    case 1:
      Runtime rt(*this, worker, node);
      conds = std::get_if<1>(&work)->operator()(rt);
    break;
  }
  _observer_epilogue(worker, node);
}



// Procedure: _invoke_module_task
inline void Executor::_invoke_module_task(Worker& w, Node* node) {
  _observer_prologue(w, node);
  _consume_graph(  w, node, std::get_if<Node::Module>(&node->_handle)->graph );
  _observer_epilogue(w, node);
}

// Procedure: _invoke_async_task
inline void Executor::_invoke_async_task(Worker& w, Node* node) {
  _observer_prologue(w, node);
  std::get_if<Node::Async>(&node->_handle)->work();
  _observer_epilogue(w, node);
}

// Procedure: _invoke_dependent_async_task
inline void Executor::_invoke_dependent_async_task(Worker& w, Node* node) {
  _observer_prologue(w, node);
  std::get_if<Node::DependentAsync>(&node->_handle)->work();
  _observer_epilogue(w, node);
}

// Function: run
inline tf::Future<void> Executor::run(Taskflow& f) {
  return run_n(f, 1, [](){});
}

// Function: run
inline tf::Future<void> Executor::run(Taskflow&& f) {
  return run_n(std::move(f), 1, [](){});
}

// Function: run
template <typename C>
tf::Future<void> Executor::run(Taskflow& f, C&& c) {
  return run_n(f, 1, std::forward<C>(c));
}

// Function: run
template <typename C>
tf::Future<void> Executor::run(Taskflow&& f, C&& c) {
  return run_n(std::move(f), 1, std::forward<C>(c));
}

// Function: run_n
inline tf::Future<void> Executor::run_n(Taskflow& f, size_t repeat) {
  return run_n(f, repeat, [](){});
}

// Function: run_n
inline tf::Future<void> Executor::run_n(Taskflow&& f, size_t repeat) {
  return run_n(std::move(f), repeat, [](){});
}

// Function: run_n
template <typename C>
tf::Future<void> Executor::run_n(Taskflow& f, size_t repeat, C&& c) {
  return run_until( f, [repeat]() mutable { return repeat-- == 0; }, std::forward<C>(c));
}

// Function: run_n
template <typename C>
tf::Future<void> Executor::run_n(Taskflow&& f, size_t repeat, C&& c) {
  return run_until( std::move(f), [repeat]() mutable { return repeat-- == 0; }, std::forward<C>(c) );
}

// Function: run_until
template<typename P>
tf::Future<void> Executor::run_until(Taskflow& f, P&& pred) {
  return run_until(f, std::forward<P>(pred), [](){});
}

// Function: run_until
template<typename P>
tf::Future<void> Executor::run_until(Taskflow&& f, P&& pred) {
  return run_until(std::move(f), std::forward<P>(pred), [](){});
}

// Function: run_until
template <typename P, typename C>
tf::Future<void> Executor::run_until(Taskflow& f, P&& p, C&& c) {

  _increment_topology();

  // 需要检查锁下的空，因为 dynamic task 可能定义同时修改 taskflow 的 detached blocks
  bool empty;
  {
    std::lock_guard<std::mutex> lock(f._mutex);
    empty = f.empty();
  }

  //// 不需要创建一个真正的 topology 但返回一个 dummy future
  if(empty || p()) {
    c();
    std::promise<void> promise;
    promise.set_value();
    _decrement_topology_and_notify();
    return tf::Future<void>(promise.get_future(), std::monostate{});
  }

  // create a topology for this run
  auto t = std::make_shared<Topology>(f, std::forward<P>(p), std::forward<C>(c));

  // 需要在拓扑被快速拆除之前创建 future 
  tf::Future<void> future(t->_promise.get_future(), t);

  // 修改拓扑需要加锁保护 
  {
    std::lock_guard<std::mutex> lock(f._mutex);
    f._topologies.push(t);
    if(f._topologies.size() == 1) {
      _set_up_topology(_this_worker(), t.get());
    }
  }

  return future;
}

// Function: run_until
template <typename P, typename C>
tf::Future<void> Executor::run_until(Taskflow&& f, P&& pred, C&& c) {

  std::list<Taskflow>::iterator itr;

  {
    std::scoped_lock<std::mutex> lock(_taskflows_mutex);
    itr = _taskflows.emplace(_taskflows.end(), std::move(f));
    itr->_satellite = itr;
  }

  return run_until(*itr, std::forward<P>(pred), std::forward<C>(c));
}

// Function: corun
template <typename T>
void Executor::corun(T& target) {
  
  auto w = _this_worker();

  if(w == nullptr) {
    TF_THROW("corun must be called by a worker of the executor");
  }

  Node parent;  // dummy parent
  _consume_graph(*w, &parent, target.graph());
}

// Function: corun_until
template <typename P>
void Executor::corun_until(P&& predicate) {
  
  auto w = _this_worker();

  if(w == nullptr) {
    TF_THROW("corun_until must be called by a worker of the executor");
  }

  _corun_until(*w, std::forward<P>(predicate));
}

// Procedure: _increment_topology
inline void Executor::_increment_topology() {
  std::lock_guard<std::mutex> lock(_topology_mutex);
  ++_num_topologies;
}

// Procedure: _decrement_topology_and_notify
inline void Executor::_decrement_topology_and_notify() {
  std::lock_guard<std::mutex> lock(_topology_mutex);
  if(--_num_topologies == 0) {
    _topology_cv.notify_all();
  }
}

// Procedure: _decrement_topology
inline void Executor::_decrement_topology() {
  std::lock_guard<std::mutex> lock(_topology_mutex);
  --_num_topologies;
}

// Procedure: wait_for_all
inline void Executor::wait_for_all() {
  std::unique_lock<std::mutex> lock(_topology_mutex);
  _topology_cv.wait(lock, [&](){ return _num_topologies == 0; });
}

// Function: _set_up_topology
inline void Executor::_set_up_topology(Worker* worker, Topology* tpg) {

  // ---- under taskflow lock ----

  tpg->_sources.clear();
  tpg->_taskflow._graph._clear_detached();

  // 扫描图中的每个节点并建立链接
  for(auto node : tpg->_taskflow._graph._nodes) {

    node->_topology = tpg;
    node->_parent = nullptr;
    node->_state.store(0, std::memory_order_relaxed);

    if(node->num_dependents() == 0) {
      tpg->_sources.push_back(node);
    }

    node->_set_up_join_counter();
  }

  tpg->_join_counter.store(tpg->_sources.size(), std::memory_order_relaxed);

  if(worker) {
    _schedule(*worker, tpg->_sources);
  }
  else {
    _schedule(tpg->_sources);
  }
}


// Function: _tear_down_topology
inline void Executor::_tear_down_topology(Worker& worker, Topology* tpg) {

  auto &f = tpg->_taskflow;

  //assert(&tpg == &(f._topologies.front()));

  // case 1: 我们仍然需要再次运行 topology topology 
  if(!tpg->_is_cancelled && !tpg->_pred()) {
    //assert(tpg->_join_counter == 0);
    std::lock_guard<std::mutex> lock(f._mutex);
    tpg->_join_counter.store(tpg->_sources.size(), std::memory_order_relaxed);
    _schedule(worker, tpg->_sources);
  }
  // case 2: 此 topology 的最终运行 
  else {

    // TODO: 如果 topology 被取消，需要释放所有信号量  
    if(tpg->_call != nullptr) {
      tpg->_call();
    }

    // 如果有另一个 run （锁之间交错）  
    if(std::unique_lock<std::mutex> lock(f._mutex); f._topologies.size()>1) {
      //assert(tpg->_join_counter == 0);

      // Set the promise
      tpg->_promise.set_value();
      f._topologies.pop();
      tpg = f._topologies.front().get();

      // 减少 topology 但因为这不是最后一次我们不 notify
      _decrement_topology();

      // 设置 topology  需要在锁下，否则它会通过 pop 引入内存顺序错误
      _set_up_topology(&worker, tpg);
    }
    else {
      //assert(f._topologies.size() == 1);

      // 需要先在这里备份 promise，因为 taskflow 可能会在调用 get 后很快被破坏   
      auto p {std::move(tpg->_promise)};

      // 备份 lambda 捕获以防它具有拓扑指针，以避免它在 _mutex.unlock 和 _promise.set_value 之前在 pop_front 上释放。 离开作用域时安全释放。
      auto c {std::move(tpg->_call)};

      // Get the satellite if any
      auto s {f._satellite};

      // 现在我们从这个 taskflow 中删除 topology   
      f._topologies.pop();

      lock.unlock();

      // 我们最终 set the promise ，以防 taskflow 离开范围。 set_value 之后，调用者将从 wait 返回   
      p.set_value();

      _decrement_topology_and_notify();

      // 如果 taskflow 由 executor 管理，则将其移除
       // TODO：将来，我们可能需要在等待时进行同步  （这意味着以下代码应该移到 set_value 之前）
      if(s) {
        std::scoped_lock<std::mutex> lock(_taskflows_mutex);
        _taskflows.erase(*s);
      }
    }
  }
}

// ############################################################################
// Forward Declaration: Subflow
// ############################################################################

inline void Subflow::join() {

  // assert(this_worker().worker == &_worker);

  if(!_joinable) {
    TF_THROW("subflow not joinable");
  }

  // 只有 parent  worker 可以  join the subflow
  _executor._consume_graph(_worker, _parent, _graph);
  _joinable = false;
}

inline void Subflow::detach() {
  // assert(this_worker().worker == &_worker);
  if(!_joinable) {
    TF_THROW("subflow already joined or detached");
  }

  // 只有 parent worker  可以  detach the subflow
  _executor._detach_dynamic_task(_worker, _parent, _graph);
  _joinable = false;
}



// ############################################################################
// Forward Declaration: Runtime
// ############################################################################

// Procedure: schedule
inline void Runtime::schedule(Task task) {
  
  auto node = task._node;
  // 需要保持不变性：调度 task 时， task 必须具有零依赖性（连接计数器为 0），否则我们在插入嵌套流时会遇到错误（例如，模块任务）
  node->_join_counter.store(0, std::memory_order_relaxed);

  auto& j = node->_parent ? node->_parent->_join_counter :  node->_topology->_join_counter;
  j.fetch_add(1, std::memory_order_relaxed);
  _executor._schedule(_worker, node);
}


// Procedure: corun
template <typename T>
void Runtime::corun(T&& target) {

  // dynamic task (subflow)
  if constexpr(is_dynamic_task_v<T>) {
    Graph graph;
    Subflow sf(_executor, _worker, _parent, graph);
    target(sf);
    if(sf._joinable) {
      _executor._consume_graph(_worker, _parent, graph);
    }
  }
  // 定义了 `tf::Graph& T::graph()` 的可组合图形对象  
  else {
    _executor._consume_graph(_worker, _parent, target.graph());
  }
}

// Procedure: corun_until
template <typename P>
void Runtime::corun_until(P&& predicate) {
  _executor._corun_until(_worker, std::forward<P>(predicate));
}

// Function: _silent_async
template <typename F>
void Runtime::_silent_async(Worker& w, const std::string& name, F&& f) {

  _parent->_join_counter.fetch_add(1, std::memory_order_relaxed);

  auto node = node_pool.animate(
    name, 0, _parent->_topology, _parent, 0,
    std::in_place_type_t<Node::Async>{}, std::forward<F>(f)
  );

  _executor._schedule(w, node);
}

// Function: silent_async
template <typename F>
void Runtime::silent_async(F&& f) {
  _silent_async(*_executor._this_worker(), "", std::forward<F>(f));
}

// Function: silent_async
template <typename F>
void Runtime::silent_async(const std::string& name, F&& f) {
  _silent_async(*_executor._this_worker(), name, std::forward<F>(f));
}

// Function: silent_async_unchecked
template <typename F>
void Runtime::silent_async_unchecked(const std::string& name, F&& f) {
  _silent_async(_worker, name, std::forward<F>(f));
}

// Function: _async
template <typename F>
auto Runtime::_async(Worker& w, const std::string& name, F&& f) {

  _parent->_join_counter.fetch_add(1, std::memory_order_relaxed);

  using R = std::invoke_result_t<std::decay_t<F>>;

  std::promise<R> p;
  auto fu{p.get_future()};

  auto node = node_pool.animate(
    name, 0, _parent->_topology, _parent, 0,
    std::in_place_type_t<Node::Async>{},
    [p = make_moc(std::move(p)), f = std::forward<F>(f)] () mutable {
      if constexpr(std::is_same_v<R, void>) {
        f();
        p.object.set_value();
      }
      else {
        p.object.set_value(f());
      }
    }
  );

  _executor._schedule(w, node);
  return fu;
}

// Function: async
template <typename F>
auto Runtime::async(F&& f) {
  return _async(*_executor._this_worker(), "", std::forward<F>(f));
}

// Function: async
template <typename F>
auto Runtime::async(const std::string& name, F&& f) {
  return _async(*_executor._this_worker(), name, std::forward<F>(f));
}

// Function: join
inline void Runtime::join() {
  corun_until([this] () -> bool { 
    return _parent->_join_counter.load(std::memory_order_acquire) == 0; 
  });
}

}  // end of namespace tf -----------------------------------------------------






