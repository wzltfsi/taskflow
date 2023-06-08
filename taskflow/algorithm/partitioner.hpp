// reference:
// - gomp: https://github.com/gcc-mirror/gcc/blob/master/libgomp/iter.c
// - komp: https://github.com/llvm-mirror/openmp/blob/master/runtime/src/kmp_dispatch.cpp

#pragma once

/**
@file partitioner.hpp
@brief partitioner include file
*/

namespace tf {

// ----------------------------------------------------------------------------
// Partitioner Base
// ----------------------------------------------------------------------------

/**
@class PartitionerBase

@brief class to derive a partitioner for scheduling parallel algorithms

该类提供基本方法来派生可用于安排并行迭代的分区器（例如，tf::Taskflow::for_each）。 
分区器定义了运行并行算法的调度方法，例如 tf::Taskflow::for_each、tf::Taskflow::reduce 等。
 默认情况下，我们提供以下分区程序：

+ tf::GuidedPartitioner 启用自适应块大小的引导调度算法
+ tf::DynamicPartitioner 启用等块大小的动态调度算法
+ tf::StaticPartitioner 启用静态块大小的静态调度算法
+ tf::RandomPartitioner 启用随机块大小的随机调度算法

根据应用程序的不同，分区算法会对性能产生很大影响。 
例如，如果并行迭代工作负载每次迭代包含一个常规工作单元，则 tf::StaticPartitioner 可以提供最佳性能。 
另一方面，如果每次迭代的工作单元不规则且不平衡，则 tf::GuidedPartitioner 或 tf::DynamicPartitioner 的性能可能优于 tf::StaticPartitioner。 
在大多数情况下，tf::GuidedPartitioner 可以提供不错的性能，因此被用作我们的默认分区程序。
*/
class PartitionerBase {

  public:

  // default constructor
  PartitionerBase() = default;

  // 构造一个具有给定块大小的分区器
  explicit PartitionerBase(size_t chunk_size) : _chunk_size {chunk_size} {}

  // 查询此分区程序的块大小
  size_t chunk_size() const { return _chunk_size; }
  
  // 更新此分区程序的块大小
  void chunk_size(size_t cz) { _chunk_size = cz; }

  protected:
   
  size_t _chunk_size{0};
};

// ----------------------------------------------------------------------------
// Guided Partitioner
// ----------------------------------------------------------------------------
  
/**
@class GuidedPartitioner

@brief class to construct a guided partitioner for scheduling parallel algorithms
 
The size of a partition is proportional to the number of unassigned iterations  divided by the number of workers, 
and the size will gradually decrease to the given chunk size. The last partition may be smaller than the chunk size.
*/
class GuidedPartitioner : public PartitionerBase {

  public:
 
  GuidedPartitioner() : PartitionerBase{1} {}
 
  explicit GuidedPartitioner(size_t sz) : PartitionerBase (sz) {}
  
  // --------------------------------------------------------------------------
  // 调度方法
  // --------------------------------------------------------------------------
  
  // @private
  template <typename F, std::enable_if_t<std::is_invocable_r_v<void, F, size_t, size_t>, void>* = nullptr>
  void loop(
    size_t N, 
    size_t W, 
    std::atomic<size_t>& next, 
    F&& func
  ) const {

    size_t chunk_size = (_chunk_size == 0) ? size_t{1} : _chunk_size;

    size_t p1 = 2 * W * (chunk_size + 1);
    float  p2 = 0.5f / static_cast<float>(W);
    size_t curr_b = next.load(std::memory_order_relaxed);

    while(curr_b < N) {

      size_t r = N - curr_b;

      // 细粒度
      if(r < p1) {
        while(1) {
          curr_b = next.fetch_add(chunk_size, std::memory_order_relaxed);
          if(curr_b >= N) {
            return;
          }
          func(curr_b, std::min(curr_b + chunk_size, N));
        }
        break;
      }
      // 粗粒度
      else {
        size_t q = static_cast<size_t>(p2 * r);
        if(q < chunk_size) {
          q = chunk_size;
        }
        //size_t curr_e = (q <= r) ? curr_b + q : N;
        size_t curr_e = std::min(curr_b + q, N);
        if(next.compare_exchange_strong(curr_b, curr_e, std::memory_order_relaxed,  std::memory_order_relaxed)) {
          func(curr_b, curr_e);
          curr_b = next.load(std::memory_order_relaxed);
        }
      }
    }
  }
  

