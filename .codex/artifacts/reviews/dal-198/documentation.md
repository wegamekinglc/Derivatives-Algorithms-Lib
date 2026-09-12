DAL-198 F1 documentation handoff, 2026-09-12. This record controls the pending
independent review; retire it after delivery.

Reconciled documentation against source and tests at
`9e5613fde2c171bf094ad868b5601c0689fb82e1`, the accepted F1 issue and frontend
design/critique/API boundaries, and the implementation/testing evidence.

Documentation changes are required:

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

Validation:

- `python3 .github/scripts/check_docs.py`: passed for all Markdown files,
  checking local links, tables, whitespace, final newlines and documentation
  metadata/workflow rules.
- `git diff --check` and `git diff --cached --check`: passed; the correction's
  staged scope contains only `docs/methodology/script_engine.md` and this
  active evidence file.
- Reviewed the complete published-document diff against lexer/preprocessor/
  parser/node, indice EQ/FX parsers, event execution guards and debug renderers.
  Independent tester results on the repaired source are focused 29/29
  (`ScriptObservationTest.*:*Debug*`) and fresh full Linux 1588/1588, including
  the FIX debug regressions and exact legacy SPOT snapshots. No C++ test rerun
  is needed for these documentation-only edits. The orchestrator records the
  final documentation commit SHA; independent re-review is the next stage.
