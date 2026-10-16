# Historical overlay input fixture

`native-05a24acd.json.gz` contains the four complete UTF-8 source files exported
with `git show 05a24acd3457e0d42ce396f02d88572872d14095:<path>`.
Its deterministic gzip payload records that commit and the original path/content
mapping. `native_fixture.py` verifies every complete source against the unchanged
`lsmc_storage_patch.SOURCE_HASHES` before returning it.

That tool commit uses the native LSMC implementation from the training-budget
study's `582b1fe5f99a0a07480702a54a9e2d7756a82663` baseline. The fixture tests
the historical address and binary32/binary64 overlays, including UTF-8/newline
handling, without relying on the live checkout still matching that baseline.
Current checkpoint production sources are outside those overlays' support domain;
separate tests require their rejection. No source admission rule is relaxed and
no historical numerical or performance result is extended to the current code.
