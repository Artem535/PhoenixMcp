#!/usr/bin/env bash

set -euo pipefail

BASE_URL="${BASE_URL:-http://127.0.0.1:8080}"
MCP_URL="${BASE_URL}/mcp"
HEALTH_URL="${BASE_URL}/health"
TMP_BODY="$(mktemp)"

cleanup() {
  rm -f "${TMP_BODY}"
}

trap cleanup EXIT

post_json() {
  local payload="$1"
  local status
  local started_at
  local finished_at
  local elapsed_ms
  started_at="$(date +%s%3N)"
  status="$(curl -sS -o "${TMP_BODY}" -w '%{http_code}' "${MCP_URL}" \
    -H 'content-type: application/json' \
    -d "${payload}")"
  finished_at="$(date +%s%3N)"
  elapsed_ms="$((finished_at - started_at))"
  echo "status: ${status}"
  echo "elapsed_ms: ${elapsed_ms}"
  cat "${TMP_BODY}"
}

echo "[1/6] health"
health_started_at="$(date +%s%3N)"
health_status="$(curl -sS -o "${TMP_BODY}" -w '%{http_code}' "${HEALTH_URL}")"
health_finished_at="$(date +%s%3N)"
health_elapsed_ms="$((health_finished_at - health_started_at))"
echo "status: ${health_status}"
echo "elapsed_ms: ${health_elapsed_ms}"
cat "${TMP_BODY}"
echo

echo "[2/6] initialize"
post_json '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"smoke-test","version":"1.0"}}}'
echo

echo "[3/6] notifications/initialized"
post_json '{"jsonrpc":"2.0","method":"notifications/initialized"}'
echo

echo "[4/6] sum_tool"
post_json '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"sum_tool","arguments":{"a":2,"b":3}}}'
echo

echo "[5/6] delayed_sum_tool"
post_json '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"delayed_sum_tool","arguments":{"a":2,"b":3,"delay_ms":1000}}}'
echo

echo "[6/6] delayed_sum_struct_tool"
post_json '{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"delayed_sum_struct_tool","arguments":{"a":2,"b":3,"delay_ms":1000}}}'
echo

if command -v hey >/dev/null 2>&1; then
  echo "[extra] parallel load with hey"
  hey -n 20 -c 4 -m POST \
    -H 'content-type: application/json' \
    -d '{"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"delayed_sum_tool","arguments":{"a":2,"b":3,"delay_ms":1000}}}' \
    "${MCP_URL}"
else
  echo "[extra] hey not found, running 4 parallel curl requests"
  payload='{"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"delayed_sum_tool","arguments":{"a":2,"b":3,"delay_ms":1000}}}'
  for _ in 1 2 3 4; do
    (
      started_at="$(date +%s%3N)"
      response="$(curl -sS "${MCP_URL}" -H 'content-type: application/json' -d "${payload}")"
      finished_at="$(date +%s%3N)"
      elapsed_ms="$((finished_at - started_at))"
      echo "elapsed_ms: ${elapsed_ms} ${response}"
    ) &
  done
  wait
  echo
fi
