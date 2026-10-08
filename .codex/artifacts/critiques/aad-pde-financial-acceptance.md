# Fixed-grid European PDE acceptance critique

Verdict: Proceed with caveats.

## Blocking issues addressed before implementation

- The first refinement proposal changed the strike's cell phase. Its retained
  independent output has nonmonotonic price/strike errors. The replacement
  factor-three refinements preserve the midpoint phase and show every declared
  error reduction before DAL acceptance runs. Method and terminal payoff are
  unchanged; this is an explicit fixture correction, not a tolerance repair.
- Active old/boundary matrices require valid native slots for every cell,
  including exact zeros. Use one recorded zero expression as a shared alias.
  Otherwise the advertised caller would fail capture before any PDE step.
- Terminal payoff and discounted boundaries both depend on strike; coefficients
  and discounted boundaries both depend on rate. Every independent bump must
  rebuild the whole chain, not reuse frozen coefficient/terminal/boundary state.

## Significant concerns and required evidence

- Native event scatter aggregates coefficients over the two financial layers.
  The output-channel axis must remain independent: explicit call, put, weighted
  and zero lanes; repeated sweeps must not retain earlier seeds or root risks.
- The finite-domain boundary is an approximation to an unbounded model. Frozen
  discrete oracles accept the declared program; erfc closed forms accept its
  separately bounded discretization error. No shared tolerance is valid.
- Damped stepping has four half steps and ordinarySteps-2 full steps. Assert
  ordinarySteps+2 event reports and exact final expiry construction, avoiding
  cumulative tau drift or an accidental extra ordinary step.
- Independent dense elimination must construct its own generator and identity
  boundary rows. Calling the same DAL cache for finite differences alone cannot
  prove an omitted model dependency or a wrong financial forward program.
- Example helpers may support the declared fixture and clear validated shape
  constraints; they must not present this increment as a general pricing facade,
  adaptive mesh derivative, American exercise solver or second-order engine.
- This is an additive caller/test/example increment. Reuse immutable #505 core
  archive/object/caller proof when inputs match; no old performance matrix is
  justified by adding these financial assertions.

## Acceptance scope

Six named cases and one self-contained example, explicit fixed parameters and
three refinement levels. Freeze literals and model/difference/parity ceilings
before the first native execution. Retain failed evidence and investigate any
failure rather than widening tolerance. Required remote gates remain exact-head
checks, full review-body inspection, Codacy annotations, actual new-case runtime
and guarded accepted/merged tree verification.

Open questions: none for this bounded increment.
