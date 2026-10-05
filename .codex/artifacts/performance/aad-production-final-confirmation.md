# Final production Monte Carlo confirmation

Status: complete current-head confirmation passes 44/44 cases; final PR checks remain required.

## Protocol and inputs

Baseline `b5e3caca85bb1a5b1aa3acf7898b387832d443c8` is the original implementation baseline.
Head `7a46b93219055d7a591727d0860e80e4a1106e1a` uses current core/public sources.
Release GCC 15.2, static libraries, Eigen ON, native architecture OFF, empty extra C++ flags,
native AAD lifetime/profiling OFF. Both sides use four native threads and CPU affinity 4,6,8,10.
Original benchmark commands, inner work and path counts remain.

Before sampling, declare the complete 44-case scope, two independent rounds, thirty
alternating processes per side/round, minimum reduction and strict +4% in both rounds.
This is the established noisy-host confirmation policy. No outcome-selected repetition
or threshold change occurs. Linux load/process snapshots are quiet; Windows host activity
remains uncontrolled on shared WSL2. No edits, builds, tests, Git or PR work overlap sampling.

All 600 processes preserve 1,240 frozen source/helper/configuration/archive/executable
hashes. All three LSM PV/every-risk vectors remain bitwise equal in every paired sample,
retaining rel/abs 1e-10 checks. Native mathematical and nine-target acceptance are separate.

## Complete existing rows

Times are round minima in nanoseconds.

