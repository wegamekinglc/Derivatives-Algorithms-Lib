# WP-4c documentation decision

Followed dal-doc-writer locally. Read the complete LSM methodology and relevant
source/public headers/tests. Its early-exercise section described replay as a
per-batch worker; update it for per-replicate persistent workers, preserved batch
ordering, leaf clearing and bounded numeric partials. Keep the central explanation
in that guide. The script-engine batching section already states LSMC retains
its separate BatchPlan policy and remains accurate.

No CHANGELOG entry: this is an internal scheduling/storage refactor preserving
public APIs, numerical algorithms, default budgets, stream layout and results.
It does not meet the repository's significant-capability/methodology threshold.
No new, removed or renamed methodology page, so index updates are unnecessary.
Markdown integrity checks are required before publication.
