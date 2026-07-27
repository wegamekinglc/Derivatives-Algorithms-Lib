# Curve Lab Technical Design Revision 8 — Independent Adversarial Re-review

Reviewed target:
`curve-lab-technical-design-revision-8.md`, SHA-256
`3eccea423d58738a13d74d3ce89796cbb4845ce3e21b408dbdbdd7ec9a616668`.

Controlling inputs:

- approved product specification `curve-lab-dal-web-v0.5.md`, SHA-256
  `0d0ce731b2beb5591616e6fa865f61335cfcc185c155606e55b67049359ed8da`;
- Revision 8 `MANIFEST.md`, SHA-256
  `27d0689063d71c28299cb7fe11aca6d890a4f387076f27edf5309b3f78c7ee0b`;
- `pricing-risk.mmd`, SHA-256
  `7bdc9bde5fb8c029b74e613f58f21c9d1be43f5d9b2b8d625284a679110aee27`;
- `versions.mmd`, SHA-256
  `46491e329339d1942d1cd34deff06229afbe983cf75fab62015e61c8768f82ad`;
- retained Curve Builder PNG, SHA-256
  `7a9ac6ad2f563105140a0940f4ce872a91b00783db802718d25f193f14c4840c`;
- Revision 7 critique, SHA-256
  `24f8531e3d8c7da4bd6c448f76ccb6c2a47ddbd9d4dbffa6f4f6be5bc3a34ffb`;
- local and freshly resolved remote `master`,
  `98f7b65975a9a5294f5af3693a6fc4a4f1dee7a8`.

The design's pinned source baseline remains the latest available `master`, so
source drift does not affect this review. I read the complete Revision 8,
product specification v0.5, manifest, previous critique, standalone visuals,
Jacobian and AAD methodology, and the closest native, Python, backend,
frontend, persistence, OpenAPI and test analogues.

## Findings

Revision 8 closes the numerical-units blocker from Revision 7. The seven-row
registry now uniquely owns coordinate, canonical unit, raw bump and normalized
bump (`curve-lab-technical-design-revision-8.md:436-456`):

```text
DEPOSIT/FRA/OIS/IRS       -> RATE   / DECIMAL      / +0.0001 / +0.0001
BASIS_SWAP/XCCY           -> SPREAD / DECIMAL      / +0.0001 / +0.0001
FUTURE                    -> PRICE  / PRICE_POINTS / -0.01   / +0.0001
```

`InstrumentDefinitionInputV2` contains only an author-supplied canonical
`raw_quote`, while all five coordinate/unit/normalized/bump members are
derived and read-only. Every successful RATE/SPREAD path therefore stores
canonical decimal, and percent is removed before the draft repository,
fingerprint, run, axis, native gateway or replay sees the value
(`:381-421,518-528`). `QuoteAxisEntry` reparses and byte-checks the stored
canonical value, recomputes the normalized quote, and copies all registry
evidence without accepting override arguments. This is the right P-03
contract and prevents the prior 100-times bump error.

The exact-decimal core is otherwise unusually complete. The two convention
enums are closed; coordinate eligibility is explicit; the ASCII grammar,
512-byte pre/post-transform limit, integer-and-scale base-10 arithmetic,
plain canonical serialization, signed-zero collapse, native binary64
overflow/nonzero-underflow checks, bump distinguishability, exact display
inverse and presentation-only half-even rounding are all unique
(`:470-552`). The fixed first-error table makes malformed combinations
deterministic and requires zero repository, queue, native or tape side
effects.

The proposed tests are capable of detecting the original units bug once an
actual authoring boundary exists. They pair `4/PERCENT` with
`0.04/DECIMAL` for all six RATE/SPREAD families and compare durable JSON,
fingerprint, quote axis, gateway value, DV01/KRD and fresh-process replay.
They preserve the Future price path, exercise both signed zeros and decimal
boundaries, reject derived-field overrides, and explicitly reject a `0.01`
normalized move (`:2492-2532`).

One new cross-layer blocker remains. Revision 8 defines an exact backend
canonicalization service and requires the browser controller to send
`QuoteInputV1` to that adapter, but the formal HTTP surface contains no
authoring-adapter endpoint. The durable draft endpoints cannot fill that gap:
they accept canonical `InstrumentDefinitionInputV2` only and deliberately
reject `PERCENT`. Consequently the required percent UI workflow and its
OpenAPI/client/server/repository/gateway fixture have no executable transport
contract.

## Blocking issues

### 1. The non-durable quote adapter is not reachable across the frontend/backend boundary

The conflicting statements are implementation-controlling:

- `QuoteInputV1` is only `{input_lexeme,input_convention}` and must become
  `InstrumentDefinitionInputV2` before any repository or run input
  (`:403-421`).
- `quote_canonicalization.py` is the sole exact parser, convention adapter,
  serializer, display inverse and native-range preflight; routers and other
  services may not independently convert percent (`:2168-2172`).
