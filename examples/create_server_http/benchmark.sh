#!/usr/bin/env bash

set -euo pipefail

BASE_URL="${BASE_URL:-http://127.0.0.1:8080}"
MCP_URL="${BASE_URL}/mcp"

REQUESTS="${REQUESTS:-100}"
CONCURRENCY="${CONCURRENCY:-8}"
DELAY_MS="${DELAY_MS:-1000}"
WARMUP="${WARMUP:-1}"

SUM_PAYLOAD='{"jsonrpc":"2.0","id":10,"method":"tools/call","params":{"name":"sum_tool","arguments":{"a":2,"b":3}}}'
DELAYED_PAYLOAD="{\"jsonrpc\":\"2.0\",\"id\":11,\"method\":\"tools/call\",\"params\":{\"name\":\"delayed_sum_tool\",\"arguments\":{\"a\":2,\"b\":3,\"delay_ms\":${DELAY_MS}}}}"
INIT_PAYLOAD='{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"benchmark","version":"1.0"}}}'
INITIALIZED_PAYLOAD='{"jsonrpc":"2.0","method":"notifications/initialized"}'

# Since #27, each TCP connection gets its own ServerSession, which must
# complete the initialize handshake before it accepts tools/call. A fresh
# curl invocation opens a fresh connection, so every call on a connection
# that hasn't handshaked gets rejected with "Invalid request method".
# initialized_post_json runs the handshake and the payload as one curl
# invocation (via --next) so they share a connection, the way a real client
# would.
initialized_post_json() {
  local payload="$1"
  curl -sS "${MCP_URL}" -H 'content-type: application/json' -d "${INIT_PAYLOAD}" \
    --next "${MCP_URL}" -H 'content-type: application/json' -d "${INITIALIZED_PAYLOAD}" \
    --next "${MCP_URL}" -H 'content-type: application/json' -d "${payload}" \
    >/dev/null
}

echo "[warmup]"
for _ in $(seq 1 "${WARMUP}"); do
  initialized_post_json "${SUM_PAYLOAD}"
  initialized_post_json "${DELAYED_PAYLOAD}"
done

if command -v hey >/dev/null 2>&1; then
  # NOTE: hey has no concept of "handshake this connection first, then send
  # the payload" — it opens its own connection pool and replays the same raw
  # request on it. Every one of those connections will get "Invalid request
  # method" back (no initialize ever happened on it), so this measures raw
  # HTTP throughput/latency only, not real tool-call execution. Use the
  # curl-based fallback below for a benchmark that reflects real MCP traffic.
  echo "[benchmark] sum_tool (raw HTTP throughput only -- see NOTE above; no MCP handshake)"
  hey -n "${REQUESTS}" -c "${CONCURRENCY}" -m POST \
    -H 'content-type: application/json' \
    -d "${SUM_PAYLOAD}" \
    "${MCP_URL}"

  echo
  echo "[benchmark] delayed_sum_tool delay_ms=${DELAY_MS} (raw HTTP throughput only -- see NOTE above)"
  hey -n "${REQUESTS}" -c "${CONCURRENCY}" -m POST \
    -H 'content-type: application/json' \
    -d "${DELAYED_PAYLOAD}" \
    "${MCP_URL}"
else
  echo "hey not found; running fallback benchmark with curl"
  echo "[benchmark] delayed_sum_tool delay_ms=${DELAY_MS} concurrency=${CONCURRENCY}"
  started_at="$(date +%s%3N)"
  for _ in $(seq 1 "${CONCURRENCY}"); do
    (
      initialized_post_json "${DELAYED_PAYLOAD}"
    ) &
  done
  wait
  finished_at="$(date +%s%3N)"
  elapsed_ms="$((finished_at - started_at))"
  echo "elapsed_ms: ${elapsed_ms}"
fi
