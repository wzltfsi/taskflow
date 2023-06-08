#pragma once

#include "observer.hpp"
#include "taskflow.hpp"

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

executor 使用高效的工作窃取调度算法管理一组工作线程来运行一个或多个 taskflows
 

@code{.cpp}
tf::Executor executor;
tf::Taskflow taskflow;
 
tf::Task A = taskflow.emplace([] () { std::cout << "This is TaskA\n"; });
tf::Task B = taskflow.emplace([] () { std::cout << "This is TaskB\n"; });
tf::Task C = taskflow.emplace([] () { std::cout << "This is TaskC\n"; });
 
A.precede(B, C);

tf::Future<void> fu = executor.run(taskflow);
fu.wait();          

executor.run(taskflow, [](){ std::cout << "end of 1 run"; }).wait();
executor.run_n(taskflow, 4);
executor.wait_for_all();  // 阻塞直到所有关联的执行完成
executor.run_n(taskflow, 4, [](){ std::cout << "end of 4 runs"; }).wait();
executor.run_until(taskflow, [cnt=0] () mutable { return ++cnt == 10; });
@endcode

所有 run 方法都是 线程安全的。 您可以同时将多个任务流提交给来自不同线程的执行程序。
*/
class Executor {

  friend class FlowBuilder;
  friend class Subflow;
  friend class Runtime;

  public:

    /**
    @brief constructs the executor with @c N worker threads

    构造函数生成 N 个工作线程以在工作窃取循环中运行任务。 工人的数量必须大于零，否则将抛出异常。 
    默认情况下，工作线程数等于 std::thread::hardware_concurrency 返回的最大硬件并发数.
    */
    explicit Executor(size_t N = std::thread::hardware_concurrency());

    /**
    @brief destructs the executor
    析构函数调用 Executor::wait_for_all 等待所有提交的任务流完成，然后通知所有工作线程停止并加入这些线程。 
    */
    ~Executor();

    /**
    @brief runs a taskflow once

    @param taskflow a tf::Taskflow object
    @return a tf::Future that holds the result of the execution

    该成员函数执行一次给定的任务流，并返回一个最终保存执行结果的 tf::Future 对象。 这个成员函数是线程安全的。

    @code{.cpp}
    tf::Future<void> future = executor.run(taskflow);
    // do something else
    future.wait();
    @endcode
 

    @attention
    executor 不拥有给定的任务流。 您有责任确保任务流在执行期间保持活动状态。 
    */
    tf::Future<void> run(Taskflow& taskflow);


    /**
    @brief runs a moved taskflow once
    @param taskflow a moved tf::Taskflow object
    @return a tf::Future that holds the result of the execution

    该成员函数执行一次移动的任务流，并返回一个最终保存执行结果的 tf::Future 对象。 
    executor 将负责移动任务流的生命周期。这个成员函数是线程安全的。
   
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

    @code{.cpp}
    tf::Future<void> future = executor.run(taskflow, [](){ std::cout << "done"; });
    // do something else
    future.wait();
    @endcode

    This member function is thread-safe.

    executor 不拥有给定的任务流。 您有责任确保任务流在执行期间保持活动状态。 
    */
    template<typename C>
    tf::Future<void> run(Taskflow& taskflow, C&& callable);


    /**
    @brief runs a moved taskflow once and invoke a callback upon completion

    @param taskflow a moved tf::Taskflow object
    @param callable a callable object to be invoked after this run

    @return a tf::Future that holds the result of the execution

    此成员函数执行一次移动的 taskflow  ，并在执行完成时调用给定的可调用对象。 
    该成员函数返回一个 tf::Future 对象，该对象最终保存执行结果。 执行者将负责移动任务流的生命周期。
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
    
    该成员函数执行给定的任务流 N 次，并返回一个最终保存执行结果的 tf::Future 对象。这个成员函数是线程安全的。
     executor 不拥有给定的 taskflow 。 您有责任确保任务流在执行期间保持活动状态。
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

    该成员函数执行移动的任务流 N 次，并返回一个最终保存执行结果的 tf::Future 对象。 
    executor 将负责移动任务流的生命周期。这个成员函数是线程安全的。
    
    @code{.cpp}
    tf::Future<void> future = executor.run_n( std::move(taskflow), 2  );   // run the moved taskflow 2 times
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
    该成员函数返回一个 tf::Future 对象，该对象最终保存执行结果。这个成员函数是线程安全的
    executor 不拥有给定的任务流。 您有责任确保任务流在执行期间保持活动状态。
    @code{.cpp}
    tf::Future<void> future = executor.run( taskflow, 2, [](){ std::cout << "done"; } );
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
   此成员函数执行移动的任务流  N 次，并在执行完成时调用给定的可调用对象。 该成员函数返回一个 tf::Future 对象，该对象最终保存执行结果。
   这个成员函数是线程安全的。

    @code{.cpp}
    tf::Future<void> future = executor.run(  std::move(taskflow), 2, [](){ std::cout << "done"; } );
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
   此成员函数多次执行给定的任务流，直到谓词返回 @c true。 该成员函数返回一个 tf::Future 对象，该对象最终保存执行结果。
   这个成员函数是线程安全的。 @attention 执行者不拥有给定的任务流。 您有责任确保任务流在执行期间保持活动状态。

    @code{.cpp}
    tf::Future<void> future = executor.run( taskflow, [](){ return rand() % 10 == 0 } );
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
  executor将负责移动任务流的生命周期。这个成员函数是线程安全的。


    @code{.cpp}
    tf::Future<void> future = executor.run(std::move(taskflow), [](){ return rand()%10 == 0 } );
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

     此成员函数多次执行给定的任务流，直到谓词返回  true，然后在执行完成时调用给定的可调用对象。
      该成员函数返回一个 tf::Future 对象，该对象最终持有执行的结果。该成员函数是线程安全的。 
    executor 不拥有给定的任务流。 您有责任确保任务流在执行期间保持活动状态。
 

    @code{.cpp}
    tf::Future<void> future = executor.run( taskflow, [](){ return rand()%10 == 0 }, [](){ std::cout << "done"; }  );
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

     此成员函数多次执行移动的任务流，直到谓词返回  true，然后在执行完成时调用给定的可调用对象。 
     该成员函数返回一个 tf::Future 对象，该对象最终保存执行结果。 执行者将负责移动任务流的生命周期。
     这个成员函数是线程安全的。

    @code{.cpp}
    tf::Future<void> future = executor.run( std::move(taskflow), [](){ return rand()%10 == 0 }, [](){ std::cout << "done"; } );
    // do something else
    future.wait();
    @endcode 
    */
    template<typename P, typename C>
    tf::Future<void> run_until(Taskflow&& taskflow, P&& pred, C&& callable);