- The frontend controller sends the lexeme and convention to that adapter and
  replaces its model with returned canonical bytes (`:2403-2408`).
- The complete listed V2 HTTP surface has draft, build, version, import and
  risk resources, but no quote-canonicalization or presentation-preference
  resource (`:2217-2241`).
- The text nevertheless says the non-durable quote/display schemas appear on
  “authoring adapter/presentation preference endpoints” (`:2243-2252`) without
  defining any such path or schema.

A React controller cannot call a Python service directly. The current
`dal-web/frontend/src/api/client.ts` pattern reaches backend functionality
through typed HTTP methods, and current FastAPI routers expose services through
explicit `APIRouter` handlers. Revision 8 neither adds such a handler nor
permits the durable draft handler to receive `PERCENT`. It also gives
`QuoteInputV1` no family or coordinate member, so a standalone adapter could
not select the registry row from that payload alone.

This leaves all of the following undefined:

1. the URI and HTTP method;
2. a closed request carrying the registry lookup context, instrument identity
   and `QuoteInputV1`;
3. the exact canonical response and whether it returns only canonical
   `raw_quote` or the full server-derived projection;
4. how the Section 5.1 error codes, field paths and fixed precedence are
   represented at the transport boundary;
5. the no-transaction/no-audit/no-queue side-effect rule, authentication and
   retry semantics;
6. the client method and OpenAPI snapshot that make the mandated end-to-end
   percent fixture executable;
7. whether display preferences are client-local or use the claimed
   user/workspace record and endpoint.

The legacy V1 compatibility statement exposes the same unclosed boundary.
Current `SingleCalibrationRequest` receives `market_rate` as a Pydantic
`float` after FastAPI's JSON decoder. Revision 8 promises instead to preserve
the exact incoming JSON numeric token and never persist a binary64
re-rendering (`:2434-2440`), but it does not specify a raw-body or
`parse_float`-equivalent route boundary and does not map an exact-token
compatibility test. Converting the already-created Python float to text would
violate the stated contract.

Required closure:

1. Add one authoritative authoring transport contract. The smallest option is
   a stateless `POST /api/curve-lab/quote-canonicalizations` endpoint whose
   closed request carries `instrument_type` plus `QuoteInputV1`, and whose
   closed response returns canonical `raw_quote`, normalized quote and the
   registry-owned coordinate/unit/bumps. An equally valid alternative is a
   separate authoring request union on the draft endpoint that is normalized
   before repository DTO construction. Do not weaken the canonical durable DTO.
2. Put the selected route in Section 12, its router/service files in Section
   11, and its exact OpenAPI request, response, status, error-location and
   zero-side-effect contracts in the normative design. State how an array row
   or instrument ID is preserved in returned diagnostics.
3. Define the frontend client method and controller sequence. Tests must prove
   the browser sends the original string lexeme, never passes through a
   JavaScript binary number, receives canonical bytes, and submits only those
   bytes to the durable draft endpoint.
4. Make presentation preference ownership unique. Either define the claimed
   endpoint and user/workspace persistence record, including defaults and
   isolation from revisions/fingerprints, or state that preferences are
   client-local and remove the endpoint/store claims.
5. Make the six-family percent/decimal test and the percent-authored
   fresh-process DV01/KRD fixture traverse the actual OpenAPI/client/router/
   adapter boundary before comparing durable bytes and risk. Include
   convention mismatch, malformed lexeme, oversized lexeme, native-range and
   attempted-derived-field cases at that same boundary.
6. For V1 compatibility, specify how the original finite JSON number token is
   captured before binary64 conversion, or explicitly define a narrower
   compatibility contract consistent with the existing Pydantic boundary.
   Add lexical fixtures such as `0.0400`, `4e-2`, a 17-digit value and negative
   zero so an accidental float re-render is observable.

Until this route and token boundary are closed, DAL-16 implementation must
remain paused.

## Significant concerns

No separate significant concern remains after treating the V2 authoring route
and V1 exact-token path as one inbound-canonicalization blocker. The decimal
model, registry derivation, risk bumps, persistence rules and replay semantics
are internally coherent after that boundary.

## Minor notes

1. `CanonicalQuoteDecimalV1` is the canonical output type, while its stated
   accepted input grammar intentionally admits non-canonical leading/trailing
   zeros. Keep distinct OpenAPI names for pre-canonical lexemes and canonical
   output strings so generated clients do not mistake acceptance grammar for
   byte-canonicality.
2. The unchanged Builder PNG is explicitly representative, not an exhaustive
   seven-family registry view. Its manifest wording preserves that distinction.
3. Keep the forbidden-product package check semantic. Terms such as
   `YieldCurve_` and ordinary rate coupons are legitimate curve/rate concepts
   even though the removed bond product surface must stay absent.

## Counter-proposals

1. Prefer the stateless endpoint described above. It preserves one exact
   backend decimal implementation, keeps the repository schema canonical, and
   gives OpenAPI/frontend tests a concrete seam without creating a draft or
   audit event.
