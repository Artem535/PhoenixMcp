#include "phoenix_mcp/server/session_states.h"

#include <spdlog/spdlog.h>

#include "phoenix_mcp/server/server_session.h"

namespace phoenix_mcp::server::states {

// ---------------------------------------------------------------------------
// Uninitialized state
// ---------------------------------------------------------------------------

void Uninitialized::react(const InitializeRequest&, EventControl& control) {
  control.changeTo<Initializing>();
}

// ---------------------------------------------------------------------------
// Initializing state
// ---------------------------------------------------------------------------

void Initializing::enter(PlanControl& control) {
  deadline = std::chrono::steady_clock::now() +
             control.context().config().init_timeout;
  spdlog::info("SessionStates| Entered Initializing state");
}

void Initializing::react(const InitializedNotification&,
                          EventControl& control) {
  control.changeTo<Operation>();
}

void Initializing::react(const TimeoutExpired&, EventControl& control) {
  control.changeTo<Failed>();
}

// ---------------------------------------------------------------------------
// Operation state
// ---------------------------------------------------------------------------

void Operation::enter(PlanControl&) {
  spdlog::info("SessionStates| Entered Operation state");
}

void Operation::react(const ShutdownRequest&, EventControl& control) {
  control.changeTo<Stopping>();
}

// ---------------------------------------------------------------------------
// Failed state
// ---------------------------------------------------------------------------

void Failed::enter(PlanControl&) {
  spdlog::error("SessionStates| Entered Failed state");
}

void Failed::react(const ShutdownRequest&, EventControl& control) {
  control.changeTo<Stopping>();
}

// ---------------------------------------------------------------------------
// Stopping state
// ---------------------------------------------------------------------------

void Stopping::enter(PlanControl& control) {
  settle_deadline = std::chrono::steady_clock::now() +
                    control.context().config().settle_timeout;
  spdlog::info("SessionStates| Entered Stopping state");
}

void Stopping::react(const SettleComplete&, EventControl& control) {
  control.changeTo<Settled>();
}

// ---------------------------------------------------------------------------
// Settled state
// ---------------------------------------------------------------------------

void Settled::enter(PlanControl&) {
  spdlog::info("SessionStates| Entered Settled state");
}

}  // namespace phoenix_mcp::server::states
