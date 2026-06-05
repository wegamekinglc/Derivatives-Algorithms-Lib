#!/usr/bin/env bash
# Start the DAL web UI (FastAPI backend + React/Vite frontend).
#
# Usage:
#   ./webui/scripts/start.sh
#
# What it does:
#   1. Verifies prerequisites (python 3.13+, uv, node, npm).
#   2. Reads the backend port from webui/frontend/vite.config.ts.
#   3. Checks that both ports (backend + 5173) are free.
#   4. Starts the backend (uvicorn) in the background.
#   5. Starts the frontend (vite) in the background.
#   6. Waits for both to be ready, then runs a smoke test.
#   7. Prints the URLs.
#
# Logs are written to webui/backend/.server.log and
# webui/frontend/.server.log. PIDs are stored in .server.pid next to
# the respective server directory, so stop.sh can kill them cleanly.
#
# Exit codes:
#   0  both services started successfully
#   1  prerequisites missing or ports already in use
#   2  backend failed to start
#   3  frontend failed to start

set -euo pipefail

# ---------------------------------------------------------------------------
# Resolve repo root (this script lives in webui/scripts/)
# ---------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
cd "${REPO_ROOT}"

BACKEND_DIR="webui/backend"
FRONTEND_DIR="webui/frontend"
FRONTEND_PORT=5173

# Read backend port from vite.config.ts proxy target.
# Matches lines like: target: "http://127.0.0.1:8001"
if ! command -v grep >/dev/null 2>&1; then
  echo "error: grep is required but not found" >&2
  exit 1
fi
BACKEND_PORT="$(grep -E 'target.*http.*127\.0\.0\.1:[0-9]+' "${FRONTEND_DIR}/vite.config.ts" 2>/dev/null | grep -oE ':[0-9]+' | tr -d ':' || true)"
BACKEND_PORT="${BACKEND_PORT:-8001}"

# PID and log files
BACKEND_PID_FILE="${BACKEND_DIR}/.server.pid"
BACKEND_LOG_FILE="${BACKEND_DIR}/.server.log"
FRONTEND_PID_FILE="${FRONTEND_DIR}/.server.pid"
FRONTEND_LOG_FILE="${FRONTEND_DIR}/.server.log"

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

# ---------------------------------------------------------------------------
# 1. Prerequisites
# ---------------------------------------------------------------------------
info "Checking prerequisites..."

check_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    error "$1 is not installed. Please install it and retry."
    return 1
  fi
}

FAILED_PREREQS=0
check_cmd python3   || FAILED_PREREQS=1
check_cmd uv        || FAILED_PREREQS=1
check_cmd node      || FAILED_PREREQS=1
check_cmd npm       || FAILED_PREREQS=1
check_cmd curl      || FAILED_PREREQS=1
check_cmd ss        || FAILED_PREREQS=1
check_cmd nohup     || FAILED_PREREQS=1
[ "${FAILED_PREREQS}" -eq 0 ] || exit 1

PYTHON_VERSION="$(python3 -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")')"
PYTHON_MAJOR="$(echo "${PYTHON_VERSION}" | cut -d. -f1)"
PYTHON_MINOR="$(echo "${PYTHON_VERSION}" | cut -d. -f2)"
if [ "${PYTHON_MAJOR}" -lt 3 ] || { [ "${PYTHON_MAJOR}" -eq 3 ] && [ "${PYTHON_MINOR}" -lt 13 ]; }; then
  error "Python >= 3.13 is required (found ${PYTHON_VERSION})"
  exit 1
fi
info "  python ${PYTHON_VERSION}, uv $(uv --version | awk '{print $2}'), node $(node --version), npm $(npm --version)"

# ---------------------------------------------------------------------------
# 2. Check ports are free
# ---------------------------------------------------------------------------
info "Checking ports (backend=${BACKEND_PORT}, frontend=${FRONTEND_PORT})..."

if port_busy "${BACKEND_PORT}"; then
  error "Port ${BACKEND_PORT} is already in use. Run webui/scripts/stop.sh first, or pick a different port in ${FRONTEND_DIR}/vite.config.ts."
  exit 1
fi
if port_busy "${FRONTEND_PORT}"; then
  error "Port ${FRONTEND_PORT} is already in use. Run webui/scripts/stop.sh first."
  exit 1
fi

# ---------------------------------------------------------------------------
# 3. Backend setup
# ---------------------------------------------------------------------------
info "Installing backend dependencies (uv sync)..."
(cd "${BACKEND_DIR}" && uv sync --quiet)

