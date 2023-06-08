#pragma once

#include <iostream>
#include <vector>
#include <cstdlib>
#include <cstdio>
#include <atomic>
#include <memory>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <algorithm>
#include <numeric>
#include <cassert>

// 此文件是 Eigen 的一部分，Eigen 是用于线性代数的轻量级 C++ 模板库。
// Copyright (C) 2016 Dmitry Vyukov <dvyukov@google.com>

namespace tf {

//  Notifier 允许等待非阻塞算法中的任意谓词。 考虑条件变量，但等待谓词不需要由互斥体保护
// 
//  ### Waiting thread does: ###
//
//   if (predicate)
//     return act();
//   Notifier::Waiter& w = waiters[my_index];
//   ec.prepare_wait(&w);
//   if (predicate) {
//     ec.cancel_wait(&w);
//     return act();
//   }
//   ec.commit_wait(&w);
//
// ### Notifying thread does: ###
//
//   predicate = true;
//   ec.notify(true);
// 
// 如果没有等待线程，notify 是便宜的。 prepare_wait/commit_wait 并不便宜，但只有在前面的谓词检查失败时才会执行它们。
//
// Algorihtm outline:
// 主要有两个变量：predicate（由用户管理）和_state。 操作非常类似于 Dekker 相互算法：https://en.wikipedia.org/wiki/Dekker%27s_algorithm
// Waiting thread 设置 _state 然后检查谓词，通知线程设置谓词然后检查 _state。 由于这些操作之间的 seq_cst 栅栏，可以保证 waiter  将看到谓词更改并且不会阻塞，
// 或者 notifying thread 将看到 _state 更改并将解除 waiter 的阻塞，或两者兼而有之。 但是不可能两个线程都看不到对方的变化，这会导致死锁。
class Notifier {

  friend class Executor;

  public:

  struct Waiter {
    std::atomic<Waiter*> next;
    std::mutex mu;
    std::condition_variable cv;
    uint64_t epoch;
    unsigned state;
    enum {
      kNotSignaled,
      kWaiting,
      kSignaled,
    };
  };

  explicit Notifier(size_t N) : _waiters{N} {
    assert(_waiters.size() < (1 << kWaiterBits) - 1);
    // 将 epoch 初始化为接近溢出的值以测试溢出 
    _state = kStackMask | (kEpochMask - kEpochInc * _waiters.size() * 2);
  }

  ~Notifier() {
    // 确保没有  waiters.
    assert((_state.load() & (kStackMask | kWaiterMask)) == kStackMask);
  }

  // prepare_wait 准备等待。 调用此函数后，线程必须重新检查等待谓词并调用传递相同 Waiter 对象的 cancel_wait 或 commit_wait。
  void prepare_wait(Waiter* w) {
    w->epoch = _state.fetch_add(kWaiterInc, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_seq_cst);
  }

  // commit_wait 提交等待
  void commit_wait(Waiter* w) {
    w->state = Waiter::kNotSignaled;
    // Modification epoch of this waiter.
    uint64_t epoch =  (w->epoch & kEpochMask) +  (((w->epoch & kWaiterMask) >> kWaiterShift) << kEpochShift);
    uint64_t state = _state.load(std::memory_order_seq_cst);
    for (;;) {
      if (int64_t((state & kEpochMask) - epoch) < 0) {
        // 前面的 waiter 还没有决定它的命运。 等到它调用 cancel_wait 或 commit_wait，或收到通知。  
        std::this_thread::yield();
        state = _state.load(std::memory_order_seq_cst);
        continue;
      }
      // 我们已经收到通知。
      if (int64_t((state & kEpochMask) - epoch) > 0) return;
      // 从 prewait counter 中删除此线程并将其添加到等待列表中 
      assert((state & kWaiterMask) != 0);
      uint64_t newstate = state - kWaiterInc + kEpochInc;
      newstate = static_cast<uint64_t>((newstate & ~kStackMask) | static_cast<uint64_t>(w - &_waiters[0]));
      if ((state & kStackMask) == kStackMask) {
             w->next.store(nullptr, std::memory_order_relaxed);
      } 
      else{
             w->next.store(&_waiters[state & kStackMask], std::memory_order_relaxed);
      }
     
      if (_state.compare_exchange_weak(state, newstate,  std::memory_order_release))
        break;
    }
    _park(w);
  }