| Profile                | Case                                                                             | R1 base ns   | R1 head ns   | R1 change | R2 base ns   | R2 head ns   | R2 change | Verdict |
| ---------------------- | -------------------------------------------------------------------------------- | -----------: | -----------: | --------: | -----------: | -----------: | --------: | ------- |
| ordinary               | GSR 1F 5Y swaption (1 price, order 16/32)                                        | 16885.00     | 16910.00     | +0.15%    | 15836.00     | 16775.00     | +5.93%    | PASS    |
| ordinary               | GSR 1F bond (100K paths x 4 steps)                                               | 7988000.00   | 8362000.00   | +4.68%    | 8280000.00   | 8046000.00   | -2.83%    | PASS    |
| ordinary               | GSR 1F bond option (1000 prices)                                                 | 2146000.00   | 2120000.00   | -1.21%    | 2082000.00   | 2130000.00   | +2.31%    | PASS    |
| ordinary               | GSR 1F swap and Libor (10K paths x 4 steps)                                      | 19238000.00  | 18718000.00  | -2.70%    | 18727000.00  | 18770000.00  | +0.23%    | PASS    |
| ordinary               | GSR 2F 5Y swaption (1 price, order 16/32)                                        | 314140.00    | 322548.00    | +2.68%    | 301593.00    | 314203.00    | +4.18%    | PASS    |
| ordinary               | GSR 2F bond (100K paths x 4 steps)                                               | 11603000.00  | 11760000.00  | +1.35%    | 11143000.00  | 11288000.00  | +1.30%    | PASS    |
| ordinary               | GSR 2F bond option (1000 prices)                                                 | 2979000.00   | 3037000.00   | +1.95%    | 2848000.00   | 2980000.00   | +4.63%    | PASS    |
| ordinary               | GSR 2F swap and Libor (10K paths x 4 steps)                                      | 18435000.00  | 19154000.00  | +3.90%    | 18470000.00  | 18088000.00  | -2.07%    | PASS    |
| ordinary               | GSR 3F 5Y swaption (1 price, order 16/32)                                        | 308484.00    | 317814.00    | +3.02%    | 307065.00    | 315125.00    | +2.62%    | PASS    |
| ordinary               | GSR 3F bond (100K paths x 4 steps)                                               | 12746000.00  | 12799000.00  | +0.42%    | 12744000.00  | 12555000.00  | -1.48%    | PASS    |
| ordinary               | GSR 3F bond option (1000 prices)                                                 | 4048000.00   | 3959000.00   | -2.20%    | 4041000.00   | 3995000.00   | -1.14%    | PASS    |
| ordinary               | GSR 3F swap and Libor (10K paths x 4 steps)                                      | 19487000.00  | 17741000.00  | -8.96%    | 19048000.00  | 19215000.00  | +0.88%    | PASS    |
| ordinary               | GSR g calibration (3 quotes x 3 buckets)                                         | 240046.00    | 239035.00    | -0.42%    | 232354.00    | 243310.00    | +4.72%    | PASS    |
| ordinary               | LSMC regression degree=3 (100000 paths)                                          | 467635.00    | 455032.00    | -2.70%    | 454565.00    | 455091.00    | +0.12%    | PASS    |
| ordinary               | LSMC regression degree=3 ITM mask (100000 paths)                                 | 431558.00    | 432380.00    | +0.19%    | 444103.00    | 443058.00    | -0.24%    | PASS    |
| ordinary               | LSMC regression degree=8 (100000 paths)                                          | 851687.00    | 830518.00    | -2.49%    | 852517.00    | 828463.00    | -2.82%    | PASS    |
| ordinary               | LSMC regression degree=8 ITM mask (100000 paths)                                 | 814490.00    | 792241.00    | -2.73%    | 808487.00    | 815099.00    | +0.82%    | PASS    |
| ordinary               | LSMC regression features=2 degree=2 ITM mask (100000 paths)                      | 8974000.00   | 9087000.00   | +1.26%    | 9022000.00   | 9067000.00   | +0.50%    | PASS    |
| ordinary               | LSMC regression features=2 degree=3 ITM mask (100000 paths)                      | 15703000.00  | 15824000.00  | +0.77%    | 15598000.00  | 15696000.00  | +0.63%    | PASS    |
| ordinary               | LSMC regression features=3 degree=3 ITM mask (100000 paths)                      | 39560000.00  | 39047000.00  | -1.30%    | 39286000.00  | 39423000.00  | +0.35%    | PASS    |
| ordinary               | correlated BS path (100K x 12 steps x 1 assets)                                  | 12384000.00  | 12460000.00  | +0.61%    | 12340000.00  | 12432000.00  | +0.75%    | PASS    |
| ordinary               | correlated BS path (100K x 12 steps x 2 assets)                                  | 18377000.00  | 18344000.00  | -0.18%    | 18249000.00  | 18489000.00  | +1.32%    | PASS    |
| ordinary               | correlated BS path (100K x 12 steps x 3 assets)                                  | 25000000.00  | 24671000.00  | -1.32%    | 24527000.00  | 24514000.00  | -0.05%    | PASS    |
| ordinary               | hybrid flat-rate path (100K x 12 steps x 2 assets)                               | 45885000.00  | 45826000.00  | -0.13%    | 45800000.00  | 45513000.00  | -0.63%    | PASS    |
| ordinary               | hybrid logDF path (100K x 12 steps x 2 assets)                                   | 44817000.00  | 45891000.00  | +2.40%    | 45615000.00  | 45470000.00  | -0.32%    | PASS    |
| ordinary               | script engine bermudan exercise double compiled=false (100000 paths x 54 events) | 143310000.00 | 141142000.00 | -1.51%    | 139743000.00 | 144534000.00 | +3.43%    | PASS    |
| ordinary               | script engine bermudan exercise double compiled=true (100000 paths x 54 events)  | 134021000.00 | 131991000.00 | -1.51%    | 129311000.00 | 129407000.00 | +0.07%    | PASS    |
| ordinary               | script engine vanilla Number_ compiled=false (20000 paths x 1 events)            | 823581.00    | 820498.00    | -0.37%    | 805946.00    | 825407.00    | +2.41%    | PASS    |
| ordinary               | script engine vanilla Number_ compiled=true (20000 paths x 1 events)             | 785041.00    | 763269.00    | -2.77%    | 776408.00    | 789740.00    | +1.72%    | PASS    |
| ordinary               | script engine vanilla double compiled=false (200000 paths x 1 events)            | 2196000.00   | 2194000.00   | -0.09%    | 2125000.00   | 2207000.00   | +3.86%    | PASS    |
| ordinary               | script engine vanilla double compiled=true (200000 paths x 1 events)             | 2013000.00   | 2046000.00   | +1.64%    | 2040000.00   | 2040000.00   | +0.00%    | PASS    |
| ordinary               | script engine weekly barrier Number_ compiled=false (10000 paths x 52 events)    | 11336000.00  | 10974000.00  | -3.19%    | 10916000.00  | 11004000.00  | +0.81%    | PASS    |
| ordinary               | script engine weekly barrier Number_ compiled=true (10000 paths x 52 events)     | 9257000.00   | 8927000.00   | -3.56%    | 9127000.00   | 8994000.00   | -1.46%    | PASS    |
| ordinary               | script engine weekly barrier double compiled=false (100000 paths x 52 events)    | 43288000.00  | 43649000.00  | +0.83%    | 43099000.00  | 42664000.00  | -1.01%    | PASS    |
| ordinary               | script engine weekly barrier double compiled=true (100000 paths x 52 events)     | 26465000.00  | 25744000.00  | -2.72%    | 25844000.00  | 25797000.00  | -0.18%    | PASS    |
| gsr-market-calibration | GSR lagged swaption, 512 outer x 32 inner paths                                  | 7118000.00   | 7285000.00   | +2.35%    | 7133000.00   | 7229000.00   | +1.35%    | PASS    |
| gsr-market-calibration | GSR market 4-node fit, 12 expiry/tenor/strike quotes, AAD                        | 89808000.00  | 91196000.00  | +1.55%    | 88671000.00  | 92843000.00  | +4.71%    | PASS    |
| gsr-market-calibration | GSR market 4-node fit, 12 expiry/tenor/strike quotes, FD                         | 25978000.00  | 26518000.00  | +2.08%    | 26096000.00  | 25997000.00  | -0.38%    | PASS    |
| gsr-market-calibration | GSR market 48-node price Jacobian, 12 quotes, AAD                                | 22267000.00  | 23321000.00  | +4.73%    | 22451000.00  | 22546000.00  | +0.42%    | PASS    |
| gsr-market-calibration | GSR market 48-node price Jacobian, 12 quotes, FD                                 | 58322000.00  | 58307000.00  | -0.03%    | 56850000.00  | 57305000.00  | +0.80%    | PASS    |
| gsr-market-calibration | GSR market vol + native curve quote risk, one fitted node                        | 25937000.00  | 26102000.00  | +0.64%    | 26212000.00  | 25237000.00  | -3.72%    | PASS    |
| lsm-bs-tree            | lsm-bs-tree                                                                      | 119253339.00 | 119792218.00 | +0.45%    | 117549364.00 | 118112142.00 | +0.48%    | PASS    |
| lsm-bs-compiled        | lsm-bs-compiled                                                                  | 119747620.00 | 117500442.00 | -1.88%    | 118073459.00 | 117950307.00 | -0.10%    | PASS    |
| lsm-lv-daily           | lsm-lv-daily                                                                     | 586340022.00 | 588395593.00 | +0.35%    | 596241078.00 | 586854921.00 | -1.57%    | PASS    |

## Evidence and reconciliation

Evidence root: `/home/wegamekinglc/.cache/dal-aad-evidence-20261004-8886c083/evidence`.
`aad-pr-fix-production-mc-environment-01.json` freezes inputs and declares the protocol;
`aad-pr-fix-production-mc-confirmation-01/` keeps every raw output, sample, numeric check,
result and summary. `aad-pr-fix-production-mc-after-01.json` proves zero input/head drift
and exact LSM numeric parity. The worker remains the unchanged
`production-profiling-8886c083-frozen-tools-01/run_native_only_mc_pairing.py`.

The earlier original-baseline 43/44 and matched-prefix 42/44 failures remain in
[production profiling](aad-production-profiling.md); their different failed cases are not
relabeled as passes. This complete new confirmation resolves remaining MC acceptance
alongside accepted 25/25 curve confirmation and resource/scaling evidence. The registration
help repair changes only cold worksheet registration and preserves measured native inputs.
Whole-PR review, master reconciliation and exact final-head CI remain required; later
implementation stages remain open.
