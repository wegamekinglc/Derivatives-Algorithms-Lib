# Excel PDE design critique

Verdict: Proceed with caveats.

## Blocking issues

None in the revised narrow interface.

## Significant concerns

1. Generated scalar conversion may erase bool/text/blank distinctions.
   Guard actual OPER inputs first, normalize supported integer/one-cell forms,
   and test actual exported wrappers rather than only helper functions.
2. Generic storable metadata overloads declared after a template must be found
   through argument-dependent lookup. Keep overloads beside value namespaces,
   and test inclusion after existing risk headers. Do not modify every old
   wrapper or duplicate its storage/serialization implementation.
3. Optional blank budgets and numeric zero are different. The round trip must
   preserve both, including a result's copied actual request. Optional index
   stays omitted until evaluation resolves it.
4. Native valid extents can exceed a worksheet's output rows; checked settings
   must reserve header space for grid and ordinary_steps+2 diagnostics without
   allocating huge arrays to test the bounds.
5. Settings/request construction and every getter must remain passive beside
   a caller graph. Only result construction executes the existing public
   evaluator; failures must preserve both caller state and previous outputs.
6. Retained native payload does not account for Excel handles/cells/labels.
   Preserve the exact native formula and explicit exclusions; never advertise
   it as a bound on all worksheet memory.

## Minor notes

Keep node indices zero-based and chronological step labels one-based explicitly.
Preserve raw units, four transpose axes and every actual solve diagnostic.
Keep generated help within existing registration limits and avoid multi-output
format arguments when each getter has one useful output.

## Counter-proposal

Adopted: reuse existing risk storables and a three-line public validation
projection, with separate immutable settings/request/result handles. This
avoids a broad shared-template refactor and a second PDE program.

## Author questions

None blocking. Performance and platform acceptance remain subsequent gates,
not conclusions implied by this design verdict.
