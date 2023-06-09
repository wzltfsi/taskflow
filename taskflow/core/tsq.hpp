#pragma once

#include "../utility/macros.hpp"
#include "../utility/traits.hpp"

/**
@file tsq.hpp
@brief task queue include file
*/

namespace tf {


// ----------------------------------------------------------------------------
// Task Types
// ----------------------------------------------------------------------------

/**
@enum TaskPriority  枚举所有任务优先级值
优先级是@c unsigned 类型的枚举值。 目前，%Taskflow定义了@c HIGH、@c NORMAL、@c LOW三个优先级，从0、1到2，即数值越低，优先级越高。
*/
enum class TaskPriority : unsigned {
  HIGH = 0,
  NORMAL = 1,
  LOW = 2,
  MAX = 3
};



// ----------------------------------------------------------------------------
// Task Queue
// ----------------------------------------------------------------------------


/**
@class: TaskQueue
@brief class 创建无锁无界单生产者多消费者队列（lock-free unbounded single-producer multiple-consumer queue）

This class implements the work-stealing queue described in the paper,
<a href="https://www.di.ens.fr/~zappa/readings/ppopp13.pdf">Correct and Efficient Work-Stealing for Weak Memory Models</a>,
and extends it to include priority.

只有队列所有者可以执行弹出和推送操作，而其他人可以同时从队列中窃取数据。 
优先级从零（最高优先级）到模板值“TF_MAX_PRIORITY-1”（最低优先级）。 
所有操作都与优先级值相关联，以指示应用操作的相应队列。 默认模板值“TF_MAX_PRIORITY”是“TaskPriority::MAX”，它仅将三个优先级应用于任务队列。

@code{.cpp}
auto [A, B, C, D, E] = taskflow.emplace(
  [] () { },
  [&] () { 
    std::cout << "Task B: " << counter++ << '\n';  // 0
  },
  [&] () { 
    std::cout << "Task C: " << counter++ << '\n';  // 2
  },
  [&] () { 
    std::cout << "Task D: " << counter++ << '\n';  // 1
  },
  [] () { }
);

A.precede(B, C, D); 
E.succeed(B, C, D);
  
B.priority(tf::TaskPriority::HIGH);
C.priority(tf::TaskPriority::LOW);
D.priority(tf::TaskPriority::NORMAL);
  
executor.run(taskflow).wait();
@endcode

在上面的例子中，我们有五个任务的任务图，@c A、@c B、@c C、@c D和@c E，其中@c B、@c C和@c D可以 @c A 完成时同时运行。
由于我们在执行器中只使用一个工作线程，我们可以确定地首先运行@c B，然后是@c D，然后@c C 按照它们的优先级值的顺序运行。 输出如下：

@code{.shell-session}
Task B: 0
Task D: 1
Task C: 2
@endcode

*/
template <typename T, unsigned TF_MAX_PRIORITY = static_cast<unsigned>(TaskPriority::MAX)>
class TaskQueue {
  
  static_assert(TF_MAX_PRIORITY > 0, "TF_MAX_PRIORITY must be at least one");
  static_assert(std::is_pointer_v<T>, "T must be a pointer type");

  struct Array {

    int64_t C;
    int64_t M;
    std::atomic<T>* S;

    explicit Array(int64_t c) :
      C {c},
      M {c-1},
      S {new std::atomic<T>[static_cast<size_t>(C)]} {
    }

    ~Array() {
      delete [] S;
    }

    int64_t capacity() const noexcept {
      return C;
    }

    void push(int64_t i, T o) noexcept {
      S[i & M].store(o, std::memory_order_relaxed);
    }

    T pop(int64_t i) noexcept {
      return S[i & M].load(std::memory_order_relaxed);
    }

    Array* resize(int64_t b, int64_t t) {
      Array* ptr = new Array {2*C};
      for(int64_t i=t; i!=b; ++i) {
        ptr->push(i, pop(i));
      }
      return ptr;
    }

  };