  // private
  template <typename F,   std::enable_if_t<std::is_invocable_r_v<bool, F, size_t, size_t>, void>* = nullptr>
  void loop_until(
    size_t N, 
    size_t W, 
    std::atomic<size_t>& next, 
    F&& func
  ) const {

    size_t chunk_size = (_chunk_size == 0) ? size_t{1} : _chunk_size;

    size_t p1 = 2 * W * (chunk_size + 1);
    float  p2 = 0.5f / static_cast<float>(W);
    size_t curr_b = next.load(std::memory_order_relaxed);

    while(curr_b < N) {

      size_t r = N - curr_b;

      // fine-grained
      if(r < p1) {
        while(1) {
          curr_b = next.fetch_add(chunk_size, std::memory_order_relaxed);
          if(curr_b >= N) {
            return;
          }
          if(func(curr_b, std::min(curr_b + chunk_size, N))) {
            return;
          }
        }
        break;
      }
      // coarse-grained
      else {
        size_t q = static_cast<size_t>(p2 * r);
        if(q < chunk_size) {
          q = chunk_size;
        }
        //size_t curr_e = (q <= r) ? curr_b + q : N;
        size_t curr_e = std::min(curr_b + q, N);
        if(next.compare_exchange_strong(curr_b, curr_e, std::memory_order_relaxed,  std::memory_order_relaxed)) {
          if(func(curr_b, curr_e)) {
            return;
          }
          curr_b = next.load(std::memory_order_relaxed);
        }
      }
    }
  }
};

// ----------------------------------------------------------------------------
// Dynamic Partitioner
// ----------------------------------------------------------------------------

/**
@class DynamicPartitioner

@brief class to construct a dynamic partitioner for scheduling parallel algorithms

The partitioner splits iterations into many partitions each of size equal to the given chunk size.
Different partitions are distributed dynamically to workers without any specific order.
*/
class DynamicPartitioner : public PartitionerBase {

  public:
 
  DynamicPartitioner() : PartitionerBase{1} {};
   
  explicit DynamicPartitioner(size_t sz) : PartitionerBase (sz) {}
  
  // --------------------------------------------------------------------------
  // scheduling methods
  // --------------------------------------------------------------------------
  
  // @private
  template <typename F,   std::enable_if_t<std::is_invocable_r_v<void, F, size_t, size_t>, void>* = nullptr>
  void loop(
    size_t N, 
    size_t, 
    std::atomic<size_t>& next, 
    F&& func
  ) const {

    size_t chunk_size = (_chunk_size == 0) ? size_t{1} : _chunk_size;
    size_t curr_b = next.fetch_add(chunk_size, std::memory_order_relaxed);

    while(curr_b < N) {
      func(curr_b, std::min(curr_b + chunk_size, N));
      curr_b = next.fetch_add(chunk_size, std::memory_order_relaxed);
    }
  }
  
  //  @private
  template <typename F,  std::enable_if_t<std::is_invocable_r_v<bool, F, size_t, size_t>, void>* = nullptr>
  void loop_until(
    size_t N, 
    size_t, 
    std::atomic<size_t>& next, 
    F&& func
  ) const {

    size_t chunk_size = (_chunk_size == 0) ? size_t{1} : _chunk_size;
    size_t curr_b = next.fetch_add(chunk_size, std::memory_order_relaxed);

    while(curr_b < N) {
      if(func(curr_b, std::min(curr_b + chunk_size, N))) {
        return;
      }
      curr_b = next.fetch_add(chunk_size, std::memory_order_relaxed);
    }
  }
};

// ----------------------------------------------------------------------------
// Static Partitioner
// ----------------------------------------------------------------------------

/**
@class StaticPartitioner

@brief class to construct a dynamic partitioner for scheduling parallel algorithms

The partitioner divides iterations into chunks and distributes chunks to workers in order.
If the chunk size is not specified (default @c 0), the partitioner resorts to a chunk size that equally distributes iterations into workers.

@code{.cpp}
std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}
taskflow.for_each( data.begin(), data.end(), [](int i){}, StaticPartitioner(0) );
executor.run(taskflow).run();
@endcode
*/
class StaticPartitioner : public PartitionerBase {

  public:
 
  StaticPartitioner() : PartitionerBase{0} {};
   
  explicit StaticPartitioner(size_t sz) : PartitionerBase(sz) {}
  
  /**  查询调整后的 chunk size
   如果不为零，则返回给定的块大小，否则返回 N/W + (w < N%W)，其中 N 是迭代次数，W 是工人数，w 是工人 ID。
  */
  size_t adjusted_chunk_size(size_t N, size_t W, size_t w) const {
    return _chunk_size ? _chunk_size : N/W + (w < N % W);
  }
  
  // --------------------------------------------------------------------------
  // 调度方法
  // --------------------------------------------------------------------------