info "Starting backend on port ${BACKEND_PORT}..."
(
  cd "${BACKEND_DIR}"
  nohup uv run uvicorn app.main:app --reload --host 127.0.0.1 --port "${BACKEND_PORT}" \
    > "${REPO_ROOT}/${BACKEND_LOG_FILE}" 2>&1 &
  echo $!
) > "${REPO_ROOT}/${BACKEND_PID_FILE}"
BACKEND_PID="$(cat "${REPO_ROOT}/${BACKEND_PID_FILE}")"
info "  backend PID ${BACKEND_PID}, log: ${BACKEND_LOG_FILE}"

# Wait for backend to accept connections (up to 20s).
info "Waiting for backend health check..."
for i in $(seq 1 40); do
  if curl -sf "http://127.0.0.1:${BACKEND_PORT}/api/health" >/dev/null 2>&1; then
    info "  backend is up"
    break
  fi
  if ! kill -0 "${BACKEND_PID}" 2>/dev/null; then
    error "Backend process exited before becoming healthy. Check ${BACKEND_LOG_FILE}:"
    tail -n 20 "${REPO_ROOT}/${BACKEND_LOG_FILE}" >&2 || true
    rm -f "${REPO_ROOT}/${BACKEND_PID_FILE}"
    exit 2
  fi
  sleep 0.5
done
if ! curl -sf "http://127.0.0.1:${BACKEND_PORT}/api/health" >/dev/null 2>&1; then
  error "Backend did not become healthy within 20s. Check ${BACKEND_LOG_FILE}."
  exit 2
fi

# ---------------------------------------------------------------------------
# 4. Frontend setup
# ---------------------------------------------------------------------------
info "Installing frontend dependencies (npm install)..."
(cd "${FRONTEND_DIR}" && npm install --silent --no-audit --no-fund >/dev/null)

info "Starting frontend on port ${FRONTEND_PORT}..."
(
  cd "${FRONTEND_DIR}"
  # Run vite directly rather than `npm run dev` so the PID we save is the
  # actual node process holding the port. npm wraps the script in a parent
  # process that doesn't forward SIGTERM to its child, which previously
  # left orphaned node processes behind on stop.
  nohup ./node_modules/.bin/vite > "${REPO_ROOT}/${FRONTEND_LOG_FILE}" 2>&1 &
  echo $!
) > "${REPO_ROOT}/${FRONTEND_PID_FILE}"
FRONTEND_PID="$(cat "${REPO_ROOT}/${FRONTEND_PID_FILE}")"
info "  frontend PID ${FRONTEND_PID}, log: ${FRONTEND_LOG_FILE}"

# Wait for frontend to accept connections (up to 30s).
info "Waiting for frontend to be ready..."
for i in $(seq 1 60); do
  if curl -sf "http://localhost:${FRONTEND_PORT}" >/dev/null 2>&1; then
    info "  frontend is up"
    break
  fi
  if ! kill -0 "${FRONTEND_PID}" 2>/dev/null; then
    error "Frontend process exited before becoming healthy. Check ${FRONTEND_LOG_FILE}:"
    tail -n 20 "${REPO_ROOT}/${FRONTEND_LOG_FILE}" >&2 || true
    rm -f "${REPO_ROOT}/${FRONTEND_PID_FILE}"
    exit 3
  fi
  sleep 0.5
done
if ! curl -sf "http://localhost:${FRONTEND_PORT}" >/dev/null 2>&1; then
  error "Frontend did not become ready within 30s. Check ${FRONTEND_LOG_FILE}."
  exit 3
fi

# ---------------------------------------------------------------------------
# 5. Smoke test — proxy should forward /api to the backend
# ---------------------------------------------------------------------------
info "Smoke test (frontend -> backend proxy)..."
HEALTH="$(curl -sf "http://localhost:${FRONTEND_PORT}/api/health" || echo "")"
if [ -z "${HEALTH}" ]; then
  warn "Frontend is up but /api proxy is not forwarding to the backend. Check vite.config.ts."
else
  info "  /api/health via proxy -> ${HEALTH}"
fi

# ---------------------------------------------------------------------------
# Done
# ---------------------------------------------------------------------------
echo ""
printf "%s✓ DAL web UI is running%s\n" "${GREEN}" "${NC}"
echo ""
echo "  Frontend:  http://localhost:${FRONTEND_PORT}"
echo "  Backend:   http://127.0.0.1:${BACKEND_PORT}"
echo "  API docs:  http://127.0.0.1:${BACKEND_PORT}/docs"
echo "  Backend:   PID ${BACKEND_PID}"
echo "  Frontend:  PID ${FRONTEND_PID}"
echo ""
echo "To stop:     ./webui/scripts/stop.sh"
echo "Logs:        ${BACKEND_LOG_FILE}, ${FRONTEND_LOG_FILE}"
