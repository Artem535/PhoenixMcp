#include "runtime.h"

#include <algorithm>
#include <thread>

namespace pxm::runtime {

namespace {

size_t sanitize_threads(const size_t requested) {
  return std::max<size_t>(1, requested);
}

} // namespace

Runtime::Runtime(const size_t cpu_threads, const size_t io_threads)
    : cpu_pool_(sanitize_threads(cpu_threads)),
      io_pool_(sanitize_threads(io_threads)) {
}

folly::Executor::KeepAlive<folly::CPUThreadPoolExecutor>
Runtime::cpu_executor() {
  return cpu_pool_.getKeepAliveToken(cpu_pool_);
}

folly::Executor::KeepAlive<folly::IOThreadPoolExecutor>
Runtime::io_executor() {
  return io_pool_.getKeepAliveToken(io_pool_);
}

std::shared_ptr<Runtime> make_default_runtime() {
  const auto hardware_threads =
      static_cast<size_t>(std::thread::hardware_concurrency());
  const auto cpu_threads = sanitize_threads(hardware_threads);
  const auto io_threads = sanitize_threads(std::min<size_t>(4, cpu_threads));
  return std::make_shared<Runtime>(cpu_threads, io_threads);
}

} // namespace pxm::runtime
