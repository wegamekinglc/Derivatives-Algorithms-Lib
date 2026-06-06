#!/usr/bin/env bash
# Stop the DAL web UI (FastAPI backend + React/Vite frontend).
#
# Usage:
#   ./dal-web/scripts/stop.sh [--force]
#
# What it does:
#   1. Reads the backend port from dal-web/frontend/vite.config.ts.
#   2. Kills each service by PID (from the .server.pid files written by
#      start.sh), falling back to killing by port if no PID file exists.
#   3. Removes the PID files.
#   4. Verifies that both ports are free.
#
# With --force, escalates from SIGTERM to SIGKILL if a process refuses
# to die within 5 seconds.
#
# Exit codes:
#   0  services stopped successfully (or were already stopped)
#   1  a service could not be stopped even with --force

set -euo pipefail

# ---------------------------------------------------------------------------
# Resolve repo root (this script lives in dal-web/scripts/)
# ---------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
cd "${REPO_ROOT}"

BACKEND_DIR="dal-web/backend"
FRONTEND_DIR="dal-web/frontend"
FRONTEND_PORT=5173

# Read backend port from vite.config.ts proxy target.
BACKEND_PORT="$(grep -E 'target.*http.*127\.0\.0\.1:[0-9]+' "${FRONTEND_DIR}/vite.config.ts" 2>/dev/null | grep -oE ':[0-9]+' | tr -d ':' || true)"
BACKEND_PORT="${BACKEND_PORT:-8001}"

# PID and log files
BACKEND_PID_FILE="${BACKEND_DIR}/.server.pid"
FRONTEND_PID_FILE="${FRONTEND_DIR}/.server.pid"

FORCE=0
[ "${1:-}" = "--force" ] && FORCE=1

# ---------------------------------------------------------------------------
# Colours and helpers
# ---------------------------------------------------------------------------
RED=$'\033[0;31m'; GREEN=$'\033[0;32m'; YELLOW=$'\033[0;33m'; NC=$'\033[0m'

info()  { printf "%s[info]%s  %s\n" "${GREEN}"  "${NC}" "$*"; }
warn()  { printf "%s[warn]%s  %s\n" "${YELLOW}" "${NC}" "$*"; }
error() { printf "%s[error]%s %s\n" "${RED}"    "${NC}" "$*" >&2; }

port_busy() {
  ss -tlnp 2>/dev/null | grep -qE ":${1}(\s|$)"
}

# Kill a process by PID with SIGTERM, waiting up to TIMEOUT seconds.
# Returns 0 on success, 1 if the process is still alive.
kill_graceful() {
  local pid="$1" name="$2" timeout="${3:-5}"
  if ! kill -0 "${pid}" 2>/dev/null; then
    # Process is already gone.
    return 0
  fi
  info "Sending SIGTERM to ${name} (PID ${pid})..."
  kill -TERM "${pid}" 2>/dev/null || true
  for _ in $(seq 1 $(( timeout * 2 ))); do
    if ! kill -0 "${pid}" 2>/dev/null; then
      return 0
    fi
    sleep 0.5
  done
  if kill -0 "${pid}" 2>/dev/null; then
    return 1
  fi
  return 0
}

# Kill a process by PID with SIGKILL (last resort).
kill_hard() {
  local pid="$1" name="$2"
  if ! kill -0 "${pid}" 2>/dev/null; then
    return 0
  fi
  warn "Sending SIGKILL to ${name} (PID ${pid})..."
  kill -KILL "${pid}" 2>/dev/null || true
  sleep 1
  if kill -0 "${pid}" 2>/dev/null; then
    return 1
  fi
  return 0
}

# Kill anything listening on a given port (fallback when no PID file).
kill_by_port() {
  local port="$1" name="$2"
  local pids
  pids="$(lsof -ti:"${port}" 2>/dev/null || true)"
  if [ -z "${pids}" ]; then
    return 0
  fi
  info "Killing ${name} by port ${port} (PIDs: ${pids//$'\n'/, })..."
  echo "${pids}" | xargs -r kill 2>/dev/null || true
  sleep 1
  if port_busy "${port}"; then
    if [ "${FORCE}" -eq 1 ]; then
      warn "Port ${port} still busy; escalating to SIGKILL..."
      echo "${pids}" | xargs -r kill -KILL 2>/dev/null || true
      sleep 1
    fi
  fi
}

