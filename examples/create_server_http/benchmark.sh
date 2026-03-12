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

post_json() {
  local payload="$1"
  curl -sS "${MCP_URL}" \
    -H 'content-type: application/json' \
    -d "${payload}" >/dev/null
}

echo "[warmup] initialize"
curl -sS "${MCP_URL}" \
  -H 'content-type: application/json' \
  -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"benchmark","version":"1.0"}}}' >/dev/null || true
curl -sS "${MCP_URL}" \
  -H 'content-type: application/json' \
  -d '{"jsonrpc":"2.0","method":"notifications/initialized"}' >/dev/null || true

for _ in $(seq 1 "${WARMUP}"); do
  post_json "${SUM_PAYLOAD}"
  post_json "${DELAYED_PAYLOAD}"
done

if command -v hey >/dev/null 2>&1; then
  echo "[benchmark] sum_tool"
  hey -n "${REQUESTS}" -c "${CONCURRENCY}" -m POST \
    -H 'content-type: application/json' \
    -d "${SUM_PAYLOAD}" \
    "${MCP_URL}"

  echo
  echo "[benchmark] delayed_sum_tool delay_ms=${DELAY_MS}"
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
      post_json "${DELAYED_PAYLOAD}"
    ) &
  done
  wait
  finished_at="$(date +%s%3N)"
  elapsed_ms="$((finished_at - started_at))"
  echo "elapsed_ms: ${elapsed_ms}"
fi