  // cancel_wait 取消先前 prepare_wait 调用的效果
  void cancel_wait(Waiter* w) {
    uint64_t epoch =  (w->epoch & kEpochMask) +   (((w->epoch & kWaiterMask) >> kWaiterShift) << kEpochShift);
    uint64_t state = _state.load(std::memory_order_relaxed);
    for (;;) {
      if (int64_t((state & kEpochMask) - epoch) < 0) {
        // 前面的 waiter 还没有决定它的命运。 等到它调用 cancel_wait 或 commit_wait，或收到通知
        std::this_thread::yield();
        state = _state.load(std::memory_order_relaxed);
        continue;
      }
      // 我们已经收到通知 
      if (int64_t((state & kEpochMask) - epoch) > 0) return;
      // 从 prewait counter 中删除此线程 
      assert((state & kWaiterMask) != 0);
      if (_state.compare_exchange_weak(state, state - kWaiterInc + kEpochInc,  std::memory_order_relaxed))
        return;
    }
  }


  // 通知唤醒一个或所有等待线程。 必须在更改关联的等待谓词后调用   
  void notify(bool all) {
    std::atomic_thread_fence(std::memory_order_seq_cst);
    uint64_t state = _state.load(std::memory_order_acquire);
    for (;;) {
      // 简单的情况：没有waiters.
      if ((state & kStackMask) == kStackMask && (state & kWaiterMask) == 0)
        return;

      uint64_t waiters = (state & kWaiterMask) >> kWaiterShift;
      uint64_t newstate;
      if (all) {
        // 重置  prewait counter  并清空等待列表 
        newstate = (state & kEpochMask) + (kEpochInc * waiters) + kStackMask;
      } else if (waiters) {
        // 有一个线程处于 pre-wait 状态，  unblock it.
        newstate = state + kEpochInc - kWaiterInc;
      } else {
        // 从列表中弹出一个 waiter 并  unpark it.
        Waiter* w = &_waiters[state & kStackMask];
        Waiter* wnext = w->next.load(std::memory_order_relaxed);
        uint64_t next = kStackMask;

        if (wnext != nullptr) next = static_cast<uint64_t>(wnext - &_waiters[0]);
        // 注意：我们不在这里添加 kEpochInc。 无锁栈上的 ABA 问题不会发生，因为等待者只有在处于预等待状态后才会被重新压入栈中，这不可避免地导致 epoch 递增。
        newstate = (state & kEpochMask) + next;
      }
      if (_state.compare_exchange_weak(state, newstate,   std::memory_order_acquire)) {
        if (!all && waiters) return;  // unblocked pre-wait thread
        if ((state & kStackMask) == kStackMask) return;
        Waiter* w = &_waiters[state & kStackMask];
        if (!all) w->next.store(nullptr, std::memory_order_relaxed);
        _unpark(w);
        return;
      }
    }
  }

  // notify n workers
  void notify_n(size_t n) {
    if(n >= _waiters.size()) {
      notify(true);
    }
    else {
      for(size_t k=0; k<n; ++k) {
        notify(false);
      }
    }
  }

  size_t size() const {
    return _waiters.size();
  }

 private:

  // State_ layout:
  // - low kStackBits   : waiters committed wait  的 stack  
  // - next kWaiterBits : 处于 prewait 状态的 waiters 计数 
  // - next kEpochBits  : modification counter.
  static const uint64_t kStackBits = 16;
  static const uint64_t kStackMask = (1ull << kStackBits) - 1;
  static const uint64_t kWaiterBits = 16;
  static const uint64_t kWaiterShift = 16;
  static const uint64_t kWaiterMask = ((1ull << kWaiterBits) - 1)
                                      << kWaiterShift;
  static const uint64_t kWaiterInc = 1ull << kWaiterBits;
  static const uint64_t kEpochBits = 32;
  static const uint64_t kEpochShift = 32;
  static const uint64_t kEpochMask = ((1ull << kEpochBits) - 1) << kEpochShift;
  static const uint64_t kEpochInc = 1ull << kEpochShift;

  std::atomic<uint64_t> _state;
  std::vector<Waiter>  _waiters;

  void _park(Waiter* w) {
    std::unique_lock<std::mutex> lock(w->mu);
    while (w->state != Waiter::kSignaled) {
      w->state = Waiter::kWaiting;
      w->cv.wait(lock);
    }
  }

  void _unpark(Waiter* waiters) {
    Waiter* next = nullptr;
    for (Waiter* w = waiters; w; w = next) {
      next = w->next.load(std::memory_order_relaxed);
      unsigned state;
      {
        std::unique_lock<std::mutex> lock(w->mu);
        state = w->state;
        w->state = Waiter::kSignaled;
      }
      // 如果没有等待，则避免通知 
      if (state == Waiter::kWaiting) w->cv.notify_one();
    }
  }

};



}  // namespace tf ------------------------------------------------------------

