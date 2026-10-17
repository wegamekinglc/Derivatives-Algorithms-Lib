#!/usr/bin/env bash
set -euo pipefail

# Arguments: compiler, runtime search directory, log directory, stock|instrumented.
cxx=$(command -v "$1")
cxx=$(realpath "$cxx")
runtime=$(realpath "$2")
mkdir -p "$3"
output=$(realpath "$3")
kind=$4
sources=$(cd -- "$(dirname -- "$0")" && pwd)

identify() {
    local binary=$1
    env LD_LIBRARY_PATH="$runtime" ldd "$binary" | tee "$binary.ldd"
    local name library
    for name in libstdc++.so.6 libtsan.so.2; do
        library=$(awk -v name="$name" '$1 == name {print $3}' "$binary.ldd")
        test -f "$library"
        realpath "$library"
        sha256sum "$library"
        readelf -n "$library"
    done
    library=$(awk '$1 == "libstdc++.so.6" {print $3}' "$binary.ldd")
    test "$(realpath "$library")" = "$(realpath "$runtime/libstdc++.so.6")"
    library=$(awk '$1 == "libtsan.so.2" {print $3}' "$binary.ldd")
    test "$(realpath "$library")" = "$(realpath "$("$cxx" -print-file-name=libtsan.so.2)")"
}

compile() {
    set -x
    "$cxx" -std=c++17 -O2 -g -fsanitize=thread -fno-omit-frame-pointer \
        -pthread "$sources/$1.cpp" -o "$output/$1" \
        "-L$runtime" "-Wl,-rpath,$runtime"
    set +x
    identify "$output/$1"
}

check_run() {
    local name=$1 expected=$2
    shift 2
    local status=0
    timeout 30s env LD_LIBRARY_PATH="$runtime" TSAN_OPTIONS=halt_on_error=0:second_deadlock_stack=1 \
        "$@" > "$output/$name.log" 2>&1 || status=$?
    cat "$output/$name.log"
    echo "$name exit=$status expected=$expected"
    if grep -q 'FATAL: ThreadSanitizer' "$output/$name.log"; then
        return 1
    fi
    test "$status" -eq "$expected"
    if [ "$expected" -eq 0 ]; then
        if grep -q 'WARNING: ThreadSanitizer' "$output/$name.log"; then
            return 1
        fi
        grep -q 'messageSize=256' "$output/$name.log"
    else
        grep -q 'WARNING: ThreadSanitizer: data race' "$output/$name.log"
    fi
}

"$cxx" --version
sha256sum "$cxx" "$sources/exception_lifetime_repro.cpp" "$sources/real_race.cpp"
compile exception_lifetime_repro
expected=0
if [ "$kind" = stock ]; then
    expected=66
else
    test "$kind" = instrumented
fi
check_run relaxed "$expected" "$output/exception_lifetime_repro"
check_run synchronized 0 "$output/exception_lifetime_repro" synchronized
compile real_race
check_run real-race 66 "$output/real_race"