    /**
    @brief wait for all tasks to complete
    此成员函数等待所有提交的任务  （例如，任务流、异步任务）完成。

    @code{.cpp}
    executor.run(taskflow1);
    executor.run_n(taskflow2, 10);
    executor.run_n(taskflow3, 100);
    executor.wait_for_all();  // 等到上面提交的 taskflows 完成   
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
    @brief queries the number of running topologies at the time of this call
    将 taskflow 提交给 executor 时，会创建一个拓扑来存储正在运行的任务流的运行时元数据。 
    当提交的 taskflow 执行完成时，其对应的拓扑将从 executor 中删除。

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
   每个 worker 都有一个唯一的 id，在 0 到 N-1 的范围内，与其父执行器相关联。 如果调用者线程不属于 executor，则返回 -1。
    @code{.cpp}
    tf::Executor executor(4);   // 4 workers in the executor
    executor.this_worker_id();  // -1 (main thread is not a worker)

    taskflow.emplace([&](){ std::cout << executor.this_worker_id();  }); // 0, 1, 2, or 3
    executor.run(taskflow);
    @endcode
    */
    int this_worker_id() const;


    /**
    @brief runs a given function asynchronously

    @tparam F callable type
    @tparam ArgsT parameter types

    @param f callable object to call
    @param args parameters to pass to the callable

    @return a tf::Future that will holds the result of the execution

   该方法创建一个异步任务以在给定参数上启动给定函数。 与 std::async 不同，这里的返回是一个 tf::Future，
   它持有一个可选的结果对象。 如果异步任务在运行前被取消，则返回一个  std::nullopt，或可调用对象返回的值。
   这个成员函数是线程安全的。

    @code{.cpp}
    tf::Future<std::optional<int>> future = executor.async([](){
      std::cout << "create an asynchronous task and returns 1\n";
      return 1;
    });
    @endcode
    */
    template <typename F, typename... ArgsT>
    auto async(F&& f, ArgsT&&... args);



    /**
    @brief runs a given function asynchronously and gives a name to this task

    @tparam F callable type
    @tparam ArgsT parameter types

    @param name name of the asynchronous task
    @param f callable object to call
    @param args parameters to pass to the callable

    @return a tf::Future that will holds the result of the execution
    
    该方法创建一个命名的异步任务以在给定参数上启动给定函数。 命名异步任务主要用于分析和可视化任务执行时间线。 
    与 std::async 不同，这里的返回是一个 tf::Future，它持有一个可选的结果对象。 如果异步任务在运行前被取消，则返回一个@c std::nullopt，或可调用对象返回的值。

    @code{.cpp}
    tf::Future<std::optional<int>> future = executor.named_async("name", [](){
      std::cout << "create an asynchronous task with a name and returns 1\n";
      return 1;
    });
    @endcode

    This member function is thread-safe.
    */
    template <typename F, typename... ArgsT>
    auto named_async(const std::string& name, F&& f, ArgsT&&... args);


    /**
    @brief similar to tf::Executor::async but does not return a future object
     
     该成员函数比 tf::Executor::async 更高效，鼓励在没有数据返回时使用。 这个成员函数是线程安全的。

    @code{.cpp}
    executor.silent_async([](){  std::cout << "create an asynchronous task with no return\n";});
    @endcode
    */
    template <typename F, typename... ArgsT>
    void silent_async(F&& f, ArgsT&&... args);

    /**
    @brief similar to tf::Executor::named_async but does not return a future object
    
    该成员函数比 tf::Executor::named_async 更高效，鼓励在没有数据返回时使用。 这个成员函数是线程安全的。

    @code{.cpp}
    executor.named_silent_async("name", [](){ std::cout << "create an asynchronous task with a name and no return\n";});
    @endcode
    */
    template <typename F, typename... ArgsT>
    void named_silent_async(const std::string& name, F&& f, ArgsT&&... args);




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



