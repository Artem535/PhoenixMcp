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
