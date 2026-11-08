# DAL-502 Eigen checkout origin metadata repair

Baseline: `fbb899fa72fc232d4f81b2e4a604c1594db44698`, ready PR #685.
Implementation: `12cb20b4ccf9ed321b8908c76fde935d2cae019f`.
Scope: checkout helper Git metadata, its direct contracts and this evidence.

## Observed failure and minimal repair

The successful GCC 15 job `128701875529` ends with this checkout cleanup:

```bash
git submodule foreach --recursive git config --local --show-origin --name-only --get-regexp remote.origin.url
```

Its log enters `dal-cpp/externals/eigen`, reports a nonzero submodule command
and warns that Git exited 128. Fetching by URL into a manually initialized
repository does not create `remote.origin.url`. An unmatched local config
query exits 1; the recursive foreach then exits 128. Offline fixtures reproduce
the exact command and diagnostic. Existing checkouts with a stale origin pass
the presence query but retain incorrect source metadata before this repair.

After the first successful primary/mirror fetch, the helper now runs:

```bash
git config --local remote.origin.url <successful-fetch-URL>
```

This single helper line supplies origin for a fresh or previously initialized
checkout and updates a stale origin to the source actually used. Fetching still
uses the same primary then mirror URLs and the exact gitlink pin; detached
checkout and final HEAD validation are unchanged. The metadata write is outside
the fetch fallback handler, so a config failure is fatal and does not trigger
another download. No remote refspec, global config, dependency pin, workflow,
wheel helper, product source or numerical behavior changes.

## RED/GREEN and direct coverage

The new fixtures create a parent repository with a real Eigen gitlink and local
primary/mirror Git repositories. Their pinned commit is followed by a later
commit to ensure checkout selects the gitlink rather than the remote tip.
Only constants naming these sources are replaced; Git init/fetch/detach/config,
gitlink parsing and recursive cleanup run as actual subprocesses. The fixture
allows only `file` transport and isolates global/system Git configuration.

Six paths cover new, existing-without-origin and existing-with-stale-origin
checkouts, each through primary success and primary-failure/mirror success.
They assert the exact recursive cleanup exits 0, local origin names the
successful source, HEAD equals the parent gitlink, HEAD is detached, payload
bytes match the pinned revision, the parent remains clean and fetches use the
same pin. Ordinary helper failure contracts remain enabled: malformed gitlinks,
both fetches failing and final HEAD mismatch. A new contract proves metadata
write failure propagates after one successful fetch without contacting mirror.

On the old helper, **11 tests produced seven failing subcases/assertions**:
four missing-origin cases reproduced cleanup exit 128, two existing stale-origin
cases retained the wrong source, and metadata-failure propagation was absent.
After the one-line change, **11/11 pass on CPython 3.9.25 and 3.13.9**. Existing
tests and their assertions are retained. CCN<=8 and whitespace checks pass.

Commands (all dependency operations are offline local Git fixtures):

```bash
uv run --offline --isolated --no-project --python 3.9 python \
  -m unittest discover -s .github/scripts/tests \
  -p test_checkout_dependencies.py -v
python3 -m unittest discover -s .github/scripts/tests \
  -p test_checkout_dependencies.py -v
lizard -C 8 -w .github/scripts/checkout_dependencies.py \
  .github/scripts/tests/test_checkout_dependencies.py
git diff --check
```

## Identity, reuse and limits

[identity.json](dal502-checkout-origin/identity.json) binds source/test hashes,
both interpreter executables, Git 2.53.0 and submodule helper hashes, the original
GCC job log hash, the unchanged Eigen constants and repository gitlink
`3147391d946bb4b6c68edd901f2add6ac1f31f8c`, fixture pin and RED/GREEN log hashes.
The [raw archive manifest](dal502-checkout-origin/raw-archive.json) identifies
the before/after sources, real failure excerpt, full local logs, protected tree
snapshot and identity collection script.

All **3,273 protected tracked entries** (modes/blobs/gitlinks) match baseline
across `dal-cpp`, `dal-public`, `dal-python`, `dal-excel`, `.gitmodules`, all
workflows and the reviewed wheel helper/tests. Runtime submodule identities are
also recorded without updating or fetching them. Existing N1–N12's 12 PASS and
accepted production/method/product/performance/wheel evidence remain applicable;
none of those matrices or network downloads were rerun. This fixes Git metadata
used by shared CI checkout callers, so numerical/performance checks add no
coverage for the changed behavior. Real Windows cleanup remains an actual-CI
gate; Linux fixture results are not claimed as Windows execution.

New-head CI is captured separately after push without waiting. PR #685 remains
ready with auto merge disabled. DAL-502 and DAL-497 stay in progress; coordinator
review and actual S3 merge remain prerequisites for S4.
