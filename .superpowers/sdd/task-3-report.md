# Task 3 evidence: public SDK header layout

## Delivered

- Added self-contained public `.h` headers beneath `include/phoenix_mcp` for
  runtime/core, protocol messages, server, tool registry, stdio transport, and
  the legacy abstract transport surface.
- Moved implementation consumers to `namespace phoenix_mcp`; source-side
  headers are now private forwarding shims to the public declarations.
- Added `include/phoenix_mcp/compat/pxm.h`. It creates a deprecated `pxm`
  transition namespace that imports `phoenix_mcp` only when explicitly
  included.
- Updated the stdio examples and CMake include propagation to consume public
  headers. Optional Crow and Drogon adapters were not modified.
- Added the CTest `phoenix_mcp_public_headers` compilation executable, which
  includes every public header and the compatibility shim.

## Red-green evidence

1. Before the include tree existed, `rtk cmake --build --preset linux-minimal
   --target phoenix_mcp_public_headers_test` failed with `phoenix_mcp/compat/pxm.h:
   No such file or directory`.
2. Initial implementation exposed an invalid deprecated namespace-alias syntax
   and an incomplete `McpSession` ownership boundary. The compatibility header
   now uses a deprecated namespace definition; `McpRequestHandler` has an
   out-of-line destructor in the implementation that includes the private
   session header.

## Final verification

- `rtk cmake --build --preset linux-minimal` — passed; library, public-header
  check, `create_server`, and `using_tool_registry` linked.
- `rtk ctest --preset linux-minimal --output-on-failure` — passed: 2/2 tests
  (`phoenix_mcp_public_headers`, `phoenix_mcp_manual_authority`).
- `rtk git diff --check` — passed.
- `rtk xmake f -m debug` — not feasible in this environment because the `rtk`
  wrapper reports `No such file or directory` for `xmake`.

## Notes

The minimal build emits existing warning-class diagnostics for partial
aggregate initialization and a pessimizing move in the legacy server/tool
implementation. They do not fail the configured warning policy.
