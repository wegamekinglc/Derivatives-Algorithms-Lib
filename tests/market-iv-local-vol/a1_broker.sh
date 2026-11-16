#!/usr/bin/env bash
set -euo pipefail
root=$1
repository=$(pwd -P)
watchers=()
jobs=()

run_job() {
    local folder=$1 native_pid status=125
    trap 'printf "%s\n" "$status" > "$folder/status.next"; mv -- "$folder/status.next" "$folder/status"' EXIT
    /bin/bash "$repository/tests/market-iv-local-vol/a1_dispatch.sh" \
        < "$folder/request" > "$folder/stdout" 2> "$folder/stderr" &
    native_pid=$!
    cat "/proc/$native_pid/stat" > "$folder/start.stat" 2>/dev/null || true
    printf '%s\n' "$native_pid" > "$folder/pid.next"
    mv -- "$folder/pid.next" "$folder/pid"
    status=0
    wait "$native_pid" || status=$?
    exit "$status"
}

exec 3<>"$root/queue"
printf '%s\n' "$BASHPID" > "$root/broker.pid"
touch "$root/ready"
while IFS= read -r folder <&3; do
    [[ "$folder" != STOP ]] || break
    case "$folder" in "$root"/job-*) ;; *) exit 2 ;; esac
    jobs+=("$folder")
    run_job "$folder" &
    watchers+=("$!")
done
for folder in "${jobs[@]}"; do
    while [[ ! -f "$folder/pid" && ! -f "$folder/status" ]]; do sleep .02; done
done
cleanup_status=0
if "$MULTICA_CLI_PATH" daemon status --output json > "$root/daemon-status.json"; then
    python3 "$repository/tests/market-iv-local-vol/a1_process.py" "$root" || cleanup_status=$?
else
    cleanup_status=$?
fi
for watcher in "${watchers[@]}"; do wait "$watcher" || true; done
exit "$cleanup_status"
