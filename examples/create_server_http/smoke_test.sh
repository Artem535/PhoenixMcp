#!/usr/bin/env bash

set -euo pipefail

BASE_URL="${BASE_URL:-http://127.0.0.1:8080}"
MCP_URL="${BASE_URL}/mcp"
HEALTH_URL="${BASE_URL}/health"
TMP_DIR="$(mktemp -d)"

cleanup() {
  rm -rf "${TMP_DIR}"
}

trap cleanup EXIT

echo "[1/6] health"
health_started_at="$(date +%s%3N)"
health_status="$(curl -sS -o "${TMP_DIR}/health" -w '%{http_code}' "${HEALTH_URL}")"
health_finished_at="$(date +%s%3N)"
health_elapsed_ms="$((health_finished_at - health_started_at))"
echo "status: ${health_status}"
echo "elapsed_ms: ${health_elapsed_ms}"
cat "${TMP_DIR}/health"
echo

# Every request past this point must reuse the same TCP connection: since
# #27, each connection gets its own ServerSession, so a fresh connection per
# request (what separate `curl` invocations would do) would see an
# uninitialized session on every call after the first. `curl --next` chains
# multiple requests in one invocation, reusing the connection to the same
# host across them, the same way a real long-lived MCP client would.
declare -a labels=(
  "initialize"
  "notifications/initialized"
  "sum_tool"
  "delayed_sum_tool"
  "delayed_sum_struct_tool"
)
declare -a payloads=(
  '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"smoke-test","version":"1.0"}}}'
  '{"jsonrpc":"2.0","method":"notifications/initialized"}'
  '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"sum_tool","arguments":{"a":2,"b":3}}}'
  '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"delayed_sum_tool","arguments":{"a":2,"b":3,"delay_ms":1000}}}'
  '{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"delayed_sum_struct_tool","arguments":{"a":2,"b":3,"delay_ms":1000}}}'
)

curl_args=()
for i in "${!payloads[@]}"; do
  if [ "${i}" -gt 0 ]; then
    curl_args+=(--next)
  fi
  curl_args+=(
    -sS -o "${TMP_DIR}/body.${i}"
    -w "status: %{http_code}\nelapsed_ms: %{time_total}\n"
    "${MCP_URL}" -H 'content-type: application/json' -d "${payloads[i]}"
  )
done
curl "${curl_args[@]}" > "${TMP_DIR}/meta"

meta_line=0
for i in "${!labels[@]}"; do
  echo "[$((i + 2))/6] ${labels[i]}"
  sed -n "$((meta_line + 1)),$((meta_line + 2))p" "${TMP_DIR}/meta"
  meta_line=$((meta_line + 2))
  cat "${TMP_DIR}/body.${i}"
  echo
done

payload='{"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"delayed_sum_tool","arguments":{"a":2,"b":3,"delay_ms":1000}}}'
if command -v hey >/dev/null 2>&1; then
  # NOTE: hey has no concept of "handshake this connection, then send the
  # payload" -- since #27 every connection needs its own initialize first,
  # so hey's raw requests all get "Invalid request method" back. This only
  # measures raw HTTP throughput, not real tool-call execution; the curl
  # fallback below (each worker self-initializes its own connection) is the
  # one that actually exercises delayed_sum_tool under concurrency.
  echo "[extra] parallel load with hey (raw HTTP throughput only -- no MCP handshake, see NOTE above)"
  hey -n 20 -c 4 -m POST \
    -H 'content-type: application/json' \
    -d "${payload}" \
    "${MCP_URL}"
else
  echo "[extra] hey not found, running 4 parallel curl requests (each self-initializes its own connection)"
  for _ in 1 2 3 4; do
    (
      started_at="$(date +%s%3N)"
      response="$(curl -sS "${MCP_URL}" -H 'content-type: application/json' -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"smoke-test","version":"1.0"}}}' \
        --next "${MCP_URL}" -H 'content-type: application/json' -d '{"jsonrpc":"2.0","method":"notifications/initialized"}' \
        --next "${MCP_URL}" -H 'content-type: application/json' -d "${payload}")"
      finished_at="$(date +%s%3N)"
      elapsed_ms="$((finished_at - started_at))"
      echo "elapsed_ms: ${elapsed_ms} ${response}"
    ) &
  done
  wait
  echo
fi
