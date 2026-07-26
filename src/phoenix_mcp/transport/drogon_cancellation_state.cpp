#include "drogon_cancellation_state.h"

#include <utility>

namespace phoenix_mcp::server::drogon_internal {

std::pair<folly::CancellationToken, bool>
DrogonCancellationState::register_connection(const std::string& connection_id) {
  std::lock_guard lock(mutex_);
  auto [it, inserted] = sources_.try_emplace(
      connection_id, std::make_shared<folly::CancellationSource>());
  return {it->second->getToken(), inserted};
}

folly::CancellationToken DrogonCancellationState::token_for(
    const std::string& connection_id) {
  return register_connection(connection_id).first;
}

void DrogonCancellationState::close(const std::string& connection_id) {
  std::shared_ptr<folly::CancellationSource> source;
  {
    std::lock_guard lock(mutex_);
    const auto it = sources_.find(connection_id);
    if (it == sources_.end()) {
      return;
    }
    source = std::move(it->second);
    sources_.erase(it);
  }
  source->requestCancellation();
}

bool DrogonCancellationState::contains(const std::string& connection_id) const {
  std::lock_guard lock(mutex_);
  return sources_.contains(connection_id);
}

}  // namespace phoenix_mcp::server::drogon_internal
