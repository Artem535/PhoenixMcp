#!/usr/bin/env bash

set -euo pipefail

server_executable="$1"
smoke_script="$2"
base_url="http://127.0.0.1:8080"
server_log="$(mktemp)"

cleanup() {
  if [[ -n "${server_pid:-}" ]] && kill -0 "${server_pid}" 2>/dev/null; then
    kill "${server_pid}" 2>/dev/null || true
    wait "${server_pid}" 2>/dev/null || true
  fi
  rm -f "${server_log}"
}
trap cleanup EXIT

if curl --silent --show-error --fail --max-time 1 "${base_url}/health" \
    >/dev/null 2>&1; then
  echo "Refusing to run HTTP integration test: ${base_url} is already in use" >&2
  exit 1
fi

"${server_executable}" >"${server_log}" 2>&1 &
server_pid=$!

for _ in $(seq 1 50); do
  if curl --silent --show-error --fail --max-time 1 "${base_url}/health" \
      >/dev/null 2>&1; then
    break
  fi
  sleep 0.1
done

if ! curl --silent --show-error --fail --max-time 1 "${base_url}/health" \
    >/dev/null; then
  cat "${server_log}" >&2
  echo "HTTP example did not become ready" >&2
  exit 1
fi

output="$(BASE_URL="${base_url}" bash "${smoke_script}")"
printf '%s\n' "${output}"

grep --fixed-strings --quiet '"protocolVersion":"2025-06-18"' <<<"${output}"
grep --fixed-strings --quiet '\"sum\":5' <<<"${output}"
grep --fixed-strings --quiet '\"async\":true' <<<"${output}"

session_headers="$(mktemp)"
trap 'rm -f "${session_headers}"; cleanup' EXIT
curl --silent --show-error -D "${session_headers}" -o /dev/null \
  -H 'content-type: application/json' \
  -d '{"jsonrpc":"2.0","id":8,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"streamable-http-test","version":"1.0"}}}' \
  "${base_url}/mcp"
session_id="$(awk 'tolower($1) == "mcp-session-id:" { sub(/^[^:]*: */, ""); sub(/\r$/, ""); print; exit }' "${session_headers}")"
test -n "${session_id}"

missing_session_status="$(curl --silent --show-error -o /dev/null -w '%{http_code}' \
  "${base_url}/mcp")"
test "${missing_session_status}" = '400'
unsupported_content_type_status="$(curl --silent --show-error -o /dev/null -w '%{http_code}' \
  -X POST -H 'Content-Type: text/plain' -d '{}' "${base_url}/mcp")"
test "${unsupported_content_type_status}" = '415'
unsupported_accept_status="$(curl --silent --show-error -o /dev/null -w '%{http_code}' \
  -H "Mcp-Session-Id: ${session_id}" -H 'Accept: application/json' \
  "${base_url}/mcp")"
test "${unsupported_accept_status}" = '406'

sse_output="$(curl --silent --show-error --max-time 1 \
  -H "Mcp-Session-Id: ${session_id}" -H 'Accept: text/event-stream' \
  "${base_url}/mcp" || true)"
grep --fixed-strings --quiet 'retry: 1000' <<<"${sse_output}"

delete_status="$(curl --silent --show-error -o /dev/null -w '%{http_code}' \
  -X DELETE -H "Mcp-Session-Id: ${session_id}" "${base_url}/mcp")"
test "${delete_status}" = '204'
closed_status="$(curl --silent --show-error -o /dev/null -w '%{http_code}' \
  -H "Mcp-Session-Id: ${session_id}" "${base_url}/mcp")"
test "${closed_status}" = '404'
