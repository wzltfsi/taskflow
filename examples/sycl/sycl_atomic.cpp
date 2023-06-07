// 该程序演示了如何使用 syclFlow 和统一共享内存 (USM) 创建一个简单的向量相加应用程序。
#include <taskflow/sycl/syclflow.hpp>

constexpr size_t N = 10000;

int main() {

  // 创建一个独立的 scylFlow
  sycl::queue queue;

  tf::syclFlow syclflow(queue);

  // 分配共享内存并初始化数据
  auto data = sycl::malloc_shared<int>(N, queue);

  for(size_t i=0; i<N; i++) {
    data[i] = i;
  }

  // 使用 ONEAPI atomic_ref 将总和减少到第一个元素
  syclflow.parallel_for(
    sycl::range<1>(N), [=](sycl::id<1> id) {

      auto ref = sycl::atomic_ref<
        int, 
        sycl::memory_order_relaxed, 
        sycl::memory_scope::device,
        sycl::access::address_space::global_space
      >{data[0]};

      ref.fetch_add(data[id]);
    }
  );

  // run the syclflow
  syclflow.offload();

  // create a deallocate task that checks the result and frees the memory
  if(data[0] != (N-1)*N/2) {
    std::cout << data[0] << '\n';
    throw std::runtime_error("incorrect result");
  }

  std::cout << "correct result\n";

  // deallocates the memory
  sycl::free(data, queue);


  return 0;
}


