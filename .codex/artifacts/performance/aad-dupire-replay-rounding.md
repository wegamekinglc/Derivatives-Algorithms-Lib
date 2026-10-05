# Dupire replay correction: paired entry costs

Status: local correction passes the retained entry comparison on the unaffected
default x86 build. FMA correctness and exact publication CI are separate checks;
P01's production MC verdict remains inconclusive.

## Protocol and failed first implementation

Compare the unchanged accepted native archive in the `12dcd09a` installed
package with the final correction, using the same helper and GCC 15 Release
configuration. Pin each process to CPU 4 and one worker. Run two rounds of ten
interleaved processes per side, mode and case; rotate mode order and alternate
which side starts. Preserve the existing best-of-ten minima and 4% two-round
boundary. There are 480 processes per implementation comparison, with no DAL
build/test running during either measurement.

The three modes are original numeric calibration, checked snapshot construction
and scalar quote VJP. Flat/Merton bases use 9x2 and 21x8 surfaces, 3x2 quote
axes and zero base spreads; independent functional tests cover nonzero spreads.
Three untimed warmups precede 64 small or 16 medium calls per process. Timing windows
include result checks and destruction. Every price/surface/gradient/checksum
cell agrees bitwise across both sides; full source/helper/package/binary hashes
and dependency SHAs are unchanged within each measurement.

The unconditional call-aligned replay passes correctness but fails the cost
comparison: all four VJP cases increase by approximately 72–98% in both rounds.
Keep that complete measurement and exact failed implementation source. The
final implementation retains ordinary replay whenever its strict primal check
passes; only a mismatch records the call-aligned graph. Every final case passes
the unchanged comparison. No threshold, sample count or failed row is removed.

## Final comparison

Percentages compare final head with the accepted installed entry in each round.

| Case        | Mode     | Round 1 | Round 2 |
|-------------|----------|---------|---------|
| Flat 9x2    | Legacy   | +0.04%  | −0.07%  |
| Flat 9x2    | Snapshot | +0.47%  | −0.07%  |
| Flat 9x2    | VJP      | −0.59%  | −2.23%  |
| Flat 21x8   | Legacy   | −1.15%  | −0.13%  |
| Flat 21x8   | Snapshot | −0.60%  | +1.97%  |
| Flat 21x8   | VJP      | +0.13%  | +0.43%  |
| Merton 9x2  | Legacy   | −0.19%  | +0.83%  |
| Merton 9x2  | Snapshot | −0.20%  | +1.19%  |
| Merton 9x2  | VJP      | −0.58%  | −2.34%  |
| Merton 21x8 | Legacy   | +0.40%  | −0.61%  |
| Merton 21x8 | Snapshot | −0.34%  | +0.81%  |
| Merton 21x8 | VJP      | +0.04%  | −0.19%  |

All nine original OFF gate executables remain SHA-256 identical to their
accepted measurement inputs. The native archive changes because the correction
is implemented there; do not claim archive identity. This comparison covers
the agreeing x86 path. The additional FMA recovery path has no successful old
equivalent and its cost is not measured here. Shared WSL2 hardware and the small
case inventory limit extrapolation; no production MC conclusion follows.

## Evidence

Root: `/home/wegamekinglc/.cache/dal-aad-evidence-20261004-8886c083/evidence`.

- `aad-dupire-replay-cost-paired-01/`: failed first implementation, all 480
  processes, raw output, numeric assertions, environment and frozen hashes.
- `aad-dupire-replay-unconditional.cpp`: first source reconstructed and verified
  exactly against its measured SHA-256.
- `aad-dupire-replay-cost-paired-02/`: final 480-process comparison with the same
  protocol and complete raw output, numeric assertions and identities.
- `aad-dupire-replay-cost-pairs.py`: shell-free orchestration and identity checks;
  `aad-dupire-entry-cost-source/` retains the unchanged operation helper.
- `aad-dupire-replay-cost-{old-build,head-build-01,head-build-02}` build artifacts and
  `aad-dupire-replay-{off,combined}-install-{01,02}` retain both implementations.
- The final review retains nine-binary identity, unchanged native/Python oracle
  traces, installed-consumer parity and conditional replay correctness.
