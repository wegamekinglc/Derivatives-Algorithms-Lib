#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

cleanup() {
  "${REPO_ROOT}/dal-web/scripts/stop.sh" --force >/dev/null 2>&1 || true
}

trap cleanup EXIT INT TERM

"${REPO_ROOT}/dal-web/scripts/start.sh"

sleep infinity