2. If a round trip on every keystroke is undesirable, ship a generated,
   string-only TypeScript canonicalizer with golden vectors shared with the
   Python service, then validate again on the backend. That is a larger
   contract change because it contradicts the current “sole adapter” rule; it
   must identify which implementation is normative and how parity is enforced.
3. Presentation preferences need not be server-persisted for MVP. A client-local
   preference is smaller, provided the formal endpoint/record claims are
   removed and tests prove it cannot enter draft revision or fingerprint state.

## Author questions

1. What exact HTTP call is the controller at `:2406-2408` expected to make?
2. Which field supplies the family/coordinate context missing from
   `QuoteInputV1`?
3. Is the presentation-preference endpoint deliberately omitted from the
   resource list, or should the preference remain client-local?
4. Must V1 preserve the incoming number token's lexical bytes, or only its
   mathematical decimal value? The current wording promises the former, while
   the current FastAPI/Pydantic path supplies only a float.

## Gate verification

| Gate | Independent evidence | Result |
|---|---|---|
| Revision 7 raw-unit blocker | Every durable RATE/SPREAD quote is canonical decimal; both fixed bumps are `+0.0001`; Future keeps price points and `-0.01/+0.0001` | Closed |
| Percent isolation | Repository, fingerprint, version, axis, gateway and replay exclude percent; exact base-10 transform and display inverse are defined | Closed at the value model; transport open |
| Closed conventions and decimal behavior | Separate exact enums; complete coordinate eligibility, grammar, signed-zero, rounding, size/native range and fixed error precedence | Closed |
| Quote-axis derivation | Stored DTO byte-check, normalized recomputation and registry-only coordinate/unit/bump projection; no override parameter | Closed |
| Cross-layer equivalence tests | Six-family, Future, boundary, override and 100-times replay assertions are specified | Not executable until the adapter route exists |
| Formal package | All declared hashes match; both embedded Mermaid bodies are byte-identical to standalone files; manifest contains only five normative assets | Closed |
| Source baseline | Local HEAD and freshly resolved remote `master` both equal the pinned SHA | Closed |

## Regression matrix

| Previously closed area | Revision 8 evidence | Result |
|---|---|---|
| Closed pricing result and seven-family allowlist | Revision 7 Section 6.3 is byte-identical; generated exact-key and exact-registry projection tests remain | Closed |
| Removed product scope | Formal design, v0.5, manifest and Mermaid scan contains no bond product, quote, DTO, error, test, future-work or forbidden-surface contract | Closed |
| Bag grammar and error determinism | Revision 7 Section 7.2 is byte-identical; all-token classification, unsigned sorting, alias collision, precedence and permutation fixtures remain | Closed |
| Product/native Jacobian identity | Revision 7 Sections 9.3–9.4 are byte-identical; product `P x Q`, native diagnostic `Q x P`, axes, scaling, tolerance, rank and replay selection remain explicit | Closed |
| AAD eligibility, tape and parity | Revision 7 Section 10.2 is byte-identical; guarded thread-local recording, complete-plan eligibility, parity and fallback rules remain | Closed |
| AAD admission budget | Revision 7 Section 10.6 is byte-identical; central `2PT`, recording/evaluation, context, solve and wall budgets remain charged before side effects | Closed |
| JSON byte boundary | Exact `const char* + size_t` and Python bytes path, 50 MiB/UTF-8/NUL/full-document limits, writer escaping, float formatting and byte stability remain | Closed |
| Deposit/Future methodology | Signed Deposit contract rate and Future decimal convexity/model-rate/price/full-price-point multiplier paths remain | Closed |
| Typed collateral and source-less reconstruction | Persisted semantic-keyed `Bag_` remains sufficient for unique local/XCCY reconstruction; `CurveBlock_` remains runtime-only | Closed |
| PWC and parameter/quote axes | PWC keeps one native-ordered right-forward coordinate per knot; quote-axis fields are now stronger and registry-owned | Closed |
| Exact DV01/KRD without product `J` | Dependency-aware exact recalibration remains available with verified lineage even when `J` is unavailable | Closed |
| Event loop, performance and restart | Awaited native calls, between-call soft deadlines, checked admission budgets, immutable persistence and restart replay remain | Closed except the inbound adapter route above |
| UI/API acceptance | Accessibility, viewport and result/matrix contracts remain; percent acceptance lacks the transport schema required by its own full-stack fixture | Open as described above |

## Verdict: Revise

Revision 8 fixes the canonical quote-unit and fixed-bump problem and preserves
the previously closed numerical, persistence, runtime and product-scope
contracts. One implementation blocker remains: the exact quote
canonicalization boundary is not exposed through a complete, executable
frontend/backend transport contract, and its V1 exact-token compatibility path
is likewise unspecified. Implementation blockers are therefore not zero, and
DAL-16 may not proceed until the route/schema/client/test closure above is
incorporated into the formal package.

No product design or implementation source was modified, and no branch,
commit, pull request, implementation, test or downstream review stage was
started as part of this re-review.