  // 将对齐方式加倍 2 似乎会产生最不错的性能。
  CachelineAligned<std::atomic<int64_t>> _top[TF_MAX_PRIORITY];
  CachelineAligned<std::atomic<int64_t>> _bottom[TF_MAX_PRIORITY];
  std::atomic<Array*> _array[TF_MAX_PRIORITY];
  std::vector<Array*> _garbage[TF_MAX_PRIORITY];

  //std::atomic<T> _cache {nullptr};

  public:
    // 构造具有给定容量的队列 ， capacity 队列的容量（必须是2的幂）
    explicit TaskQueue(int64_t capacity = 512);

    ~TaskQueue();

    bool empty() const noexcept;

    bool empty(unsigned priority) const noexcept;


    size_t size() const noexcept;

    // 查询调用时具有给定优先级的项目数
    size_t size(unsigned priority) const noexcept;

    // 查询队列容量
    int64_t capacity() const noexcept;
    
    // 查询特定优先级值的队列容量
    int64_t capacity(unsigned priority) const noexcept;

    TF_FORCE_INLINE void push(T item, unsigned priority);

    T pop();

    TF_FORCE_INLINE T pop(unsigned priority);


    //   从队列中窃取一个项目 任何线程都可以尝试从队列中窃取一个项目。 如果此操作失败（不一定为空），则返回可以是 @c nullptr。
    T steal();

    // 从队列中窃取具有特定优先级值的项目 ,  priority 要窃取的项目的优先级 ,  任何线程都可以尝试从队列中窃取项目。 如果此操作失败（不一定为空），则返回可以是  nullptr。
    T steal(unsigned priority);

