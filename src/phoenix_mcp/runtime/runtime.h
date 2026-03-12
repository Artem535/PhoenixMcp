#pragma once

#include <cstddef>
#include <memory>

#include <folly/Executor.h>
#include <folly/executors/CPUThreadPoolExecutor.h>
#include <folly/executors/IOThreadPoolExecutor.h>

namespace pxm::runtime {

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

} // namespace pxm::runtime
