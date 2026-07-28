#!/usr/bin/env bash
# Start the real DAL FastAPI routers with a canned DAL test double plus Vite.
# This launcher is exclusively for Playwright smoke tests; normal start.sh is
# intentionally native-only.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BACKEND_DIR="${REPO_ROOT}/dal-web/backend"
FRONTEND_DIR="${REPO_ROOT}/dal-web/frontend"
BACKEND_PORT="${DAL_PLAYWRIGHT_BACKEND_PORT:-8001}"
FRONTEND_PORT="${DAL_PLAYWRIGHT_FRONTEND_PORT:-5173}"
BACKEND_URL="http://127.0.0.1:${BACKEND_PORT}/api/health"
FRONTEND_URL="http://localhost:${FRONTEND_PORT}"

if [ "${DAL_PLAYWRIGHT_TEST_BACKEND:-}" != "1" ]; then
  echo "error: set DAL_PLAYWRIGHT_TEST_BACKEND=1 to use the canned Playwright backend" >&2
  exit 1
fi

BACKEND_PYTHON="${BACKEND_DIR}/.venv/bin/python"
VITE="${FRONTEND_DIR}/node_modules/.bin/vite"
if [ ! -x "${BACKEND_PYTHON}" ]; then
  echo "error: backend environment missing; run (cd dal-web/backend && uv sync --inexact)" >&2
  exit 1
fi
if [ ! -x "${VITE}" ]; then
  echo "error: frontend dependencies missing; run (cd dal-web/frontend && npm ci)" >&2
  exit 1
fi
if ! command -v curl >/dev/null 2>&1; then
  echo "error: curl is required" >&2
  exit 1
fi

LOG_DIR="$(mktemp -d "${TMPDIR:-/tmp}/dal-playwright.XXXXXX")"
BACKEND_LOG="${LOG_DIR}/backend.log"
FRONTEND_LOG="${LOG_DIR}/frontend.log"
BACKEND_PID=""
FRONTEND_PID=""

cleanup() {
  local status=$?
  local pid
  local alive
  trap - EXIT INT TERM

  for pid in "${FRONTEND_PID}" "${BACKEND_PID}"; do
    if [ -n "${pid}" ] && kill -0 "${pid}" 2>/dev/null; then
      kill -TERM "${pid}" 2>/dev/null || true
    fi
  done

  for _ in $(seq 1 50); do
    alive=0
    for pid in "${FRONTEND_PID}" "${BACKEND_PID}"; do
      if [ -n "${pid}" ] && kill -0 "${pid}" 2>/dev/null; then
        alive=1
      fi
    done
    [ "${alive}" -eq 0 ] && break
    sleep 0.1
  done

  for pid in "${FRONTEND_PID}" "${BACKEND_PID}"; do
    if [ -n "${pid}" ] && kill -0 "${pid}" 2>/dev/null; then
      kill -KILL "${pid}" 2>/dev/null || true
    fi
    if [ -n "${pid}" ]; then
      wait "${pid}" 2>/dev/null || true
    fi
  done

  rm -rf "${LOG_DIR}"
  exit "${status}"
}

trap cleanup EXIT
trap 'exit 130' INT TERM

show_log() {
  local name="$1"
  local path="$2"
  echo "${name} log:" >&2
  tail -n 40 "${path}" >&2 || true
}

wait_for_service() {
  local name="$1"
  local pid="$2"
  local url="$3"
  local log="$4"
  for _ in $(seq 1 120); do
    if ! kill -0 "${pid}" 2>/dev/null; then
      echo "error: ${name} exited before becoming ready" >&2
      show_log "${name}" "${log}"
      return 1
    fi
    if curl -sf "${url}" >/dev/null 2>&1; then
      return 0
    fi
    sleep 0.25
  done
  echo "error: ${name} did not become ready within 30 seconds" >&2
  show_log "${name}" "${log}"
  return 1
}

(
  cd "${BACKEND_DIR}"
  exec env DAL_PLAYWRIGHT_TEST_BACKEND=1 "${BACKEND_PYTHON}" -m uvicorn \
    tests.playwright_backend:app --host 127.0.0.1 --port "${BACKEND_PORT}"
) >"${BACKEND_LOG}" 2>&1 &
BACKEND_PID=$!
wait_for_service "Playwright test backend" "${BACKEND_PID}" "${BACKEND_URL}" "${BACKEND_LOG}"

(
  cd "${FRONTEND_DIR}"
  exec env \
    DAL_PLAYWRIGHT_BACKEND_PORT="${BACKEND_PORT}" \
    DAL_PLAYWRIGHT_FRONTEND_PORT="${FRONTEND_PORT}" \
    "${VITE}" --host 127.0.0.1 --port "${FRONTEND_PORT}" --strictPort
) >"${FRONTEND_LOG}" 2>&1 &
FRONTEND_PID=$!
wait_for_service "Vite frontend" "${FRONTEND_PID}" "${FRONTEND_URL}" "${FRONTEND_LOG}"

echo "Playwright test backend ready: canned DAL + FastAPI + Vite"

# Playwright owns this webServer process.  Stay alive until it terminates us,
# while also failing promptly if either child exits unexpectedly.
while true; do
  if ! kill -0 "${BACKEND_PID}" 2>/dev/null; then
    echo "error: Playwright test backend exited unexpectedly" >&2
    show_log "Playwright test backend" "${BACKEND_LOG}"
    exit 1
  fi
  if ! kill -0 "${FRONTEND_PID}" 2>/dev/null; then
    echo "error: Vite frontend exited unexpectedly" >&2
    show_log "Vite frontend" "${FRONTEND_LOG}"
    exit 1
  fi
  sleep 1
done
