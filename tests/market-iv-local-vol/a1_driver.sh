#!/usr/bin/env bash
set -euo pipefail
repository=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)
cd -- "$repository"
mkdir -p build
root=$(mktemp -d "$repository/build/.a1-process-XXXXXXXX")
chmod 700 "$root"
mkfifo "$root/queue"
export DAL_A1_BROKER_DIR="$root"
exec 8<>"$root/queue"
/bin/bash tests/market-iv-local-vol/a1_broker.sh "$root" &
broker_pid=$!
finish() {
    trap - EXIT INT TERM
    printf 'STOP\n' >&8
    wait "$broker_pid"
    exec 8>&-
}
trap finish EXIT INT TERM
while [[ ! -f "$root/ready" ]]; do sleep .02; done
result=0
if [[ "${1:-}" == test ]]; then
    OPENBLAS_NUM_THREADS=1 python3 -m unittest discover \
        -s tests/market-iv-local-vol -p test_a1.py -v || result=$?
else
    OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 \
        python3 tests/market-iv-local-vol/a1_run.py "$@" || result=$?
fi
exit "$result"
