# Smooth native directional AD review

## Findings

No open local correctness finding. Remote current-head CI, Codacy, review bodies,
inline threads and actual diagnostic/platform execution remain publication gates.

## Scope and reasoning

Read the complete new number/driver, tests, installed consumer and controlling
spec/API/critique. The pair of active native components retains the dependence
needed to reverse the directional derivative; a passive derivative coefficient
alone would not do so. Ordinary native node layout, arithmetic and capability
flags do not change. Zero-valued tangents remain active, including powers at zero.

The first quartic RED exposes finite-step error: 48.0004 against analytic 48.
The prototype passes the unchanged analytic assertion. Expanded REDs exposed
the legacy passive-power derivative's division by zero, allocation exception
type loss and an unguarded opaque reverse event. Local repairs isolate the
smooth power rule, preserve allocation exceptions and reject reverse events
before root access/reversal. An initial overflow fixture mistakenly used a
direction whose product was representable; it now uses a unit direction so the
analytic product actually exceeds double range. No production guard was weakened.

Twenty-one new cases and three selected legacy bump cases pass. Independent
references cover polynomial mixed Hessians, all supported primitive families,
Black-Scholes Gamma/Vanna/Volga and a separately recorded native gradient.
Lifecycle admission, snapshots, callback failures in every direction, nesting,
checkpoint/close rejection, exact budgets and independent concurrent requests
also pass. Six strict OFF/combined checks, six compile capability probes and
installed C++ consumption pass; final source identities control evidence reuse.

## Open questions and limits

No user decision is required. The surface is an opt-in C++ smooth-kernel
prototype. It does not provide arbitrary native higher-order/nested arithmetic,
nonsmooth differentiation, opaque operators or financial/binding integration.
Repeated scalar equality does not establish callback purity. Floating-point
range failures remain explicit. The two scoped paired cost cases pass analytic
checks, retain all 80 samples and leave ordinary library members unchanged.
Codacy's cost-main complexity finding is repaired by extracting request
construction. Both consuming executables and the same two paired cases are
refreshed, with all original and replacement samples retained.

## Verdict

Approve the scoped local implementation. Merge remains gated on complete
current-head CI/Codacy/reviews and actual platform/diagnostic runtime evidence.
