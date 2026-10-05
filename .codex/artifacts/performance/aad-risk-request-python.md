# Python quote request performance

Status: bounded changed-binding acceptance passes; own-head CI remains pending.
Automatic Python, Excel requests, full F01 and whole-PR/P01 gates remain open.

## Scope and frozen inputs

Baseline is the freshly rebuilt OFF module at accepted C++ head
`e42c483551cd252fc95a7574b83b562ee6abb3ee`, frozen in
`aad-risk-request-python-baseline-01`. Head is the common Python request
increment frozen in `aad-risk-request-python-measured-head-02`. Source and
module hashes identify the uncommitted implementation independently of the
later publication SHA. No core/public numerical implementation changes occur.

The original unchanged 35-case worker is
`aad-calibration-python-cost-worker-01.py`: retained-default provenance,
actual ten-trade aggregation, four curve providers, N=8/16, both inverse modes
and typed Dupire. An additional unchanged 28-case worker covers six scalar
request configurations, legacy/default/selected MC and passive Jacobian getters
for tree/compiled and 257/2057 paths, plus the six existing common VJP providers.
New request/plan/full-subset-empty VJP/projection costs are separate informational
rows; they do not replace the existing coverage.

Release/static native AAD, lifetime/profiling OFF; c++ (Ubuntu 15.2.0-16ubuntu1) 15.2.0, CPython 3.13.9,
CPU affinity 4. Original curve workloads use four native workers; the additional
scalar workload uses one for exact native sum-order checks, matching both sides.
Two independent rounds, ten alternating process samples per side per group,
minimum per round, unchanged strict +4% in both rounds. The original worker's
warmups/inner iterations remain; the extended worker uses two warmups and the
recorded case iteration counts. Raw environment/compiler/cache/protocol details
are in `aad-risk-request-python-cost-paired-02/protocol.json`.

## Failed evidence and correction

`aad-risk-request-python-cost-paired-01/results.json` fails one old case:
`scalar_request_selected` is +4.56%/+5.97%. All numbers and 747 input hashes
remain unchanged. Sharing parser type/field context added normal-path string
assembly. Replace it with explicit constant field/identifier labels while
keeping the single generic parser, every type/range check, old scalar diagnostics
and deferred semantic validation. The corrected full rerun passes all 63 rows;
the formerly failing row is -0.42%/-1.19%. Do not discard the first failure or
relax threshold, samples, work, case names or mathematical assertions.

## Complete accepted old-entry rows

All 80 paired legacy processes preserve descriptors/numeric digests. Largest
smaller-of-two-round delta is +1.63%; the calibrated criterion accepts a row
unless both confirmation rounds exceed +4%.

