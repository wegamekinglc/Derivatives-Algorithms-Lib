# WP-4c API decision

Audience: existing core/public LSMC and Black-Scholes curvature callers, Python
and Excel bindings that use those surfaces. No public argument, result layout,
default or error category changes are proposed. Fixed-policy gradients and
RetrainedBump secants retain their existing definitions and common samples.

Generalize the internal common worker/dispatcher plan type with a trailing,
deduced/defaulted template parameter so ordinary callers keep their current
source contract. LSMC supplies its existing BatchPlan_(paths, 1); do not retune
LSMC to the ordinary adaptive chunk policy.

Keep one final risk axis. Reuse built-in numeric partials and separate small
worker capacity statistics from risk slots. Curvature's maxBatchTapeBytes_ and
maxBatchCleanupReserveBytes_ still report maxima, including prefix/tape cleanup;
recording closes once per persistent worker before those statistics are read.

Rejected alternatives: change training budgets; alter the LSMC partition to
obtain finer parallelism; move recordings across threads; publish a new tuning
setting solely for implementation details; leave curvature as an untested caller
of the changed replay kernel.
