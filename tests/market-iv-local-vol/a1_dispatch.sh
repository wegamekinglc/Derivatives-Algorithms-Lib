#!/usr/bin/env bash
set -euo pipefail

# Typed job data is read from the private broker, never evaluated as code.
IFS= read -r -d '' operation
IFS= read -r -d '' environment_count
[[ "$environment_count" == 0 ]]
IFS= read -r -d '' argument_count
[[ "$argument_count" =~ ^[0-9]+$ && "$argument_count" -le 6 ]]
arguments=()
for ((index = 0; index < argument_count; index++)); do
    IFS= read -r -d '' argument
    arguments+=("$argument")
done

require_build_path() {
    case "$1" in
        "$PWD/build/"*) ;;
        *) return 2 ;;
    esac
    [[ "$1" != */../* && "$1" != */./* ]]
}

case "$operation" in
    git)
        [[ "${arguments[0]}" == -C && "${arguments[1]}" == "$PWD" ]]
        case "${arguments[*]:2}" in
            'status --porcelain') exec /usr/bin/git -C . status --porcelain ;;
            'rev-parse HEAD') exec /usr/bin/git -C . rev-parse HEAD ;;
            'submodule status --recursive') exec /usr/bin/git -C . submodule status --recursive ;;
            *) exit 2 ;;
        esac
        ;;
    compiler)
        [[ "${arguments[*]}" == --version ]]
        exec /usr/bin/c++ --version
        ;;
    multica)
        [[ "${arguments[*]}" == 'daemon status --output json' ]]
        [[ "${MULTICA_CLI_PATH:-}" == /* && -x "$MULTICA_CLI_PATH" ]]
        case "${MULTICA_CLI_PATH##*/}" in multica|multica.exe) ;; *) exit 2 ;; esac
        exec "$MULTICA_CLI_PATH" daemon status --output json
        ;;
    capture)
        [[ "$argument_count" == 3 && "${arguments[2]}" == --full-precision ]]
        require_build_path "${arguments[0]}"
        require_build_path "${arguments[1]}"
        [[ "${arguments[0]##*/}" == request.json && "${arguments[1]##*/}" == capture.json ]]
        exec /usr/bin/env -i PATH=/usr/bin:/bin LC_ALL=C OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 ./build/dal-542-reference/dal_market_iv_capture "${arguments[@]}"
        ;;
    probe)
        [[ "$argument_count" == 1 && "${arguments[0]##*/}" == probe-input.txt ]]
        require_build_path "${arguments[0]}"
        exec /usr/bin/env -i PATH=/usr/bin:/bin LC_ALL=C OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 ./build/dal-542-reference/dal_market_iv_a1_probe "${arguments[@]}"
        ;;
    mc)
        [[ "$argument_count" == 4 || "$argument_count" == 5 ]]
        case "${arguments[2]}" in F2_linear|F2_nonlinear) ;; *) exit 2 ;; esac
        case "${arguments[3]}" in 16|17|18|19|20) ;; *) exit 2 ;; esac
        require_build_path "${arguments[0]}"
        require_build_path "${arguments[1]}"
        if [[ "$argument_count" == 5 ]]; then require_build_path "${arguments[4]}"; fi
        exec /usr/bin/env -i PATH=/usr/bin:/bin LC_ALL=C OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 ./build/dal-542-reference/dal_market_iv_a1_mc "${arguments[@]}"
        ;;
    *) exit 2 ;;
esac
