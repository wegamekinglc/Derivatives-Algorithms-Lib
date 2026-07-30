# Curve Lab DAL-WEB — Corrected Design Package v0.5 / Revision 8

Status: product scope approved; Revision 8 ready for independent DAL-17
technical-design re-review  
Approved product decisions: P-01=A; P-02; P-03  
Correction date: 2026-07-27  
Supersedes Revision 7 manifest SHA-256:
`1090a569f3697b4e19e0835fa418d78d57351b9985a388775599048bbe5626ee`

## Normative decisions

- Integrate Curve Lab into DAL-WEB.
- Use exactly the ordered Curve Lab V1 success-family registry in product
  specification v0.5:
  `DEPOSIT,FRA,FUTURE,OIS,IRS,BASIS_SWAP,XCCY`.
- Project that same registry through DTO, OpenAPI, persistence, gateway, UI,
  examples, results, and tests without missing or additional entries.
- Every durable RATE/SPREAD `raw_quote` is canonical decimal.
  `exact_risk_raw_bump` and the normalized one-basis-point bump are fixed at
  `+0.0001`; percent is input/display metadata only and cannot affect stored
  financial bytes or replay.
- Future retains its `PRICE_POINTS` coordinate, `-0.01` raw bump, and
  `+0.0001` normalized bump.
- Support single-curve and multi-curve dependencies.
- Support calibration Jacobian, trade-to-curve sensitivity, PV, DV01/PV01, and
  Key Rate DV01; exclude Gamma, Vega, and CS01.
- Persist a single root as `DiscountCurve_` and a multi-root curve set as the
  serializable `Bag_` DTO using the normative native-enum semantic-key grammar
  and order-independent token-validation state machine.
- `CurveBlock_` is reconstructed only as an ephemeral runtime view. It is not
  a persisted root, DTO, archive/import/export format, or stored-type value.
- No out-of-scope successful instrument workflow is included.
- No approval or four-eyes workflow is introduced.
- Match the existing DAL-WEB industrial terminal design system.

## Normative assets

| Asset | SHA-256 |
|---|---|
| `01-curve-builder-dal-web.png` | `7a9ac6ad2f563105140a0940f4ce872a91b00783db802718d25f193f14c4840c` |
| `curve-lab-dal-web-v0.5.md` | `0d0ce731b2beb5591616e6fa865f61335cfcc185c155606e55b67049359ed8da` |
| `curve-lab-technical-design-revision-8.md` | `3eccea423d58738a13d74d3ce89796cbb4845ce3e21b408dbdbdd7ec9a616668` |
| `pricing-risk.mmd` | `7bdc9bde5fb8c029b74e613f58f21c9d1be43f5d9b2b8d625284a679110aee27` |
| `versions.mmd` | `46491e329339d1942d1cd34deff06229afbe983cf75fab62015e61c8768f82ad` |

`pricing-risk.mmd` and `versions.mmd` are unchanged from Revision 7 and are
byte-identical to their corresponding Mermaid bodies embedded in Revision 8.
They describe family flow and the persisted-`Bag_`/runtime-only-`CurveBlock_`
boundary; neither contains quote-unit, input-convention, display-convention,
or risk-bump detail affected by P-03. The retained builder PNG is unchanged
because its representative Deposit, OIS, Future, and IRS rows remain registry
members and it contains no quote-unit selector or additional product element.
There are no other normative visual assets in this package.

## Delivery boundary

This package is design evidence only. It authorizes no source implementation,
branch, commit, pull request, merge, or later implementation stage. DAL-17 must
independently re-review Revision 8 before implementation is unparked.