    /**
    @brief removes an observer from the executor
   此成员函数不是线程安全的。
    */
    template <typename Observer>
    void remove_observer(std::shared_ptr<Observer> observer);


    // 查询观察者数量
    size_t num_observers() const noexcept;

  private:

    std::condition_variable    _topology_cv;
    std::mutex                 _taskflow_mutex;
    std::mutex                 _topology_mutex;
    std::mutex                 _wsq_mutex;

    size_t _num_topologies {0};

    std::unordered_map<std::thread::id, size_t> _wids;
    std::vector<Worker>                         _workers;
    std::vector<std::thread>                    _threads;
    std::list<Taskflow>                         _taskflows;

    Notifier _notifier;

    TaskQueue<Node*> _wsq;

    std::atomic<size_t> _num_actives {0};
    std::atomic<size_t> _num_thieves {0};
    std::atomic<bool>   _done {0};

    std::unordered_set<std::shared_ptr<ObserverInterface>> _observers;

    Worker* _this_worker();

    bool _wait_for_task(Worker&, Node*&);

    void _observer_prologue(Worker&, Node*);
    void _observer_epilogue(Worker&, Node*);
    void _spawn(size_t);
    void _worker_loop(Worker&);
    void _exploit_task(Worker&, Node*&);
    void _explore_task(Worker&, Node*&);
    void _consume_task(Worker&, Node*);
    void _schedule(Worker&, Node*);
    void _schedule(Node*);
    void _schedule(Worker&, const SmallVector<Node*>&);
    void _schedule(const SmallVector<Node*>&);
    void _set_up_topology(Worker*, Topology*);
    void _tear_down_topology(Worker&, Topology*);
    void _tear_down_async(Node*);
    void _tear_down_invoke(Worker&, Node*);
    void _cancel_invoke(Worker&, Node*);
    void _increment_topology();
    void _decrement_topology();
    void _decrement_topology_and_notify();
    void _invoke(Worker&, Node*);
    void _invoke_static_task(Worker&, Node*);
    void _invoke_dynamic_task(Worker&, Node*);
    void _invoke_dynamic_task_external(Worker&, Node*, Graph&, bool);
    void _invoke_dynamic_task_internal(Worker&, Node*, Graph&);
    void _invoke_condition_task(Worker&, Node*, SmallVector<int>&);
    void _invoke_multi_condition_task(Worker&, Node*, SmallVector<int>&);
    void _invoke_module_task(Worker&, Node*, bool&);
    void _invoke_module_task_internal(Worker&, Node*, Graph&, bool&);
    void _invoke_async_task(Worker&, Node*);
    void _invoke_silent_async_task(Worker&, Node*);
    void _invoke_cudaflow_task(Worker&, Node*);
    void _invoke_syclflow_task(Worker&, Node*);
    void _invoke_runtime_task(Worker&, Node*);

    template <typename C, std::enable_if_t<is_cudaflow_task_v<C>, void>* = nullptr >
    void _invoke_cudaflow_task_entry(Node*, C&&);

    template <typename C, typename Q, std::enable_if_t<is_syclflow_task_v<C>, void>* = nullptr  >
    void _invoke_syclflow_task_entry(Node*, C&&, Q&);
};

// Constructor
inline Executor::Executor(size_t N) :
  _workers    {N},
  _notifier   {N} {

  if(N == 0) {
    TF_THROW("no cpu workers to execute taskflows");
  }

  _spawn(N);

  // 如果需要，实例化默认观察者
  if(has_env(TF_ENABLE_PROFILER)) {
    TFProfManager::get()._manage(make_observer<TFProfObserver>());
  }
}

