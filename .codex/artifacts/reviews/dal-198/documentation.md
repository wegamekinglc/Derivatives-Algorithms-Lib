DAL-198 F1 documentation handoff, updated 2026-09-13. This record controls the pending
independent review; retire it after delivery.

Reconciled documentation against source and tests at
`83e7f5a8ae2af3d1bc2930ac07ea9ffdcb0451a3`, the accepted F1 issue and frontend
design/critique/API boundaries, and the implementation/testing evidence.

Published documentation for F1:

- `docs/methodology/script_engine.md` described `Tokenize` as the sole string
  tokenizer and omitted FIX. It now describes positioned `Lex` tokens, the
  legacy string projection, complete index literals, preserved bracket content
  and delivery suffixes, protected macro/placeholder expansion, strict optional
  fixing dates, reserved FIX names, NodeFix metadata, and source origins. The
  atom/precedence table includes `NodeFix_`.
- The guide explicitly limits FIX to parsing and AST inspection. All product
  execution entries reject it with `PreparationRequired`, including dead
  branches; no fixing lookup, model binding or public named-pricing API is
  documented as available. Omitted dates remain optional AST metadata.
- `docs/methodology/index_parsing.md` incorrectly described bare names as
  matching nothing and parser input as only the remainder. It now describes
  complete-string parsing, InvalidIndex/UnknownIndex failures, complete EQ/FX
  validation, bracket content and delivery validation, and the script limit.
- `docs/README.md` gains a FIX syntax/limit entry under the existing methodology
  note. No methodology document was added, removed or renamed, so the
  `CLAUDE.md` methodology list needs no change.
- `CHANGELOG.md` gains a qualifying entry: the new parse capability reserves
  FIX and requires conflicting variables/definitions to be renamed. It also
  states the execution limit, without claiming the later preparation APIs.

The reviewer's P2 debug omission is resolved in source at
`a3a9f9583ee4dcd7fbece0e61b167efa616630c7`. The guide now states the corrected
behavior: legacy text and ASCII/Unicode trees retain complete FIX identity and
optional fixing date at narrow/wide widths; product JSON `/1` rejects any FIX
with `DebugSchemaUnsupported` before writing any output, including past events
and dead branches. FIX is removed from the published JSON `/1` kind list. No
`/2` schema or Describe API is documented as available. The existing changelog
entry remains accurate and needs no additional entry for this scoped repair.

CI repair documentation decision, production revision
`ce158d11115a1c6597540f0fa62eda6e54bcf645`: no further published documentation
or CHANGELOG edit is required. Read the complete repair diff, direct indice
and script diagnostic regressions, and current implementation/test evidence.
The explicit character-predicate bool conversion and direct exception include
repair compiler/backend portability; tape initialization repairs test setup.
Extracted scanner, source-origin, equity-validation and fixing-date helpers
preserve the accepted grammar and documented metadata. Normalizing both DAL
and standard logic errors retains `InvalidIndex`, the full delivery input and
script source context, fulfilling the existing validation contract rather
than adding a capability, algorithm or public surface. Existing FIX reservation,
protected expansion, strict dates, debug behavior and `PreparationRequired`
limits remain accurate. No methodology file or index changes are needed.

Validation:

- `python3 .github/scripts/check_docs.py`: passed for all Markdown files,
  checking local links, tables, whitespace, final newlines and documentation
  metadata/workflow rules.
- `git diff --check` and `git diff --cached --check`: passed; this CI repair
  documentation pass stages only this active evidence file.
- Reviewed the complete published-document diff against lexer/preprocessor/
  parser/node, indice EQ/FX parsers, event execution guards and debug renderers.
  The independent tester's latest results on the CI repair are native Linux
  1590/1590, Adept 1582/1582 and CoDiPack 1582/1582, with public API and portable
  Excel tests enabled. Local complexity verification reports 4/4/7/4 for the
  four flagged functions; new helpers are at most 7. Commands, logs and backend
  configuration are recorded in `testing.md` alongside this file. Remote
  Windows/Codacy results remain pending on the pushed repair head; local
  evidence does not establish those results. No C++ test rerun is needed for
  this evidence-only edit. The orchestrator records the final documentation
  commit SHA; independent re-review of that exact head is the next stage.
