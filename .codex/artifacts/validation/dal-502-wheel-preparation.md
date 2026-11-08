# DAL-502 wheel environment preparation repair

Baseline: ready PR #685, `f51471f224d2cd3c37f04b28e49e948174b55bdc`.
Implementation: `ba8f909125da683d2afe1ae024f37c6ab7dd7286`.
This change covers wheel bootstrap acquisition and its direct contracts.
S3's accepted N1–N12 result remains 12 PASS; production/numerical sources,
reference values, product tests and performance evidence are reused unchanged.

## Failure and pinned upstream contract

Run `42466469746` failed in macos-arm64 job `128687095702` and windows-amd64
job `128687095748`. Both tracebacks end with HTTP 504 while fetching
`get-virtualenv` 21.14.13. The native before-all preparation had run, but the
corresponding Python wheel build/test had not started. No compiler or test
failure is converted into a successful result.

The action pin stays `f9b966cfadf4109556cadc5bb39ddb72d0ce664e` (4.3.5).
Its [virtualenv selector](https://github.com/pypa/cibuildwheel/blob/f9b966cfadf4109556cadc5bb39ddb72d0ce664e/cibuildwheel/venv.py)
uses `CIBW_CACHE_PATH/virtualenv-<version>.pyz`, and trusts a file already at
that path. The [resource](https://github.com/pypa/cibuildwheel/blob/f9b966cfadf4109556cadc5bb39ddb72d0ce664e/cibuildwheel/resources/virtualenv.toml)
fixes version 21.14.13 and SHA256
`215e29d6c8e2a4bf5f509828368629bbe7eddb847dcfee862db0f84e6df40047`.
Its [downloader](https://github.com/pypa/cibuildwheel/blob/f9b966cfadf4109556cadc5bb39ddb72d0ce664e/cibuildwheel/util/file.py)
already has six exponential-backoff attempts. Another retry around the whole
wheel action would repeat unrelated work and could obscure build/test errors.

The official release metadata identifies the same binary as asset `666956820`.
The API download with `Accept: application/octet-stream` returns 12,474,880
bytes with exactly that SHA256. This alternate official route supplies the
same immutable content; it does not change the dependency version or digest.
Source snapshots, metadata URLs and their hashes are bound in
[identity.json](dal502-wheel-preparation/identity.json).

## Change and failure semantics

`.github/scripts/prepare_wheel_virtualenv.py` performs preparation before the
pinned action on macOS and Windows, in both CI and release workflows. Linux
keeps its existing preparation path. The helper verifies an existing cache
entry and otherwise downloads into a temporary file in the destination
directory. Only bytes passing the original SHA256 check are atomically renamed
to the expected filename; interrupted/unverified temporary files are removed.

Transient failures alternate between the canonical release URL and the
official asset API. At most six attempts are made, with 30-second socket
timeouts and delays 3/6/12/24/48 seconds. Retries cover the declared transient
HTTP/network failures. Permanent HTTP errors, certificate verification errors,
checksum failures and ordinary file errors propagate. A corrupt existing
cache is rejected; it is neither trusted nor relabeled. The timeout applies
to socket operations, not a claimed hard deadline for an entire transfer.

After success, the helper appends the resolved `CIBW_CACHE_PATH` to the step's
UTF-8 `GITHUB_ENV` file. Subsequent action processes therefore consume the
verified entry. It uses only Python 3.9-compatible standard-library APIs;
no new action/dependency version or wheel frontend is introduced.

Both workflow matrices, pinned actions, CPython selectors, existing helper
checks, wheel build/test steps, smoke checks, artifact checks and publishing
gates remain unchanged. The shared release preparation is included because
it invokes the same action and bootstrap path. No `continue-on-error`,
platform suppression or build/test retry is added.

## RED/GREEN and applicability checks

The initial single-acquisition preparation failed the new transient-recovery
and retry-exhaustion contracts (one ERROR, one FAIL); permanent-error and
checksum protection already passed. After the bounded recovery change,
all four passed without weakening their assertions. Final coverage is **11/11**:
transient 504 recovery, exhausted retries/final exception, permanent errors,
download/cache checksum rejection, valid-cache reuse, socket/certificate
behavior, interrupted-body cleanup, verified cache export, failed-export
protection and CI/release wiring to the same pin.

Actual commands:

```bash
uv run --offline --isolated --no-project --python 3.9 python \
  -m unittest discover -s .github/scripts/tests \
  -p test_prepare_wheel_virtualenv.py -v
uv run --offline --isolated --no-project --python 3.9 python \
  .github/scripts/check_dal_python_syntax.py
python3 -m unittest discover -s .github/scripts/tests \
  -p 'test_python_helpers*.py' -v
```

CPython 3.9.25: 11/11 preparation tests and all 157 package syntax files pass.
Existing helpers: 11 PASS and six Windows/pwsh/.NET-only skips, with no failure.
Those executable Windows checks remain enabled in the Windows CI job; local
skips are not claimed as Windows execution evidence.

A macOS-style aliased temporary directory also passes all 11 tests under
Python 3.9. Its first run exposed three lexical-versus-resolved path assertions;
the test assertions now compare canonical paths without changing the download
helper. Both RED and GREEN logs are retained.

The real helper CLI then encountered an actual canonical-URL HTTP 504 and
recovered through the official API route. The resulting 12,474,880-byte file
passes the pinned checksum, and the exported environment points at its cache.
The saved logs retain this recovery, rather than reporting a simulated network
event as real.

An isolated harness executes the exact cache-path assignment and selector
functions from the pinned upstream source, with real FileLock/Version and a
network function that fails if called. Both CPython 3.9 and 3.14 select the
prefilled 21.14.13 file without another download. This is a cache-contract
check, not a local macOS/Windows wheel build.

Parsed YAML comparisons verify both matrices and each original build/test
step are identical to baseline; removing the new preparation step reproduces
the original wheel-step lists exactly. Local CCN<=8, whitespace and Markdown
checks pass. No product, numerical, full-wheel or performance matrix is rerun.

## Identity and handoff

The identity record binds helper/tests/workflows, interpreter binaries and
versions, live archive, pinned upstream source and baseline failure-log hashes.
The protected product-source diff is empty. Raw logs, contracts and replay
scripts are delivered in the [archive manifest](dal502-wheel-preparation/raw-archive.json).
Pinned upstream source files and the binary are referenced by immutable URLs
and hashes; the replay script fetches and verifies source snapshots when needed.

At the baseline snapshot, the only failed checks were the two documented wheel
download jobs. New-head CI is reported separately after push. The PR stays ready
without auto merge; the coordinator owns focused CI review and final gates.
DAL-502 and DAL-497 remain in progress, and S4 waits for S3's actual merge.

## Follow-up: Python 3.9 response-body socket timeout

Review baseline: `e6a07eafba78b1d49f907e90b6125a4d006a51bf`.
Fix: `29f030e070e52863207fedf49db80e871c89cf0c`.
The original 11 contracts used built-in `TimeoutError` at connection setup;
they did not establish Python 3.9 response-body `socket.timeout` recovery.
The reviewer's offline script reproduces this gap with the actual runtime
exception raised from `read()` after partial bytes have been copied.

On CPython 3.9.25, `socket.timeout` is a separate `OSError` subclass and is
neither `TimeoutError` nor `ConnectionError`. It escaped after one attempt.
On the same baseline, CPython 3.13.9 aliases it to `TimeoutError` and retries
successfully. This is a version-specific exception-class gap, not a checksum,
cache-path, upstream asset or wheel-build failure.

The only helper changes import `socket` and explicitly include `socket.timeout`
in the existing exception tuple. No broad `OSError` handler is introduced.
The existing classification, attempt limit, delay sequence, official URL/asset,
SHA, cache verification/atomic replacement and environment export stay intact.
Both workflows are byte-identical to the reviewed baseline. Ordinary file
errors, certificates, permanent HTTP and checksum failures retain fail-fast
semantics.

The reviewer's unchanged script first produced **1 FAIL** on 3.9 (one request)
and **1 PASS** on 3.13 (two requests). New contracts then produced **1 ERROR**
for recovery and **1 FAIL** for exhaustion on unmodified 3.9 helper code;
the other 12 contracts passed. After the fix, the reviewer script passes on
both interpreters and the complete affected suite is **14/14** on each.
Python 3.9 also passes 14/14 with the existing aliased temporary directory.

New coverage uses each interpreter's `socket.timeout` from a partial response
body, not a built-in timeout substitute. Recovery checks cleanup before
backoff, response closure, fallback route, verified final bytes and successful
cache export. Exhaustion checks all six requests, 3/6/12/24/48 delays, cleanup
after each attempt, propagation of the exact last exception and unchanged
`GITHUB_ENV`. Plain `OSError` and `PermissionError` during final file replacement
also propagate after one acquisition without retry/export and remove the
temporary file. Existing certificate/permanent-HTTP/checksum contracts pass.

Commands (all network operations mocked; sleeps patched):

```bash
# Run before and after the helper change; retain both outputs.
uv run --offline --isolated --no-project --python 3.9 python \
  ../attachments/dal502-wheel-review/wheel_socket_timeout_regression.py \
  .github/scripts/prepare_wheel_virtualenv.py
python3 ../attachments/dal502-wheel-review/wheel_socket_timeout_regression.py \
  .github/scripts/prepare_wheel_virtualenv.py
uv run --offline --isolated --no-project --python 3.9 python \
  -m unittest discover -s .github/scripts/tests \
  -p test_prepare_wheel_virtualenv.py -v
python3 -m unittest discover -s .github/scripts/tests \
  -p test_prepare_wheel_virtualenv.py -v
TMPDIR="$PWD/build/dal502-wheel-prepare/tmp-alias" \
  uv run --offline --isolated --no-project --python 3.9 python \
  -m unittest discover -s .github/scripts/tests \
  -p test_prepare_wheel_virtualenv.py -v
lizard -C 8 -w .github/scripts/prepare_wheel_virtualenv.py \
  .github/scripts/tests/test_prepare_wheel_virtualenv.py
git diff --check
```

CCN<=8 and whitespace checks pass. Interpreter executables, `socket.py`,
the `_socket` binary/built-in origin, source/workflow hashes and reviewer inputs
are bound in [socket-timeout-identity.json](dal502-wheel-preparation/socket-timeout-identity.json).
The helper SHA changes from `4e2340c2…00481d9` to `28ac5ac7…7f18fe6`.
The [follow-up archive manifest](dal502-wheel-preparation/socket-timeout-raw-archive.json)
binds original reviewer inputs, RED/GREEN logs, before/after sources and the
identity collection script. New-head CI is captured separately at handoff.
The baseline snapshot has 19 success, 14 pending and no failed check among 33.

The previous verified official download and upstream selector evidence remain
applicable to their unchanged paths; no new download or selector run is claimed.
Production/numerical/product/performance evidence and N1–N12's 12 PASS remain
applicable because their source/configuration diff is empty. No such matrices,
package-syntax suite, full wheel build or Windows-only helper checks are rerun.
This follow-up is ready for the coordinator's targeted P2 review; it does not
declare PR gates complete or authorize merge.
