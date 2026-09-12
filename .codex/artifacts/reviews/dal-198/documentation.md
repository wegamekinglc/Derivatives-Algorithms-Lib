DAL-198 F1 documentation handoff, 2026-09-12. This record controls the pending
independent review; retire it after delivery.

Reconciled documentation against source and tests at
`cead11e0548d6fbffcb7c44339ccb0e4c505cfbf`, the accepted F1 issue and frontend
design/critique/API boundaries, and the implementation/testing evidence.

Documentation changes are required:

- `docs/methodology/script_engine.md` described `Tokenize` as the sole string
  tokenizer and omitted FIX. It now describes positioned `Lex` tokens, the
  legacy string projection, complete index literals, preserved bracket content
  and delivery suffixes, protected macro/placeholder expansion, strict optional
  fixing dates, reserved FIX names, NodeFix metadata, and source origins.
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

Non-trivial current debug limitation flagged to the orchestrator for review:
`Debugger_::Visit(NodeFix_)` records a legacy label and structural kind only.
The legacy text dump retains raw index/date, JSON emits kind `fix` without
index/date fields, and `TreeInlineLeaf` has no FIX case so the tree omits the
leaf. The guide states these actual limits. No source repair belongs to this
documentation stage.

Validation:

- `python3 .github/scripts/check_docs.py`: passed for all Markdown files,
  checking local links, tables, whitespace, final newlines and documentation
  metadata/workflow rules.
- `git diff --check` and `git diff --cached --check`: passed; staged scope
  contains only the four published documentation/changelog files above and
  this active evidence file.
- Reviewed the complete published-document diff against lexer/preprocessor/
  parser/node, indice EQ/FX parsers, event execution guards and debug renderers.
  Existing focused 241/241 and full Linux 1586/1586 results are owned by the
  independent tester; no C++ test rerun is needed for these documentation-only
  edits. The orchestrator records the final documentation commit SHA.
