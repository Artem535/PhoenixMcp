#ifndef PHOENIX_MCP_SERVER_SESSION_STATES_H_
#define PHOENIX_MCP_SERVER_SESSION_STATES_H_

#include <chrono>
#include <string>

#include <hfsm2/machine.hpp>

namespace phoenix_mcp::server {

// Forward declaration — states hold a reference back to the owning session
// (e.g. to read ServerConfig timeouts) via the FSM context.
class ServerSession;

// ---------------------------------------------------------------------------
// Events — what triggers state transitions
// ---------------------------------------------------------------------------

struct InitializeRequest {};
struct InitializedNotification {};
struct TimeoutExpired {};
struct ShutdownRequest {};
struct SettleComplete {};

// ---------------------------------------------------------------------------
// State definitions
// ---------------------------------------------------------------------------
//
// HFSM2 transitions are not table-driven: a state requests a transition by
// calling `control.changeTo<Target>()` from within its own `react()` (or
// `update()`) method. States must be forward-declared before the FSM type is
// defined, then given full bodies that derive from `SessionFSM::State` (this
// mirrors the HFSM2 "minimal example" idiom — see the project's design doc).

namespace states {

using Config = hfsm2::Config::ContextT<ServerSession&>;
using Machine = hfsm2::MachineT<Config>;

using SessionFSM = Machine::PeerRoot<
    struct Uninitialized,
    struct Initializing,
    struct Operation,
    struct Failed,
    struct Stopping,
    struct Settled>;

// HFSM2 dispatches every event to every active state through a single
// static-typed lookup; a state that overrides `react()` for only some of the
// FSM's event types (rather than all of them, even as no-ops) makes that
// lookup ill-formed. `NoOpReactions` supplies a do-nothing overload for each
// event type so concrete states only need to override the ones they care
// about (bringing the rest back into scope with `using NoOpReactions::react`).
struct NoOpReactions : SessionFSM::State {
  void react(const InitializeRequest&, EventControl&) {}
  void react(const InitializedNotification&, EventControl&) {}
  void react(const TimeoutExpired&, EventControl&) {}
  void react(const ShutdownRequest&, EventControl&) {}
  void react(const SettleComplete&, EventControl&) {}
};

struct Uninitialized : NoOpReactions {
  using NoOpReactions::react;
  void react(const InitializeRequest&, EventControl& control);
};

struct Initializing : NoOpReactions {
  using NoOpReactions::react;
  std::chrono::steady_clock::time_point deadline;

  void enter(PlanControl& control);
  void react(const InitializedNotification&, EventControl& control);
  void react(const TimeoutExpired&, EventControl& control);
};

struct Operation : NoOpReactions {
  using NoOpReactions::react;
  void enter(PlanControl& control);
  void react(const ShutdownRequest&, EventControl& control);
};

struct Failed : NoOpReactions {
  using NoOpReactions::react;
  std::string reason;

  void enter(PlanControl& control);
  void react(const ShutdownRequest&, EventControl& control);
};

struct Stopping : NoOpReactions {
  using NoOpReactions::react;
  std::chrono::steady_clock::time_point settle_deadline;

  void enter(PlanControl& control);
  void react(const SettleComplete&, EventControl& control);
};

struct Settled : NoOpReactions {
  using NoOpReactions::react;
  void enter(PlanControl& control);
};

}  // namespace states

using SessionFSM = states::SessionFSM;

}  // namespace phoenix_mcp::server

#endif  // PHOENIX_MCP_SERVER_SESSION_STATES_H_
