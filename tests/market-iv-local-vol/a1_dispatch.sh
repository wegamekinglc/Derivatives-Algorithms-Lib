#!/usr/bin/env bash
set -euo pipefail

# The inherited private descriptor carries data and stdio capabilities only.
control_fd=
for descriptor in /proc/$$/fd/[0-9]*; do
    target=$(readlink -- "$descriptor" 2>/dev/null || true)
    if [[ "$target" == '/memfd:dal-a1-job (deleted)' ]]; then
        [[ -z "$control_fd" ]]
        control_fd=${descriptor##*/}
    fi
done
[[ "$control_fd" =~ ^[0-9]+$ ]]
exec 0<&"$control_fd"
IFS= read -r -d '' output_fd
IFS= read -r -d '' error_fd
[[ "$output_fd" =~ ^[0-9]+$ && "$error_fd" =~ ^[0-9]+$ ]]
[[ "$output_fd" -gt 2 && "$error_fd" -gt 2 ]]
exec 1>&"$output_fd" 2>&"$error_fd"
IFS= read -r -d '' operation
IFS= read -r -d '' environment_count
[[ "$environment_count" =~ ^[0-9]+$ && "$environment_count" -le 64 ]]
for ((index = 0; index < environment_count; index++)); do
    IFS= read -r -d '' key
    IFS= read -r -d '' value
    [[ "$operation" == multica ]]
    case "$key" in
        HOME|USER|MULTICA_*) export "$key=$value" ;;
        *) exit 2 ;;
    esac
done
IFS= read -r -d '' argument_count
[[ "$argument_count" =~ ^[0-9]+$ && "$argument_count" -le 6 ]]
arguments=()
for ((index = 0; index < argument_count; index++)); do
    IFS= read -r -d '' argument
    arguments+=("$argument")
done
exec 0</dev/null
exec {control_fd}<&-
exec {output_fd}>&-
if [[ "$error_fd" != "$output_fd" ]]; then exec {error_fd}>&-; fi

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
        exec ./build/dal-542-reference/dal_market_iv_capture "${arguments[@]}"
        ;;
    probe)
        [[ "$argument_count" == 1 && "${arguments[0]##*/}" == probe-input.txt ]]
        require_build_path "${arguments[0]}"
        exec ./build/dal-542-reference/dal_market_iv_a1_probe "${arguments[@]}"
        ;;
    mc)
        [[ "$argument_count" == 4 || "$argument_count" == 5 ]]
        case "${arguments[2]}" in F2_linear|F2_nonlinear) ;; *) exit 2 ;; esac
        case "${arguments[3]}" in 16|17|18|19|20) ;; *) exit 2 ;; esac
        require_build_path "${arguments[0]}"
        require_build_path "${arguments[1]}"
        if [[ "$argument_count" == 5 ]]; then require_build_path "${arguments[4]}"; fi
        exec ./build/dal-542-reference/dal_market_iv_a1_mc "${arguments[@]}"
        ;;
    *) exit 2 ;;
esac
