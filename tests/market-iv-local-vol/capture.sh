#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 || $# -gt 3 ]]; then
    echo 'usage: bash capture.sh /absolute/capture-executable /absolute/evidence-directory [jobs]' >&2
    exit 1
fi
capture_executable="$1"
evidence="$2"
jobs="${3:-4}"
script_directory="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repository="$(cd -- "$script_directory/../.." && pwd)"
mkdir -p -- "$evidence"
python3 "$script_directory/generate.py" "$evidence/request.json"
capture_status=0
"$capture_executable" "$evidence/request.json" "$evidence/capture.json" > "$evidence/capture.log" 2>&1 || capture_status=$?
if [[ "$capture_status" != 0 && "$capture_status" != 2 ]]; then
    exit "$capture_status"
fi
printf '%s\n' "$capture_status" > "$evidence/capture-exit.txt"
git -C "$repository" rev-parse HEAD > "$evidence/source-head.txt"
git -C "$repository" rev-parse HEAD:dal-cpp/dal > "$evidence/production-tree.txt"
git -C "$repository" diff HEAD -- dal-cpp/dal > "$evidence/production-diff.txt"
git -C "$repository" submodule status > "$evidence/submodules.txt"
c++ --version > "$evidence/compiler.txt"
OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 python3 "$script_directory/run.py" \
    --capture "$capture_executable" --output "$evidence" --jobs "$jobs"