  private:
    TF_NO_INLINE Array* resize_array(Array* a, unsigned p, std::int64_t b, std::int64_t t);
};

// Constructor
template <typename T, unsigned TF_MAX_PRIORITY>
TaskQueue<T, TF_MAX_PRIORITY>::TaskQueue(int64_t c) {
  assert(c && (!(c & (c-1))));
  unroll<0, TF_MAX_PRIORITY, 1>([&](auto p){
    _top[p].data.store(0, std::memory_order_relaxed);
    _bottom[p].data.store(0, std::memory_order_relaxed);
    _array[p].store(new Array{c}, std::memory_order_relaxed);
    _garbage[p].reserve(32);
  });
}

// Destructor
template <typename T, unsigned TF_MAX_PRIORITY>
TaskQueue<T, TF_MAX_PRIORITY>::~TaskQueue() {
  unroll<0, TF_MAX_PRIORITY, 1>([&](auto p){
    for(auto a : _garbage[p]) {
      delete a;
    }
    delete _array[p].load();
  });
}

// Function: empty
template <typename T, unsigned TF_MAX_PRIORITY>
bool TaskQueue<T, TF_MAX_PRIORITY>::empty() const noexcept {
  for(unsigned i=0; i<TF_MAX_PRIORITY; i++) {
    if(!empty(i)) {
      return false;
    }
  }
  return true;
}

// Function: empty
template <typename T, unsigned TF_MAX_PRIORITY>
bool TaskQueue<T, TF_MAX_PRIORITY>::empty(unsigned p) const noexcept {
  int64_t b = _bottom[p].data.load(std::memory_order_relaxed);
  int64_t t = _top[p].data.load(std::memory_order_relaxed);
  return (b <= t);
}

// Function: size
template <typename T, unsigned TF_MAX_PRIORITY>
size_t TaskQueue<T, TF_MAX_PRIORITY>::size() const noexcept {
  size_t s;
  unroll<0, TF_MAX_PRIORITY, 1>([&](auto i) { s = i ? size(i) + s : size(i); });
  return s;
}

// Function: size
template <typename T, unsigned TF_MAX_PRIORITY>
size_t TaskQueue<T, TF_MAX_PRIORITY>::size(unsigned p) const noexcept {
  int64_t b = _bottom[p].data.load(std::memory_order_relaxed);
  int64_t t = _top[p].data.load(std::memory_order_relaxed);
  return static_cast<size_t>(b >= t ? b - t : 0);
}

// Function: push
template <typename T, unsigned TF_MAX_PRIORITY>
TF_FORCE_INLINE void TaskQueue<T, TF_MAX_PRIORITY>::push(T o, unsigned p) {

  int64_t b = _bottom[p].data.load(std::memory_order_relaxed);
  int64_t t = _top[p].data.load(std::memory_order_acquire);
  Array* a = _array[p].load(std::memory_order_relaxed);

  // queue is full
  if(a->capacity() - 1 < (b - t)) {
    a = resize_array(a, p, b, t);
  }

  a->push(b, o);
  std::atomic_thread_fence(std::memory_order_release);
  _bottom[p].data.store(b + 1, std::memory_order_relaxed);
}

// Function: pop
template <typename T, unsigned TF_MAX_PRIORITY>
T TaskQueue<T, TF_MAX_PRIORITY>::pop() {
  for(unsigned i=0; i<TF_MAX_PRIORITY; i++) {
    if(auto t = pop(i); t) {
      return t;
    }
  }
  return nullptr;
}

// Function: pop
template <typename T, unsigned TF_MAX_PRIORITY>
TF_FORCE_INLINE T TaskQueue<T, TF_MAX_PRIORITY>::pop(unsigned p) {

  int64_t b = _bottom[p].data.load(std::memory_order_relaxed) - 1;
  Array*  a = _array[p].load(std::memory_order_relaxed);
  _bottom[p].data.store(b, std::memory_order_relaxed);
  std::atomic_thread_fence(std::memory_order_seq_cst);
  int64_t t = _top[p].data.load(std::memory_order_relaxed);

  T item {nullptr};

  if(t <= b) {
    item = a->pop(b);
    if(t == b) {
      // the last item just got stolen
      if(!_top[p].data.compare_exchange_strong(t, t+1, std::memory_order_seq_cst, std::memory_order_relaxed)) {
        item = nullptr;
      }
      _bottom[p].data.store(b + 1, std::memory_order_relaxed);
    }
  }
  else {
    _bottom[p].data.store(b + 1, std::memory_order_relaxed);
  }

  return item;
}

// Function: steal
template <typename T, unsigned TF_MAX_PRIORITY>
T TaskQueue<T, TF_MAX_PRIORITY>::steal() {
  for(unsigned i=0; i<TF_MAX_PRIORITY; i++) {
    if(auto t = steal(i); t) {
      return t;
    }
  }
  return nullptr;
}

// Function: steal
template <typename T, unsigned TF_MAX_PRIORITY>
T TaskQueue<T, TF_MAX_PRIORITY>::steal(unsigned p) {
  
  int64_t t = _top[p].data.load(std::memory_order_acquire);
  std::atomic_thread_fence(std::memory_order_seq_cst);
  int64_t b = _bottom[p].data.load(std::memory_order_acquire);

  T item {nullptr};

  if(t < b) {
    Array* a = _array[p].load(std::memory_order_consume);
    item = a->pop(t);
    if(!_top[p].data.compare_exchange_strong(t, t+1, std::memory_order_seq_cst, std::memory_order_relaxed)) {
      return nullptr;
    }
  }

  return item;
}

// Function: capacity
template <typename T, unsigned TF_MAX_PRIORITY>
int64_t TaskQueue<T, TF_MAX_PRIORITY>::capacity() const noexcept {
  size_t s;
  unroll<0, TF_MAX_PRIORITY, 1>([&](auto i) { 
    s = i ? capacity(i) + s : capacity(i); 
  });
  return s;
}

// Function: capacity
template <typename T, unsigned TF_MAX_PRIORITY>
int64_t TaskQueue<T, TF_MAX_PRIORITY>::capacity(unsigned p) const noexcept {
  return _array[p].load(std::memory_order_relaxed)->capacity();
}

template <typename T, unsigned TF_MAX_PRIORITY>
TF_NO_INLINE typename TaskQueue<T, TF_MAX_PRIORITY>::Array*
  TaskQueue<T, TF_MAX_PRIORITY>::resize_array(Array* a, unsigned p, std::int64_t b, std::int64_t t) {

  Array* tmp = a->resize(b, t);
  _garbage[p].push_back(a);
  std::swap(a, tmp);
  _array[p].store(a, std::memory_order_release);
  
  return a;
}


}  // end of namespace tf -----------------------------------------------------
