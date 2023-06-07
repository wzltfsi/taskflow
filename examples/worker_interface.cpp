// 他的程序演示了如何在创建 executor 时更改 worker 的行为 
 

#include <taskflow/taskflow.hpp>

class CustomWorkerBehavior : public tf::WorkerInterface {

  public:
  
  // 在 worker 进入调度循环之前调用  
  void scheduler_prologue(tf::Worker& w) override {
    std::cout << tf::stringify( "worker ", w.id(), " (native=", w.thread()->native_handle(), ") enters scheduler\n"  ); 
  }

  // 在 worker 离开调度循环后调用  
  void scheduler_epilogue(tf::Worker& w, std::exception_ptr) override {
    std::cout << tf::stringify( "worker ", w.id(), " (native=", w.thread()->native_handle(), ") leaves scheduler\n" ); 
  }
};

int main() {
  tf::Executor executor(4, std::make_shared<CustomWorkerBehavior>());
  return 0;
}