// Destructor
inline Executor::~Executor() {
  // wait for all topologies to complete
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


// Function: named_async
template <typename F, typename... ArgsT>
auto Executor::named_async(const std::string& name, F&& f, ArgsT&&... args) {

  _increment_topology();

  using T = std::invoke_result_t<F, ArgsT...>;
  using R = std::conditional_t<std::is_same_v<T, void>, void, std::optional<T>>;

  std::promise<R> p;

  auto tpg = std::make_shared<AsyncTopology>();

  Future<R> fu(p.get_future(), tpg);

  auto node = node_pool.animate(
    std::in_place_type_t<Node::Async>{},
    [p = make_moc(std::move(p)), f = std::forward<F>(f), args...]
    (bool cancel) mutable {
      if constexpr(std::is_same_v<R, void>) {
        if(!cancel) {
          f(args...);
        }
        p.object.set_value();
      }
      else {
        p.object.set_value(cancel ? std::nullopt : std::make_optional(f(args...)));
      }
    },
    std::move(tpg)
  );

  node->_name = name;

  if(auto w = _this_worker(); w) {
    _schedule(*w, node);
  }
  else{
    _schedule(node);
  }

  return fu;
}


// Function: async
template <typename F, typename... ArgsT>
auto Executor::async(F&& f, ArgsT&&... args) {
  return named_async("", std::forward<F>(f), std::forward<ArgsT>(args)...);
}


// Function: named_silent_async
template <typename F, typename... ArgsT>
void Executor::named_silent_async( const std::string& name, F&& f, ArgsT&&... args) {

  _increment_topology();

  Node* node = node_pool.animate(
    std::in_place_type_t<Node::SilentAsync>{},
    [f = std::forward<F>(f), args...] () mutable {
      f(args...);
    }
  );

  node->_name = name;

  if(auto w = _this_worker(); w) {
    _schedule(*w, node);
  }
  else {
    _schedule(node);
  }
}


// Function: silent_async
template <typename F, typename... ArgsT>
void Executor::silent_async(F&& f, ArgsT&&... args) {
  named_silent_async("", std::forward<F>(f), std::forward<ArgsT>(args)...);
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
  size_t n = 0;

  for(size_t id = 0; id<N; ++id) {

    _workers[id]._id = id;
    _workers[id]._vtm = id;
    _workers[id]._executor = this;
    _workers[id]._waiter = &_notifier._waiters[id];

    _threads.emplace_back([this] (
      Worker& w, std::mutex& mutex, std::condition_variable& cond, size_t& n
    ) -> void {

      // enables the mapping
      {
        std::scoped_lock lock(mutex);
        _wids[std::this_thread::get_id()] = w._id;
        if(n++; n == num_workers()) {
          cond.notify_one();
        }
      } 

      Node* t = nullptr;

      // 必须使用 1 作为条件而不是 !done
      while(1) {
        _exploit_task(w, t);

        if(_wait_for_task(w, t) == false) {
          break;
        }
      }

    }, std::ref(_workers[id]), std::ref(mutex), std::ref(cond), std::ref(n));
  }

  std::unique_lock<std::mutex> lock(mutex);
  cond.wait(lock, [&](){ return n==N; });
}


// Function: _consume_task
inline void Executor::_consume_task(Worker& w, Node* p) {

  std::uniform_int_distribution<size_t> rdvtm(0, _workers.size()-1);

  while(p->_join_counter != 0) {
    exploit:
    if(auto t = w._wsq.pop(); t) {
      _invoke(w, t);
    }
    else {
      size_t num_steals = 0;
      size_t max_steals = ((_workers.size() + 1) << 1);

      explore:

      t = (w._id == w._vtm) ? _wsq.steal() : _workers[w._vtm]._wsq.steal();
      if(t) {
        _invoke(w, t);
        goto exploit;
      }
      else if(p->_join_counter != 0){

        if(num_steals++ > max_steals) {
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
  size_t max_steals = ((_workers.size() + 1) << 1);

  std::uniform_int_distribution<size_t> rdvtm(0, _workers.size()-1);

  do {
    t = (w._id == w._vtm) ? _wsq.steal() : _workers[w._vtm]._wsq.steal();

    if(t) {
      break;
    }

    if(num_steals++ > max_steals) {
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

  if(t) {
    if(_num_actives.fetch_add(1) == 0 && _num_thieves == 0) {
      _notifier.notify(false);
    }

    while(t) {
      _invoke(w, t);
      t = w._wsq.pop();
    }

    --_num_actives;
  }
}

// Function: _wait_for_task
inline bool Executor::_wait_for_task(Worker& worker, Node*& t) {

  wait_for_task:
  //assert(!t);
  ++_num_thieves;

  explore_task:

  _explore_task(worker, t);

  if(t) {
    if(_num_thieves.fetch_sub(1) == 1) {
      _notifier.notify(false);
    }
    return true;
  }

  _notifier.prepare_wait(worker._waiter);
 
  if(!_wsq.empty()) {

    _notifier.cancel_wait(worker._waiter);
    
    t = _wsq.steal();  // must steal here
    if(t) {
      if(_num_thieves.fetch_sub(1) == 1) {
        _notifier.notify(false);
      }
      return true;
    }
    else {
      worker._vtm = worker._id;
      goto explore_task;
    }
  }

  if(_done) {
    _notifier.cancel_wait(worker._waiter);
    _notifier.notify(true);
    --_num_thieves;
    return false;
  }

  if(_num_thieves.fetch_sub(1) == 1) {
    if(_num_actives) {
      _notifier.cancel_wait(worker._waiter);
      goto wait_for_task;
    }
    // check all queues again
    for(auto& w : _workers) {
      if(!w._wsq.empty()) {
        worker._vtm = w._id;
        _notifier.cancel_wait(worker._waiter);
        goto wait_for_task;
      }
    }
  }

  // Now I really need to relinguish my self to others
  _notifier.commit_wait(worker._waiter);

  return true;
}


// Function: make_observer
template<typename Observer, typename... ArgsT>
std::shared_ptr<Observer> Executor::make_observer(ArgsT&&... args) {
  static_assert( std::is_base_of_v<ObserverInterface, Observer>,  "Observer must be derived from ObserverInterface");

  // 使用局部变量来模拟构造函数
  auto ptr = std::make_shared<Observer>(std::forward<ArgsT>(args)...);
  ptr->set_up(_workers.size());
  _observers.emplace(std::static_pointer_cast<ObserverInterface>(ptr));
  return ptr;
}


// Procedure: remove_observer
template <typename Observer>
void Executor::remove_observer(std::shared_ptr<Observer> ptr) {

  static_assert(std::is_base_of_v<ObserverInterface, Observer>,"Observer must be derived from ObserverInterface" );

  _observers.erase(std::static_pointer_cast<ObserverInterface>(ptr));
}

// Function: num_observers
inline size_t Executor::num_observers() const noexcept {
  return _observers.size();
}


// Procedure: _schedule
inline void Executor::_schedule(Worker& worker, Node* node) {

  node->_state.fetch_or(Node::READY, std::memory_order_release);

  // caller is a worker to this pool
  if(worker._executor == this) {
    worker._wsq.push(node);
    return;
  }

  {
    std::lock_guard<std::mutex> lock(_wsq_mutex);
    _wsq.push(node);
  }

  _notifier.notify(false);
}


// Procedure: _schedule
inline void Executor::_schedule(Node* node) {

  node->_state.fetch_or(Node::READY, std::memory_order_release);

  {
    std::lock_guard<std::mutex> lock(_wsq_mutex);
    _wsq.push(node);
  }

  _notifier.notify(false);
}


// Procedure: _schedule
inline void Executor::_schedule(  Worker& worker, const SmallVector<Node*>& nodes) {

  // 我们需要捕获节点计数以避免在删除父拓扑时访问节点向量！  
  const auto num_nodes = nodes.size();

  if(num_nodes == 0) {
    return;
  }

  // make the node ready
  for(size_t i=0; i<num_nodes; ++i) {
    nodes[i]->_state.fetch_or(Node::READY, std::memory_order_release);
  }

  if(worker._executor == this) {
    for(size_t i=0; i<num_nodes; ++i) {
      worker._wsq.push(nodes[i]);
    }
    return;
  }

  {
    std::lock_guard<std::mutex> lock(_wsq_mutex);
    for(size_t k=0; k<num_nodes; ++k) {
      _wsq.push(nodes[k]);
    }
  }

  _notifier.notify_n(num_nodes);
}

// Procedure: _schedule
inline void Executor::_schedule(const SmallVector<Node*>& nodes) {

  // parent topology may be removed!
  const auto num_nodes = nodes.size();

  if(num_nodes == 0) {
    return;
  }

  // make the node ready
  for(size_t i=0; i<num_nodes; ++i) {
    nodes[i]->_state.fetch_or(Node::READY, std::memory_order_release);
  }

  {
    std::lock_guard<std::mutex> lock(_wsq_mutex);
    for(size_t k=0; k<num_nodes; ++k) {
      _wsq.push(nodes[k]);
    }
  }

  _notifier.notify_n(num_nodes);
}


// Procedure: _invoke
inline void Executor::_invoke(Worker& worker, Node* node) {

  int state;
  SmallVector<int> conds;

  // 同步所有由重新排序引起的未完成的内存操作
  do {
    state = node->_state.load(std::memory_order_acquire);
  } while(! (state & Node::READY));

  // 延迟节点的展开堆栈  
  if(state & Node::DEFERRED) {
    node->_state.fetch_and(~Node::DEFERRED, std::memory_order_relaxed);
    goto invoke_epilogue;
  }
 
  invoke_prologue:

  // 如果取消了拓扑，则不需要做其他事情
  if(node->_is_cancelled()) {
    _cancel_invoke(worker, node);
    return;
  }

  // 如果获取信号量存在，首先获取它们
  if(node->_semaphores && !node->_semaphores->to_acquire.empty()) {
    SmallVector<Node*> nodes;
    if(!node->_acquire_all(nodes)) {
      _schedule(worker, nodes);
      return;
    }
    node->_state.fetch_or(Node::ACQUIRED, std::memory_order_release);
  }
 
  // 由于跳转表，switch 比嵌套的 if-else 更快
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
      bool deferred = false;
      _invoke_module_task(worker, node, deferred);
      if(deferred) {
        return;
      }
    }
    break;

    // async task
    case Node::ASYNC: {
      _invoke_async_task(worker, node);
      _tear_down_async(node);
      return ;
    }
    break;

    // silent async task
    case Node::SILENT_ASYNC: {
      _invoke_silent_async_task(worker, node);
      _tear_down_async(node);
      return ;
    }
    break;

    // cudaflow task
    case Node::CUDAFLOW: {
      _invoke_cudaflow_task(worker, node);
    }
    break;

    // syclflow task
    case Node::SYCLFLOW: {
      _invoke_syclflow_task(worker, node);
    }
    break;

    // runtime task
    case Node::RUNTIME: {
      _invoke_runtime_task(worker, node);
    }
    break;

    // monostate (placeholder)
    default:
    break;
  }

  invoke_epilogue:

  // 如果释放信号量存在，则释放它们
  if(node->_semaphores && !node->_semaphores->to_release.empty()) {
    _schedule(worker, node->_release_all());
  }

  // 我们必须恢复依赖关系，因为图形可能有循环。 这必须在安排后继者之前完成，否则这可能会导致 _dependents 出现竞争条件
  if((node->_state.load(std::memory_order_relaxed) & Node::CONDITIONED)) {
    node->_join_counter = node->num_strong_dependents();
  } else {
    node->_join_counter = node->num_dependents();
  }

  // acquire the parent flow counter
  auto& j = (node->_parent) ? node->_parent->_join_counter :  node->_topology->_join_counter;

  Node* cache {nullptr};

  // 此时节点存储可能会被破坏（有待验证）
  // case 1: non-condition task
  switch(node->_handle.index()) {

    // condition and multi-condition tasks
    case Node::CONDITION:
    case Node::MULTI_CONDITION: {
      for(auto cond : conds) {
        if(cond >= 0 && static_cast<size_t>(cond) < node->_successors.size()) {
          auto s = node->_successors[cond];
          // zeroing the join counter for invariant
          s->_join_counter.store(0, std::memory_order_relaxed);
          j.fetch_add(1);
          if(cache) {
            _schedule(worker, cache);
          }
          cache = s;
        }
      }
    }
    break;

    // non-condition task
    default: {
      for(size_t i=0; i<node->_successors.size(); ++i) {
        if(--(node->_successors[i]->_join_counter) == 0) {
          j.fetch_add(1);
          if(cache) {
            _schedule(worker, cache);
          }
          cache = node->_successors[i];
        }
      }
    }
    break;
  }

  // tear_down the invoke
  _tear_down_invoke(worker, node);

  // 对最右边的孩子执行尾递归消除，以减少通过任务队列的昂贵的弹出/压入操作的数量
  if(cache) {
    node = cache;
    goto invoke_prologue;
  }
}

// Procedure: _tear_down_async
inline void Executor::_tear_down_async(Node* node) {
  if(node->_parent) {
    node->_parent->_join_counter.fetch_sub(1);
  }
  else {
    _decrement_topology_and_notify();
  }
  node_pool.recycle(node);
}

// Proecdure: _tear_down_invoke
inline void Executor::_tear_down_invoke(Worker& worker, Node* node) {
  // 我们必须在减去 join counter之前先检查父级，否则它会引入数据竞争
  if(auto parent = node->_parent; parent == nullptr) {
    if(node->_topology->_join_counter.fetch_sub(1) == 1) {
      _tear_down_topology(worker, node->_topology);
    }
  }
  else {
    // prefetch  延迟状态，因为减去   join counter 会立即导致其他工作人员释放子流 
    auto deferred = parent->_state.load(std::memory_order_relaxed) & Node::DEFERRED;
    if(parent->_join_counter.fetch_sub(1) == 1 && deferred) {
      _schedule(worker, parent);
    }
  }
}

// Procedure: _cancel_invoke
inline void Executor::_cancel_invoke(Worker& worker, Node* node) {

  switch(node->_handle.index()) {
    // async task needs to carry out the promise
    case Node::ASYNC:
      std::get_if<Node::Async>(&(node->_handle))->work(true);
      _tear_down_async(node);
    break;

    // silent async doesn't need to carry out the promise
    case Node::SILENT_ASYNC:
      _tear_down_async(node);
    break;

    // tear down topology if the node is the last leaf
    default: {
      _tear_down_invoke(worker, node);
    }
    break;
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
  std::get_if<Node::Static>(&node->_handle)->work();
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
    _invoke_dynamic_task_internal(w, node, handle->subgraph);
  }

  _observer_epilogue(w, node);
}

// Procedure: _invoke_dynamic_task_external
inline void Executor::_invoke_dynamic_task_external( Worker& w, Node* p, Graph& g, bool detach) {

  // graph is empty and has no async tasks
  if(g.empty() && p->_join_counter == 0) {
    return;
  }

  SmallVector<Node*> src;

  for(auto n : g._nodes) {
    n->_topology = p->_topology;
    n->_state.store(0, std::memory_order_relaxed);
    n->_set_up_join_counter();

    if(detach) {
      n->_parent = nullptr;
      n->_state.fetch_or(Node::DETACHED, std::memory_order_relaxed);
    } else {
      n->_parent = p;
    }

    if(n->num_dependents() == 0) {
      src.push_back(n);
    }
  }

  // detach here
  if(detach) {
    {
      std::lock_guard<std::mutex> lock(p->_topology->_taskflow._mutex);
      p->_topology->_taskflow._graph._merge(std::move(g));
    }

    p->_topology->_join_counter.fetch_add(src.size());
    _schedule(w, src);
  }
  // join here
  else {
    p->_join_counter.fetch_add(src.size());
    _schedule(w, src);
    _consume_task(w, p);
  }
}


// Procedure: _invoke_dynamic_task_internal
inline void Executor::_invoke_dynamic_task_internal(  Worker& w, Node* p, Graph& g) {

  // graph is empty and has no async tasks
  if(g.empty() && p->_join_counter == 0) {
    return;
  }

  SmallVector<Node*> src;

  for(auto n : g._nodes) {
    n->_topology = p->_topology;
    n->_state.store(0, std::memory_order_relaxed);
    n->_set_up_join_counter();
    n->_parent = p;
    if(n->num_dependents() == 0) {
      src.push_back(n);
    }
  }
  p->_join_counter.fetch_add(src.size());
  _schedule(w, src);
  _consume_task(w, p);
}


// Procedure: _invoke_module_task_internal
inline void Executor::_invoke_module_task_internal( Worker& w, Node* p, Graph& g, bool& deferred
) {

  // graph is empty and has no async tasks
  if(g.empty()) {
    return;
  }

  // set deferred
  deferred = true;
  p->_state.fetch_or(Node::DEFERRED, std::memory_order_relaxed);

  SmallVector<Node*> src;

  for(auto n : g._nodes) {
    n->_topology = p->_topology;
    n->_state.store(0, std::memory_order_relaxed);
    n->_set_up_join_counter();
    n->_parent = p;
    if(n->num_dependents() == 0) {
      src.push_back(n);
    }
  }
  p->_join_counter.fetch_add(src.size());
  _schedule(w, src);
}


// Procedure: _invoke_condition_task
inline void Executor::_invoke_condition_task( Worker& worker, Node* node, SmallVector<int>& conds) {
  _observer_prologue(worker, node);

  conds = { std::get_if<Node::Condition>(&node->_handle)->work() };

  _observer_epilogue(worker, node);
}

// Procedure: _invoke_multi_condition_task
inline void Executor::_invoke_multi_condition_task( Worker& worker, Node* node, SmallVector<int>& conds) {
  _observer_prologue(worker, node);
 
  conds = std::get_if<Node::MultiCondition>(&node->_handle)->work();
  
  _observer_epilogue(worker, node);
}


// Procedure: _invoke_cudaflow_task
inline void Executor::_invoke_cudaflow_task(Worker& worker, Node* node) {
  _observer_prologue(worker, node);
 
  std::get_if<Node::cudaFlow>(&node->_handle)->work(*this, node);

  _observer_epilogue(worker, node);
}


// Procedure: _invoke_syclflow_task
inline void Executor::_invoke_syclflow_task(Worker& worker, Node* node) {
  _observer_prologue(worker, node);
 
  std::get_if<Node::syclFlow>(&node->_handle)->work(*this, node);
 
  _observer_epilogue(worker, node);
}

// Procedure: _invoke_module_task
inline void Executor::_invoke_module_task(Worker& w, Node* node, bool& deferred) {
  _observer_prologue(w, node);

  _invoke_module_task_internal(  w, node, std::get_if<Node::Module>(&node->_handle)->graph, deferred );
 
  _observer_epilogue(w, node);
}

// Procedure: _invoke_async_task
inline void Executor::_invoke_async_task(Worker& w, Node* node) {
  _observer_prologue(w, node);
 
  std::get_if<Node::Async>(&node->_handle)->work(false);
 
  _observer_epilogue(w, node);
}

// Procedure: _invoke_silent_async_task
inline void Executor::_invoke_silent_async_task(Worker& w, Node* node) {
  _observer_prologue(w, node);

  std::get_if<Node::SilentAsync>(&node->_handle)->work();
 
  _observer_epilogue(w, node);
}

// Procedure: _invoke_runtime_task
inline void Executor::_invoke_runtime_task(Worker& w, Node* node) {
  _observer_prologue(w, node);
 
  Runtime rt(*this, w, node);
 
  std::get_if<Node::Runtime>(&node->_handle)->work(rt);
  
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
  return run_until( f, [repeat]() mutable { return repeat-- == 0; }, std::forward<C>(c) );
}

// Function: run_n
template <typename C>
tf::Future<void> Executor::run_n(Taskflow&& f, size_t repeat, C&& c) {
  return run_until(  std::move(f), [repeat]() mutable { return repeat-- == 0; }, std::forward<C>(c) );
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

  // 需要检查锁下的空，因为  dynamic task  可能定义同时修改 taskflow 的 detached blocks
  bool empty;
  {
    std::lock_guard<std::mutex> lock(f._mutex);
    empty = f.empty();
  }

  //不需要创建一个真正的 topology 但返回一个  dummy future
  if(empty || p()) {
    c();
    std::promise<void> promise;
    promise.set_value();
    _decrement_topology_and_notify();
    return tf::Future<void>(promise.get_future(), std::monostate{});
  }

  // create a topology for this run
  auto t = std::make_shared<Topology>(f, std::forward<P>(p), std::forward<C>(c));

  // need to create future before the topology got torn down quickly
  tf::Future<void> future(t->_promise.get_future(), t);

  // 修改 topology 需要加锁保护   
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
    std::scoped_lock<std::mutex> lock(_taskflow_mutex);
    itr = _taskflows.emplace(_taskflows.end(), std::move(f));
    itr->_satellite = itr;
  }

  return run_until(*itr, std::forward<P>(pred), std::forward<C>(c));
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
    node->_state.store(0, std::memory_order_relaxed);

    if(node->num_dependents() == 0) {
      tpg->_sources.push_back(node);
    }

    node->_set_up_join_counter();
  }

  tpg->_join_counter = tpg->_sources.size();

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

  // case 1: 我们仍然需要再次运行拓扑
  if(!tpg->_is_cancelled && !tpg->_pred()) {
    //assert(tpg->_join_counter == 0);
    std::lock_guard<std::mutex> lock(f._mutex);
    tpg->_join_counter = tpg->_sources.size();
    _schedule(worker, tpg->_sources);
  }
  // case 2:   the final run of this topology
  else {

    // TODO: 如果拓扑被取消，需要释放所有信号量

    if(tpg->_call != nullptr) {
      tpg->_call();
    }

    // 如果有另一个 run（锁之间交错）   
    if(std::unique_lock<std::mutex> lock(f._mutex); f._topologies.size()>1) {
      //assert(tpg->_join_counter == 0);

      // Set the promise
      tpg->_promise.set_value();
      f._topologies.pop();
      tpg = f._topologies.front().get();

      // decrement the topology but since this is not the last we don't notify
      _decrement_topology();

      // 设置 topology 需要在锁下，否则它会通过 pop 引入内存顺序错误   
      _set_up_topology(&worker, tpg);
    }
    else {
      //assert(f._topologies.size() == 1);

      // 需要先在这里备份 promise ，因为任务流可能会在调用 get 后很快被破坏    
      auto p {std::move(tpg->_promise)};

      // 备份 lambda 捕获以防它具有拓扑指针，以避免它在 _mutex.unlock 和 _promise.set_value 之前在 pop_front 上释放。 离开作用域时安全释放。 
      auto c {std::move(tpg->_call)};

      // Get the satellite if any
      auto s {f._satellite};

      // 现在我们从此  taskflow 中删除 topology  
      f._topologies.pop();

      //f._mutex.unlock();
      lock.unlock();

      // 我们最终设定了 promise ，以防 taskflow 离开范围。 set_value 之后，调用者将从 wait 返回
      p.set_value();

      _decrement_topology_and_notify();

      // 如果 taskflow 由 executor 管理，则移除 taskflow
      // TODO：将来，我们可能需要在等待时同步（这意味着以下代码应该在 set_value 之前移动） 
      if(s) {
        std::scoped_lock<std::mutex> lock(_taskflow_mutex);
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

  // 只有 parent worker 可以加入 subflow
  _executor._invoke_dynamic_task_external(_worker, _parent, _graph, false);
  _joinable = false;
}


inline void Subflow::detach() {

  // assert(this_worker().worker == &_worker);

  if(!_joinable) {
    TF_THROW("subflow already joined or detached");
  }

  //  只有 parent worker 可以分离  subflow
  _executor._invoke_dynamic_task_external(_worker, _parent, _graph, true);
  _joinable = false;
}

// Function: named_async
template <typename F, typename... ArgsT>
auto Subflow::named_async(const std::string& name, F&& f, ArgsT&&... args) {
  return _named_async( *_executor._this_worker(), name, std::forward<F>(f), std::forward<ArgsT>(args)... );
}

// Function: _named_async
template <typename F, typename... ArgsT>
auto Subflow::_named_async(
  Worker& w,
  const std::string& name,
  F&& f,
  ArgsT&&... args
) {

  _parent->_join_counter.fetch_add(1);

  using T = std::invoke_result_t<F, ArgsT...>;
  using R = std::conditional_t<std::is_same_v<T, void>, void, std::optional<T>>;

  std::promise<R> p;

  auto tpg = std::make_shared<AsyncTopology>();

  Future<R> fu(p.get_future(), tpg);

  auto node = node_pool.animate(
    std::in_place_type_t<Node::Async>{},
    [p=make_moc(std::move(p)), f=std::forward<F>(f), args...]
    (bool cancel) mutable {
      if constexpr(std::is_same_v<R, void>) {
        if(!cancel) {
          f(args...);
        }
        p.object.set_value();
      }
      else {
        p.object.set_value(cancel ? std::nullopt : std::make_optional(f(args...)));
      }
    },
    std::move(tpg)
  );

  node->_name = name;
  node->_topology = _parent->_topology;
  node->_parent = _parent;

  _executor._schedule(w, node);

  return fu;
}

// Function: async
template <typename F, typename... ArgsT>
auto Subflow::async(F&& f, ArgsT&&... args) {
  return named_async("", std::forward<F>(f), std::forward<ArgsT>(args)...);
}

// Function: _named_silent_async
template <typename F, typename... ArgsT>
void Subflow::_named_silent_async( Worker& w, const std::string& name, F&& f, ArgsT&&... args) {

  _parent->_join_counter.fetch_add(1);

  auto node = node_pool.animate(
    std::in_place_type_t<Node::SilentAsync>{},
    [f = std::forward<F>(f), args...] () mutable {
      f(args...);
    }
  );

  node->_name = name;
  node->_topology = _parent->_topology;
  node->_parent = _parent;

  _executor._schedule(w, node);
}

// Function: silent_async
template <typename F, typename... ArgsT>
void Subflow::named_silent_async(const std::string& name, F&& f, ArgsT&&... args) {
  _named_silent_async( *_executor._this_worker(), name, std::forward<F>(f), std::forward<ArgsT>(args)... );
}

// Function: named_silent_async
template <typename F, typename... ArgsT>
void Subflow::silent_async(F&& f, ArgsT&&... args) {
  named_silent_async("", std::forward<F>(f), std::forward<ArgsT>(args)...);
}




// ############################################################################
// Forward Declaration: Runtime
// ############################################################################

// Procedure: schedule
inline void Runtime::schedule(Task task) {
  auto node = task._node;
  auto& j = node->_parent ? node->_parent->_join_counter :   node->_topology->_join_counter;
  j.fetch_add(1);
  _executor._schedule(_worker, node);
}

// Procedure: run
template <typename C>
void Runtime::run(C&& callable) {

  // dynamic task (subflow)
  if constexpr(is_dynamic_task_v<C>) {
    Graph graph;
    Subflow sf(_executor, _worker, _parent, graph);
    callable(sf);
    if(sf._joinable) {
      _executor._invoke_dynamic_task_internal(_worker, _parent, graph);
    }
  }
  else {
    static_assert(dependent_false_v<C>, "unsupported task callable to run");
  }
}

}  // end of namespace tf -----------------------------------------------------