  // @private
  template <typename F,   std::enable_if_t<std::is_invocable_r_v<void, F, size_t, size_t>, void>* = nullptr >
  void loop(
    size_t N, 
    size_t W, 
    size_t curr_b, 
    size_t chunk_size,
    F&& func
  ) {
    size_t stride = W * chunk_size;
    while(curr_b < N) {
      size_t curr_e = std::min(curr_b + chunk_size, N);
      func(curr_b, curr_e);
      curr_b += stride;
    }
  }
  
 
  // @private
  template <typename F,  std::enable_if_t<std::is_invocable_r_v<bool, F, size_t, size_t>, void>* = nullptr>
  void loop_until(
    size_t N, 
    size_t W, 
    size_t curr_b, 
    size_t chunk_size,
    F&& func
  ) {
    size_t stride = W * chunk_size;
    while(curr_b < N) {
      size_t curr_e = std::min(curr_b + chunk_size, N);
      if(func(curr_b, curr_e)) {
        return;
      }
      curr_b += stride;
    }
  }
};

// ----------------------------------------------------------------------------
// RandomPartitioner
// ----------------------------------------------------------------------------

/**
@class RandomPartitioner

@brief class to construct a random partitioner for scheduling parallel algorithms

与 tf::DynamicPartitioner 类似，分区器将迭代分成许多分区，但每个分区的随机块大小在 c = [alpha * N * W, beta * N * W] 范围内。 默认情况下，alpha 分别为 0.01 和 beta 为 0.5 。
*/
class RandomPartitioner : public PartitionerBase {

  public:

  RandomPartitioner() = default;
   
  RandomPartitioner(size_t cz) : PartitionerBase(cz) {}
   
  RandomPartitioner(float alpha, float beta) : _alpha {alpha}, _beta {beta} {}

  // 查询 alpha 值
  float alpha() const { return _alpha; }
  
  // 查询 beta 值
  float beta() const { return _beta; }
  
  /**
  查询chunk大小的范围
   @param N 迭代次数
   @param W 工人数量
  */
  std::pair<size_t, size_t> chunk_size_range(size_t N, size_t W) const {
    
    size_t b1 = static_cast<size_t>(_alpha * N * W);
    size_t b2 = static_cast<size_t>(_beta  * N * W);

    if(b1 > b2) {
      std::swap(b1, b2);
    }

    b1 = std::max(b1, size_t{1});
    b2 = std::max(b2, b1 + 1);

    return {b1, b2};
  }

  // --------------------------------------------------------------------------
  // 调度方法
  // --------------------------------------------------------------------------
  
  //   @private
  template <typename F,  std::enable_if_t<std::is_invocable_r_v<void, F, size_t, size_t>, void>* = nullptr>
  void loop(
    size_t N, 
    size_t W, 
    std::atomic<size_t>& next, 
    F&& func
  ) const {

    auto [b1, b2] = chunk_size_range(N, W); 
    
    std::default_random_engine engine {std::random_device{}()};
    std::uniform_int_distribution<size_t> dist(b1, b2);
    
    size_t chunk_size = dist(engine);
    size_t curr_b = next.fetch_add(chunk_size, std::memory_order_relaxed);

    while(curr_b < N) {
      func(curr_b, std::min(curr_b + chunk_size, N));
      chunk_size = dist(engine);
      curr_b = next.fetch_add(chunk_size, std::memory_order_relaxed);
    }
  }


   //   @private
  template <typename F,  std::enable_if_t<std::is_invocable_r_v<bool, F, size_t, size_t>, void>* = nullptr>
  void loop_until(
    size_t N, 
    size_t W, 
    std::atomic<size_t>& next, 
    F&& func
  ) const {

    auto [b1, b2] = chunk_size_range(N, W); 
    
    std::default_random_engine engine {std::random_device{}()};
    std::uniform_int_distribution<size_t> dist(b1, b2);
    
    size_t chunk_size = dist(engine);
    size_t curr_b = next.fetch_add(chunk_size, std::memory_order_relaxed);

    while(curr_b < N) {
      if(func(curr_b, std::min(curr_b + chunk_size, N))){
        return;
      }
      chunk_size = dist(engine);
      curr_b = next.fetch_add(chunk_size, std::memory_order_relaxed);
    }
  }

  private:

  float _alpha {0.01f};
  float _beta  {0.5f};

};

/**
@brief default partitioner set to tf::GuidedPartitioner
Guided partitioner 可以为大多数并行算法实现不错的性能，尤其是对于那些每次迭代的工作负载不规则和不平衡的算法。
*/
using DefaultPartitioner = GuidedPartitioner;


/**
@brief determines if a type is a partitioner 
A partitioner is a derived type from tf::PartitionerBase.
*/
template <typename C>
inline constexpr bool is_partitioner_v = std::is_base_of<PartitionerBase, C>::value;

}  // end of namespace tf -----------------------------------------------------



