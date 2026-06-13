#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

CHROME_ROOT="${REPO_ROOT}/chrome"
LIB_ROOT="${REPO_ROOT}/chrome-libs/extract"
LIB_DIR="${LIB_ROOT}/usr/lib/x86_64-linux-gnu"

find_chrome_binary() {
  # Sort by version (-V) so chrome/linux-<version> dirs pick the newest, not
  # the lexically-last, then take that newest path.
  find "${CHROME_ROOT}" -path '*/chrome-linux64/chrome' -type f 2>/dev/null | sort -V | tail -n 1
}

CHROME_BINARY="$(find_chrome_binary)"

if [ ! -x "${CHROME_BINARY}" ]; then
  echo "Installing Chrome for Playwright..."
  (cd "${REPO_ROOT}" && npx --yes @puppeteer/browsers install chrome@stable)
  CHROME_BINARY="$(find_chrome_binary)"
else
  echo "Chrome already installed: ${CHROME_BINARY}"
fi

if [ ! -x "${CHROME_BINARY}" ]; then
  echo "error: Chrome installation did not produce an executable under ${CHROME_ROOT}" >&2
  exit 1
fi

if [ ! -f "${LIB_DIR}/libnspr4.so" ] || [ ! -f "${LIB_DIR}/libnss3.so" ]; then
  echo "Installing browser runtime libraries..."
  TMP_DIR="$(mktemp -d)"
  trap 'rm -rf "${TMP_DIR}"' EXIT
  (cd "${TMP_DIR}" && apt-get download libnspr4 libnss3)
  mkdir -p "${LIB_ROOT}"
  for deb in "${TMP_DIR}"/*.deb; do
    dpkg-deb -x "${deb}" "${LIB_ROOT}"
  done
else
  echo "Browser runtime libraries already installed: ${LIB_DIR}"
fi

echo "Playwright setup complete."
