#ifndef PHOENIX_MCP_CORE_RUNTIME_H_
#define PHOENIX_MCP_CORE_RUNTIME_H_

#include <folly/Executor.h>
#include <folly/executors/CPUThreadPoolExecutor.h>
#include <folly/executors/IOThreadPoolExecutor.h>

#include <memory>

namespace phoenix_mcp::runtime {

class Runtime {
 public:
  Runtime(size_t cpu_threads, size_t io_threads);

  folly::Executor::KeepAlive<folly::CPUThreadPoolExecutor> cpu_executor();
  folly::Executor::KeepAlive<folly::IOThreadPoolExecutor> io_executor();

 private:
  folly::CPUThreadPoolExecutor cpu_pool_;
  folly::IOThreadPoolExecutor io_pool_;
};

std::shared_ptr<Runtime> make_default_runtime();

}  // namespace phoenix_mcp::runtime

#endif  // PHOENIX_MCP_CORE_RUNTIME_H_