| Group    | Case                                        | R1 base ns | R1 head ns | R1 delta | R2 base ns | R2 head ns | R2 delta | Gate |
|----------|---------------------------------------------|------------|------------|----------|------------|------------|----------|------|
| original | config_1                                    | 566.11     | 569.06     | +0.52%   | 567.89     | 571.09     | +0.56%   | PASS |
| original | config_8                                    | 1250.14    | 1250.08    | -0.00%   | 1273.30    | 1267.46    | -0.46%   | PASS |
| original | single_8_ANALYTIC_provenance                | 240607.44  | 240627.76  | +0.01%   | 242546.71  | 242339.88  | -0.09%   | PASS |
| original | single_8_ANALYTIC_aggregate_10_trades       | 26806.90   | 26822.55   | +0.06%   | 26805.15   | 27281.25   | +1.78%   | PASS |
| original | generic_8_ANALYTIC_provenance               | 425099.68  | 425416.88  | +0.07%   | 423982.57  | 429559.03  | +1.32%   | PASS |
| original | generic_8_ANALYTIC_aggregate_10_trades      | 161691.10  | 161303.55  | -0.24%   | 161989.80  | 162341.15  | +0.22%   | PASS |
| original | joint_xccy_8_ANALYTIC_provenance            | 2525424.79 | 2544885.27 | +0.77%   | 2546831.04 | 2570783.40 | +0.94%   | PASS |
| original | joint_xccy_8_ANALYTIC_aggregate_10_trades   | 1907973.15 | 1897705.70 | -0.54%   | 1917366.45 | 1907575.00 | -0.51%   | PASS |
| original | staged_xccy_8_ANALYTIC_provenance           | 798048.13  | 799755.75  | +0.21%   | 808257.17  | 809390.67  | +0.14%   | PASS |
| original | staged_xccy_8_ANALYTIC_aggregate_10_trades  | 706748.00  | 706595.80  | -0.02%   | 709225.35  | 703792.85  | -0.77%   | PASS |
| original | single_8_BUMPED_provenance                  | 239508.11  | 239668.92  | +0.07%   | 241537.33  | 245811.00  | +1.77%   | PASS |
| original | single_8_BUMPED_aggregate_10_trades         | 27349.30   | 26778.60   | -2.09%   | 27123.15   | 26904.70   | -0.81%   | PASS |
| original | generic_8_BUMPED_provenance                 | 408492.58  | 413592.07  | +1.25%   | 416072.45  | 423434.93  | +1.77%   | PASS |
| original | generic_8_BUMPED_aggregate_10_trades        | 159897.45  | 161423.95  | +0.95%   | 162242.15  | 159900.55  | -1.44%   | PASS |
| original | joint_xccy_8_BUMPED_provenance              | 2513085.73 | 2506662.89 | -0.26%   | 2542715.43 | 2512208.85 | -1.20%   | PASS |
| original | joint_xccy_8_BUMPED_aggregate_10_trades     | 1907033.90 | 1898281.75 | -0.46%   | 1914243.20 | 1919611.55 | +0.28%   | PASS |
| original | staged_xccy_8_BUMPED_provenance             | 800186.64  | 802519.55  | +0.29%   | 806447.24  | 806832.93  | +0.05%   | PASS |
| original | staged_xccy_8_BUMPED_aggregate_10_trades    | 709105.35  | 706304.80  | -0.39%   | 712126.95  | 708392.00  | -0.52%   | PASS |
| original | single_16_ANALYTIC_provenance               | 483336.55  | 485996.68  | +0.55%   | 487460.52  | 494429.87  | +1.43%   | PASS |
| original | single_16_ANALYTIC_aggregate_10_trades      | 38394.10   | 37861.20   | -1.39%   | 38192.70   | 37830.50   | -0.95%   | PASS |
| original | generic_16_ANALYTIC_provenance              | 763687.76  | 758860.77  | -0.63%   | 766212.57  | 767143.19  | +0.12%   | PASS |
| original | generic_16_ANALYTIC_aggregate_10_trades     | 242875.45  | 242877.55  | +0.00%   | 242448.20  | 245850.45  | +1.40%   | PASS |
| original | joint_xccy_16_ANALYTIC_provenance           | 6667967.69 | 6699947.00 | +0.48%   | 6716771.56 | 6730758.54 | +0.21%   | PASS |
| original | joint_xccy_16_ANALYTIC_aggregate_10_trades  | 3564494.60 | 3575496.05 | +0.31%   | 3589910.20 | 3604814.35 | +0.42%   | PASS |
| original | staged_xccy_16_ANALYTIC_provenance          | 1420902.04 | 1414993.31 | -0.42%   | 1443406.69 | 1430105.55 | -0.92%   | PASS |
| original | staged_xccy_16_ANALYTIC_aggregate_10_trades | 1315162.55 | 1312427.05 | -0.21%   | 1300887.00 | 1309881.25 | +0.69%   | PASS |
| original | single_16_BUMPED_provenance                 | 481752.88  | 479883.24  | -0.39%   | 489377.84  | 486078.86  | -0.67%   | PASS |
| original | single_16_BUMPED_aggregate_10_trades        | 37877.30   | 37670.65   | -0.55%   | 37720.75   | 38498.85   | +2.06%   | PASS |
| original | generic_16_BUMPED_provenance                | 731577.43  | 733119.85  | +0.21%   | 740319.25  | 744771.34  | +0.60%   | PASS |
| original | generic_16_BUMPED_aggregate_10_trades       | 243021.45  | 241500.65  | -0.63%   | 243586.75  | 241679.85  | -0.78%   | PASS |
| original | joint_xccy_16_BUMPED_provenance             | 6685967.33 | 6681155.60 | -0.07%   | 6748978.01 | 6737501.41 | -0.17%   | PASS |
| original | joint_xccy_16_BUMPED_aggregate_10_trades    | 3578572.60 | 3573691.95 | -0.14%   | 3573339.50 | 3586916.00 | +0.38%   | PASS |
| original | staged_xccy_16_BUMPED_provenance            | 1416419.20 | 1426074.18 | +0.68%   | 1422978.68 | 1430793.96 | +0.55%   | PASS |
| original | staged_xccy_16_BUMPED_aggregate_10_trades   | 1309041.40 | 1310345.15 | +0.10%   | 1312241.65 | 1316626.35 | +0.33%   | PASS |
| original | typed_dupire_9x2                            | 24172.59   | 24202.84   | +0.13%   | 24002.05   | 24302.46   | +1.25%   | PASS |
| extended | scalar_request_default                      | 264.56     | 265.16     | +0.23%   | 269.12     | 271.14     | +0.75%   | PASS |
| extended | scalar_request_empty                        | 1717.58    | 1724.11    | +0.38%   | 1724.05    | 1718.28    | -0.33%   | PASS |
| extended | scalar_request_selected                     | 2915.86    | 2903.66    | -0.42%   | 2982.97    | 2947.55    | -1.19%   | PASS |
| extended | scalar_request_tuple                        | 3298.08    | 3350.01    | +1.57%   | 3383.98    | 3376.24    | -0.23%   | PASS |
| extended | scalar_request_dal_strings                  | 1485.25    | 1557.83    | +4.89%   | 1500.24    | 1524.75    | +1.63%   | PASS |
| extended | scalar_request_wide_128                     | 99443.17   | 99753.32   | +0.31%   | 100914.90  | 99417.42   | -1.48%   | PASS |
| extended | scalar_False_257_legacy                     | 49761.10   | 49653.60   | -0.22%   | 50381.25   | 49336.03   | -2.07%   | PASS |
| extended | scalar_False_257_structured_default         | 53968.57   | 53682.50   | -0.53%   | 54474.97   | 53144.90   | -2.44%   | PASS |
| extended | scalar_False_257_structured_selected        | 54183.50   | 54050.35   | -0.25%   | 54238.07   | 53615.80   | -1.15%   | PASS |
| extended | scalar_False_257_jacobian_getter            | 216.69     | 209.05     | -3.52%   | 215.50     | 218.94     | +1.60%   | PASS |
| extended | scalar_False_2057_legacy                    | 304928.00  | 302446.20  | -0.81%   | 303574.00  | 303717.30  | +0.05%   | PASS |
| extended | scalar_False_2057_structured_default        | 305997.80  | 308346.80  | +0.77%   | 307988.50  | 308458.20  | +0.15%   | PASS |
| extended | scalar_False_2057_structured_selected       | 307766.30  | 306416.80  | -0.44%   | 312937.10  | 307928.50  | -1.60%   | PASS |
| extended | scalar_False_2057_jacobian_getter           | 223.93     | 217.50     | -2.87%   | 214.04     | 218.05     | +1.88%   | PASS |
| extended | scalar_True_257_legacy                      | 49222.07   | 48933.03   | -0.59%   | 49528.70   | 49535.93   | +0.01%   | PASS |
| extended | scalar_True_257_structured_default          | 54535.43   | 54463.78   | -0.13%   | 54442.80   | 55228.70   | +1.44%   | PASS |
| extended | scalar_True_257_structured_selected         | 55052.25   | 54691.20   | -0.66%   | 54580.47   | 54753.10   | +0.32%   | PASS |
| extended | scalar_True_257_jacobian_getter             | 217.31     | 214.22     | -1.42%   | 212.94     | 218.50     | +2.61%   | PASS |
| extended | scalar_True_2057_legacy                     | 310414.20  | 302238.80  | -2.63%   | 306053.80  | 303836.30  | -0.72%   | PASS |
| extended | scalar_True_2057_structured_default         | 312267.80  | 312851.70  | +0.19%   | 310450.50  | 309712.30  | -0.24%   | PASS |
| extended | scalar_True_2057_structured_selected        | 309364.50  | 308736.40  | -0.20%   | 309188.70  | 309148.10  | -0.01%   | PASS |
| extended | scalar_True_2057_jacobian_getter            | 214.65     | 216.45     | +0.84%   | 220.87     | 218.28     | -1.17%   | PASS |
| extended | existing_common_single                      | 618.92     | 606.18     | -2.06%   | 614.66     | 628.89     | +2.31%   | PASS |
| extended | existing_common_generic                     | 640.06     | 632.12     | -1.24%   | 618.51     | 631.58     | +2.11%   | PASS |
| extended | existing_common_layered                     | 632.39     | 625.34     | -1.12%   | 622.38     | 624.02     | +0.26%   | PASS |
| extended | existing_common_joint_xccy                  | 611.15     | 614.74     | +0.59%   | 606.26     | 605.20     | -0.17%   | PASS |
| extended | existing_common_staged_xccy                 | 598.25     | 592.26     | -1.00%   | 594.41     | 588.68     | -0.96%   | PASS |
| extended | existing_common_dupire                      | 24674.38   | 24697.72   | +0.09%   | 24895.19   | 24765.19   | -0.52%   | PASS |