# ---------------------------------------------------------------------------
# Prerequisites
# ---------------------------------------------------------------------------
check_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    error "$1 is not installed. Please install it and retry."
    return 1
  fi
}

FAILED_PREREQS=0
check_cmd grep      || FAILED_PREREQS=1
check_cmd ss        || FAILED_PREREQS=1
check_cmd lsof      || FAILED_PREREQS=1
check_cmd xargs     || FAILED_PREREQS=1
[ "${FAILED_PREREQS}" -eq 0 ] || exit 1

# ---------------------------------------------------------------------------
# 1. Check current state
# ---------------------------------------------------------------------------
BACKEND_RUNNING=0
FRONTEND_RUNNING=0
port_busy "${BACKEND_PORT}"   && BACKEND_RUNNING=1
port_busy "${FRONTEND_PORT}"  && FRONTEND_RUNNING=1

if [ "${BACKEND_RUNNING}" -eq 0 ] && [ "${FRONTEND_RUNNING}" -eq 0 ]; then
  info "No DAL web UI services are running (ports ${BACKEND_PORT} and ${FRONTEND_PORT} are both free)."
  # Clean up any stale PID files.
  rm -f "${REPO_ROOT}/${BACKEND_PID_FILE}" "${REPO_ROOT}/${FRONTEND_PID_FILE}"
  exit 0
fi

# Kill a service. Try PID-based first (if the PID file exists), then verify
# the port is actually free. Some launchers (notably `npm run <script>`)
# spawn a child process that inherits the listening socket; killing the
# parent PID we tracked can leave that child alive and the port still
# bound. The post-PID port check catches those orphans and anything else
# that slipped through.
stop_service() {
  local port="$1" name="$2" pid_file="$3"

  if [ -f "${REPO_ROOT}/${pid_file}" ]; then
    local pid
    pid="$(cat "${REPO_ROOT}/${pid_file}")"
    if ! kill_graceful "${pid}" "${name}" 5; then
      if [ "${FORCE}" -eq 1 ]; then
        kill_hard "${pid}" "${name}" || error "${name} PID ${pid} could not be killed."
      else
        warn "${name} (PID ${pid}) did not stop within 5s. Re-run with --force to escalate."
      fi
    fi
    rm -f "${REPO_ROOT}/${pid_file}"
  else
    warn "No ${name} PID file found."
  fi

  # Defense in depth: the tracked PID may be gone while a child it spawned
  # still holds the port. Fall back to port-based kill in that case.
  if port_busy "${port}"; then
    warn "${name} port ${port} still busy after PID kill; falling back to port-based kill..."
    kill_by_port "${port}" "${name}"
  fi
}

# ---------------------------------------------------------------------------
# 2. Stop backend
# ---------------------------------------------------------------------------
if [ "${BACKEND_RUNNING}" -eq 1 ]; then
  stop_service "${BACKEND_PORT}" "backend" "${BACKEND_PID_FILE}"
fi

# ---------------------------------------------------------------------------
# 3. Stop frontend
# ---------------------------------------------------------------------------
if [ "${FRONTEND_RUNNING}" -eq 1 ]; then
  stop_service "${FRONTEND_PORT}" "frontend" "${FRONTEND_PID_FILE}"
fi

# ---------------------------------------------------------------------------
# 4. Final verification
# ---------------------------------------------------------------------------
sleep 1
REMAINING=0
if port_busy "${BACKEND_PORT}"; then
  error "Backend is still listening on port ${BACKEND_PORT}."
  REMAINING=1
fi
if port_busy "${FRONTEND_PORT}"; then
  error "Frontend is still listening on port ${FRONTEND_PORT}."
  REMAINING=1
fi

if [ "${REMAINING}" -eq 0 ]; then
  printf "%s✓ DAL web UI stopped.%s  Ports ${BACKEND_PORT} and ${FRONTEND_PORT} are free.\n" "${GREEN}" "${NC}"
  exit 0
else
  error "Some services could not be stopped. Try: ./dal-web/scripts/stop.sh --force"
  error "Or manually: sudo fuser -k ${BACKEND_PORT}/tcp; sudo fuser -k ${FRONTEND_PORT}/tcp"
  exit 1
fi
