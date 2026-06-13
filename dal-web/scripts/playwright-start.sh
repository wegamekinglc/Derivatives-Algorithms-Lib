#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

STARTED=false

cleanup() {
  if [ "${STARTED}" = true ]; then
    "${REPO_ROOT}/dal-web/scripts/stop.sh" --force >/dev/null 2>&1 || true
  fi
}

trap cleanup EXIT INT TERM

"${REPO_ROOT}/dal-web/scripts/start.sh"
STARTED=true

# Keep the process alive so Playwright's webServer doesn't exit.
# `sleep infinity` is not portable; use a long-sleep loop instead.
while true; do sleep 86400; done