## Complete new costs

Twenty new-entry processes preserve all descriptors/digests. These small
prepared-source Python costs include ordinary binding conversion/copy overhead;
they exclude initial calibration/source creation and characterize neither long
MC paths nor production portfolio scaling.

| New entry                          | R1 minimum ns | R2 minimum ns |
|------------------------------------|---------------|---------------|
| common_request_default             | 257.41        | 258.61        |
| common_plan_default                | 981.33        | 983.05        |
| common_vjp_default                 | 25452.54      | 25265.40      |
| common_request_selected            | 2814.42       | 2923.99       |
| common_plan_selected               | 1232.61       | 1207.23       |
| common_vjp_selected                | 25574.05      | 25221.95      |
| common_request_empty               | 1133.15       | 1172.24       |
| common_plan_empty                  | 1018.66       | 1023.60       |
| common_vjp_empty                   | 25104.52      | 24822.92      |
| common_getter_jacobian             | 227.00        | 225.81        |
| common_getter_calibration_jacobian | 239.84        | 233.96        |
| common_getter_direct_jacobian      | 244.97        | 232.78        |
| common_getter_reported_jacobian    | 232.15        | 230.19        |

## Evidence and boundaries

Evidence root: `/home/wegamekinglc/.cache/dal-aad-evidence-20261004-8886c083/evidence`.
`aad-risk-request-python-cost-run-02.py`, its log and
`aad-risk-request-python-cost-paired-02/` retain the exact workers, protocol,
all 100 process outputs, per-round minima/verdicts and 747 unchanged input hashes.
No source/Git/PR/build/test activity overlapped either formal run.
The first paired run and its frozen head package remain intact.

Workspace correctness passes 971; fresh installed OFF/combined modules each
pass 970 with one existing opaque-curve test-helper absence skipped. Each of
three modules matches fifteen independently installed C++ full/subset/empty
requests, every coordinate and all seven matrices exactly. Thirty-six old
scalar parser cases preserve exception type, stable diagnostic content and
validation timing; source file/line traces are excluded from that comparison.
These checks are performed again after the performance correction.

Overall verdict: no regression for these changed existing Python entries.
Native nine-target identity and accepted C++ numerical/oracle evidence remain
bounded and unchanged. This does not resolve production P01's separate noisy
MC measurements or close full F01/whole-PR/master acceptance.
