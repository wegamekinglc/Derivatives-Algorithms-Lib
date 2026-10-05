# Automatic Dupire request C++ performance

Status: bounded legacy-entry acceptance passes. Full F01 bindings, own-head CI
and whole-PR P01 production acceptance remain open.

## Frozen inputs and scope

Baseline is accepted `ea8973b1174d10814cee9bd510016258e3d8e824`, using the frozen
`aad-calibration-request-off-install-01` package. Head uses the new automatic
request source and `aad-dupire-request-off-install-01`. Installed consumers link
only exported `DAL::public`, with its transitive native core. Exact source,
installed headers/archives, binaries/builds and runner contents are frozen by
1,578 before/after hashes. Publication must match those implementation inputs.

The additive native quote-identity and public axis/layout helpers change three
old assets: the core archive and the public Dupire/scalar-risk object members.
`aad-dupire-request-old-input-identity-02.json` proves all nine accepted gate
executables and 17 other old public objects byte-identical (26/29 assets total).
The public archive adds only `dupireriskrequest.cpp.o`. Preserve `identity-01`:
it reused the earlier member inventory; `02` includes the already accepted
common-request member and compares additions with the actual ea897 install.

This identity evidence does not replace old-entry timing. All four affected
installed paths are measured: legacy dictionary valuation, structured scalar
valuation, valuation followed by quote mapping and retained-valuation pullback.
Cases cover flat/Merton, small/medium surfaces, tree/compiled evaluation and
257/2,057 paths. There are 64 old rows and 64 additional new-cost rows.

## Protocol and result

Use the unchanged two confirmation rounds, ten alternating base/head process
samples per round, round minima and 4% policy. Warm-up is three requests; timed
repetitions are ten for 257 paths and three for 2,057. Both sides are Release,
static native, lifetime/profiling OFF, one worker and CPU 4 affinity. Setup,
calibration capture and untimed validation stay outside the timed request.
Native price/quote-gradient/grid identity holds across all base/head/new samples.

All 64 old rows pass. The largest smaller-of-two-round delta is +1.21%; no row
fails the unchanged two-round 4% rule. This is bounded increment acceptance,
not a whole-PR/master or P01 production measurement verdict.

Evidence root: `/home/wegamekinglc/.cache/dal-aad-evidence-20261004-8886c083/evidence`.
`aad-dupire-request-cost-paired-01/` retains protocol, all 3,840 raw process
stdout/stderr captures, 20 complete progress inventories, final samples/results
and unchanged before/after hashes. No build/test/source/Git/PR activity overlaps
measurement. The functional installed pass separately checks 128 processes
across OFF/combined, including every mandatory surface/direct derivative,
three raw contribution matrices, exact combined payload and report factors.

## Old entry rows

| Base/grid     | Evaluator | Paths | Entry      | Base ns (r1/r2)           | Head ns (r1/r2)           | Delta % (r1/r2) | Verdict |
|---------------|-----------|-------|------------|---------------------------|---------------------------|-----------------|---------|
| flat/small    | tree      | 257   | legacy     | 226,497.1 / 225,502.7     | 230,434.3 / 221,639.3     | +1.74 / -1.71   | PASS    |
| flat/small    | tree      | 257   | structured | 245,974.3 / 244,777.3     | 250,789.6 / 244,485.7     | +1.96 / -0.12   | PASS    |
| flat/small    | tree      | 257   | quote      | 309,038.7 / 297,403.6     | 302,742.7 / 299,633.9     | -2.04 / +0.75   | PASS    |
| flat/small    | tree      | 257   | pullback   | 41,684.4 / 40,088.5       | 40,422.9 / 40,179.0       | -3.03 / +0.23   | PASS    |
| flat/small    | tree      | 2057  | legacy     | 1,530,114.0 / 1,519,835.0 | 1,526,026.3 / 1,521,248.0 | -0.27 / +0.09   | PASS    |
| flat/small    | tree      | 2057  | structured | 1,569,574.0 / 1,561,551.3 | 1,623,702.7 / 1,549,696.0 | +3.45 / -0.76   | PASS    |
| flat/small    | tree      | 2057  | quote      | 1,624,877.7 / 1,614,860.7 | 1,595,821.3 / 1,605,348.7 | -1.79 / -0.59   | PASS    |
| flat/small    | tree      | 2057  | pullback   | 42,069.7 / 41,375.3       | 42,563.0 / 41,432.3       | +1.17 / +0.14   | PASS    |
| flat/small    | compiled  | 257   | legacy     | 227,475.3 / 221,128.5     | 219,355.0 / 220,013.1     | -3.57 / -0.50   | PASS    |
| flat/small    | compiled  | 257   | structured | 244,680.7 / 241,165.5     | 243,865.6 / 244,412.8     | -0.33 / +1.35   | PASS    |
| flat/small    | compiled  | 257   | quote      | 304,520.3 / 296,131.0     | 299,287.6 / 293,852.1     | -1.72 / -0.77   | PASS    |
| flat/small    | compiled  | 257   | pullback   | 41,763.5 / 40,380.2       | 42,106.2 / 39,897.4       | +0.82 / -1.20   | PASS    |
| flat/small    | compiled  | 2057  | legacy     | 1,532,764.0 / 1,514,422.3 | 1,493,789.7 / 1,501,115.7 | -2.54 / -0.88   | PASS    |
| flat/small    | compiled  | 2057  | structured | 1,556,062.7 / 1,562,568.0 | 1,536,415.3 / 1,542,780.7 | -1.26 / -1.27   | PASS    |
| flat/small    | compiled  | 2057  | quote      | 1,624,985.7 / 1,591,769.0 | 1,585,471.3 / 1,587,153.7 | -2.43 / -0.29   | PASS    |
| flat/small    | compiled  | 2057  | pullback   | 43,002.3 / 41,599.3       | 41,812.3 / 41,294.7       | -2.77 / -0.73   | PASS    |
| flat/medium   | tree      | 257   | legacy     | 420,805.5 / 414,908.9     | 417,344.1 / 413,441.8     | -0.82 / -0.35   | PASS    |
| flat/medium   | tree      | 257   | structured | 459,308.1 / 444,129.4     | 444,147.2 / 439,345.3     | -3.30 / -1.08   | PASS    |
| flat/medium   | tree      | 257   | quote      | 778,996.4 / 765,076.2     | 772,935.1 / 757,891.0     | -0.78 / -0.94   | PASS    |
| flat/medium   | tree      | 257   | pullback   | 281,720.0 / 274,655.1     | 275,079.1 / 278,422.7     | -2.36 / +1.37   | PASS    |
| flat/medium   | tree      | 2057  | legacy     | 1,822,670.3 / 1,805,647.7 | 1,856,407.0 / 1,791,776.7 | +1.85 / -0.77   | PASS    |
| flat/medium   | tree      | 2057  | structured | 1,880,262.7 / 1,831,835.7 | 1,853,228.7 / 1,830,729.0 | -1.44 / -0.06   | PASS    |
| flat/medium   | tree      | 2057  | quote      | 2,204,275.0 / 2,152,730.0 | 2,176,003.3 / 2,177,990.3 | -1.28 / +1.17   | PASS    |
| flat/medium   | tree      | 2057  | pullback   | 282,048.7 / 272,842.7     | 281,233.7 / 269,970.3     | -0.29 / -1.05   | PASS    |
| flat/medium   | compiled  | 257   | legacy     | 426,809.2 / 416,023.8     | 416,064.8 / 409,468.4     | -2.52 / -1.58   | PASS    |
| flat/medium   | compiled  | 257   | structured | 437,964.4 / 443,259.9     | 453,947.1 / 438,193.2     | +3.65 / -1.14   | PASS    |
| flat/medium   | compiled  | 257   | quote      | 796,812.6 / 763,500.0     | 774,126.6 / 768,804.3     | -2.85 / +0.69   | PASS    |
| flat/medium   | compiled  | 257   | pullback   | 275,093.9 / 272,563.4     | 278,420.0 / 277,162.9     | +1.21 / +1.69   | PASS    |
| flat/medium   | compiled  | 2057  | legacy     | 1,816,122.3 / 1,756,213.0 | 1,835,027.7 / 1,778,661.0 | +1.04 / +1.28   | PASS    |
| flat/medium   | compiled  | 2057  | structured | 1,811,943.7 / 1,790,653.3 | 1,792,235.0 / 1,808,159.7 | -1.09 / +0.98   | PASS    |
| flat/medium   | compiled  | 2057  | quote      | 2,177,064.0 / 2,147,185.3 | 2,139,284.0 / 2,124,590.0 | -1.74 / -1.05   | PASS    |
| flat/medium   | compiled  | 2057  | pullback   | 275,626.7 / 279,342.7     | 275,451.3 / 269,993.3     | -0.06 / -3.35   | PASS    |
| merton/small  | tree      | 257   | legacy     | 229,848.2 / 224,411.4     | 227,151.8 / 227,073.6     | -1.17 / +1.19   | PASS    |
| merton/small  | tree      | 257   | structured | 250,735.5 / 243,756.4     | 253,617.4 / 249,072.4     | +1.15 / +2.18   | PASS    |
| merton/small  | tree      | 257   | quote      | 305,907.1 / 299,540.7     | 305,131.2 / 301,120.8     | -0.25 / +0.53   | PASS    |
| merton/small  | tree      | 257   | pullback   | 40,642.9 / 40,104.8       | 40,170.3 / 40,044.1       | -1.16 / -0.15   | PASS    |
| merton/small  | tree      | 2057  | legacy     | 1,537,146.0 / 1,531,710.3 | 1,595,047.7 / 1,531,287.3 | +3.77 / -0.03   | PASS    |
| merton/small  | tree      | 2057  | structured | 1,597,419.0 / 1,578,756.0 | 1,560,116.7 / 1,539,698.3 | -2.34 / -2.47   | PASS    |
| merton/small  | tree      | 2057  | quote      | 1,660,567.0 / 1,625,006.3 | 1,638,867.7 / 1,611,856.7 | -1.31 / -0.81   | PASS    |
| merton/small  | tree      | 2057  | pullback   | 41,142.3 / 41,017.7       | 41,853.3 / 40,795.0       | +1.73 / -0.54   | PASS    |
| merton/small  | compiled  | 257   | legacy     | 223,681.9 / 221,313.5     | 221,379.3 / 221,532.3     | -1.03 / +0.10   | PASS    |
| merton/small  | compiled  | 257   | structured | 242,452.8 / 241,515.6     | 241,123.9 / 239,872.4     | -0.55 / -0.68   | PASS    |
| merton/small  | compiled  | 257   | quote      | 302,349.0 / 296,324.8     | 309,031.6 / 292,641.8     | +2.21 / -1.24   | PASS    |
| merton/small  | compiled  | 257   | pullback   | 40,663.6 / 40,065.1       | 40,221.3 / 39,839.3       | -1.09 / -0.56   | PASS    |
| merton/small  | compiled  | 2057  | legacy     | 1,521,453.3 / 1,501,195.7 | 1,517,129.3 / 1,508,746.7 | -0.28 / +0.50   | PASS    |
| merton/small  | compiled  | 2057  | structured | 1,551,767.7 / 1,526,566.3 | 1,582,538.3 / 1,539,664.0 | +1.98 / +0.86   | PASS    |
| merton/small  | compiled  | 2057  | quote      | 1,582,361.0 / 1,600,890.0 | 1,625,983.3 / 1,569,361.0 | +2.76 / -1.97   | PASS    |
| merton/small  | compiled  | 2057  | pullback   | 41,186.7 / 41,262.3       | 41,303.7 / 41,771.3       | +0.28 / +1.23   | PASS    |
| merton/medium | tree      | 257   | legacy     | 424,885.6 / 410,199.4     | 415,330.8 / 410,875.1     | -2.25 / +0.16   | PASS    |
| merton/medium | tree      | 257   | structured | 446,664.1 / 438,623.3     | 438,346.6 / 444,656.8     | -1.86 / +1.38   | PASS    |
| merton/medium | tree      | 257   | quote      | 777,419.3 / 771,314.3     | 792,442.8 / 774,454.1     | +1.93 / +0.41   | PASS    |
| merton/medium | tree      | 257   | pullback   | 277,983.1 / 276,504.4     | 271,538.0 / 268,937.9     | -2.32 / -2.74   | PASS    |
| merton/medium | tree      | 2057  | legacy     | 1,857,580.3 / 1,779,399.3 | 1,861,251.3 / 1,775,208.0 | +0.20 / -0.24   | PASS    |
| merton/medium | tree      | 2057  | structured | 1,854,527.0 / 1,833,875.7 | 1,868,095.7 / 1,827,767.3 | +0.73 / -0.33   | PASS    |
| merton/medium | tree      | 2057  | quote      | 2,246,469.3 / 2,159,464.7 | 2,165,420.3 / 2,178,819.3 | -3.61 / +0.90   | PASS    |
| merton/medium | tree      | 2057  | pullback   | 274,848.0 / 279,296.0     | 280,520.0 / 270,793.7     | +2.06 / -3.04   | PASS    |
| merton/medium | compiled  | 257   | legacy     | 422,675.9 / 406,404.1     | 414,972.5 / 406,451.5     | -1.82 / +0.01   | PASS    |
| merton/medium | compiled  | 257   | structured | 449,220.8 / 443,919.3     | 457,385.9 / 444,541.2     | +1.82 / +0.14   | PASS    |
| merton/medium | compiled  | 257   | quote      | 785,814.9 / 784,055.7     | 794,681.0 / 781,018.0     | +1.13 / -0.39   | PASS    |
| merton/medium | compiled  | 257   | pullback   | 277,440.4 / 271,506.3     | 277,265.8 / 269,953.5     | -0.06 / -0.57   | PASS    |
| merton/medium | compiled  | 2057  | legacy     | 1,750,082.7 / 1,765,797.0 | 1,792,679.3 / 1,741,404.0 | +2.43 / -1.38   | PASS    |
| merton/medium | compiled  | 2057  | structured | 1,833,877.0 / 1,814,978.3 | 1,809,776.7 / 1,772,029.3 | -1.31 / -2.37   | PASS    |
| merton/medium | compiled  | 2057  | quote      | 2,201,626.7 / 2,141,161.0 | 2,181,883.0 / 2,126,020.3 | -0.90 / -0.71   | PASS    |
| merton/medium | compiled  | 2057  | pullback   | 274,745.7 / 274,943.3     | 273,708.7 / 269,201.7     | -0.38 / -2.09   | PASS    |

## Informational new request rows

`automatic` executes a prepared owning plan; `empty` keeps the same native
valuation/full quote payload with no quote projection; `plan` seals and validates
an owning request; `getter` returns a detached reported projection. The consumer
validates numeric parity around timed execution. These are full consumer request
costs, including its validation/checksum work, rather than isolated primitive
latencies. They add no new threshold or claim of old-path improvement.

| Base/grid     | Evaluator | Paths | Entry     | New ns (r1/r2)            |
|---------------|-----------|-------|-----------|---------------------------|
| flat/small    | tree      | 257   | automatic | 311,063.7 / 304,896.7     |
| flat/small    | tree      | 257   | empty     | 308,023.3 / 304,034.4     |
| flat/small    | tree      | 257   | plan      | 40,039.2 / 39,563.3       |
| flat/small    | tree      | 257   | getter    | 45.1 / 43.4               |
| flat/small    | tree      | 2057  | automatic | 1,655,485.7 / 1,605,945.3 |
| flat/small    | tree      | 2057  | empty     | 1,656,276.7 / 1,609,491.7 |
| flat/small    | tree      | 2057  | plan      | 42,217.3 / 41,730.3       |
| flat/small    | tree      | 2057  | getter    | 69.7 / 63.7               |
| flat/small    | compiled  | 257   | automatic | 317,307.4 / 306,303.3     |
| flat/small    | compiled  | 257   | empty     | 302,532.8 / 303,633.2     |
| flat/small    | compiled  | 257   | plan      | 41,318.3 / 39,564.9       |
| flat/small    | compiled  | 257   | getter    | 43.0 / 43.1               |
| flat/small    | compiled  | 2057  | automatic | 1,612,576.7 / 1,589,170.7 |
| flat/small    | compiled  | 2057  | empty     | 1,696,797.7 / 1,586,871.3 |
| flat/small    | compiled  | 2057  | plan      | 42,603.7 / 41,443.7       |
| flat/small    | compiled  | 2057  | getter    | 70.0 / 61.3               |
| flat/medium   | tree      | 257   | automatic | 928,573.8 / 899,334.1     |
| flat/medium   | tree      | 257   | empty     | 926,943.3 / 898,913.5     |
| flat/medium   | tree      | 257   | plan      | 185,078.0 / 181,107.0     |
| flat/medium   | tree      | 257   | getter    | 43.2 / 42.5               |
| flat/medium   | tree      | 2057  | automatic | 2,319,707.0 / 2,291,145.0 |
| flat/medium   | tree      | 2057  | empty     | 2,346,674.0 / 2,277,348.7 |
| flat/medium   | tree      | 2057  | plan      | 193,108.0 / 181,835.7     |
| flat/medium   | tree      | 2057  | getter    | 65.0 / 69.0               |
| flat/medium   | compiled  | 257   | automatic | 907,954.9 / 883,519.6     |
| flat/medium   | compiled  | 257   | empty     | 892,322.0 / 886,142.0     |
| flat/medium   | compiled  | 257   | plan      | 189,750.9 / 180,481.7     |
| flat/medium   | compiled  | 257   | getter    | 43.7 / 41.3               |
| flat/medium   | compiled  | 2057  | automatic | 2,321,860.3 / 2,230,861.7 |
| flat/medium   | compiled  | 2057  | empty     | 2,275,857.3 / 2,268,858.7 |
| flat/medium   | compiled  | 2057  | plan      | 185,206.3 / 183,145.7     |
| flat/medium   | compiled  | 2057  | getter    | 68.0 / 61.3               |
| merton/small  | tree      | 257   | automatic | 307,870.5 / 307,935.2     |
| merton/small  | tree      | 257   | empty     | 303,462.9 / 305,893.4     |
| merton/small  | tree      | 257   | plan      | 40,544.4 / 39,403.2       |
| merton/small  | tree      | 257   | getter    | 42.7 / 43.0               |
| merton/small  | tree      | 2057  | automatic | 1,623,949.7 / 1,607,194.3 |
| merton/small  | tree      | 2057  | empty     | 1,641,681.7 / 1,605,346.0 |
| merton/small  | tree      | 2057  | plan      | 43,096.7 / 41,117.3       |
| merton/small  | tree      | 2057  | getter    | 67.0 / 68.0               |
| merton/small  | compiled  | 257   | automatic | 305,962.1 / 303,713.4     |
| merton/small  | compiled  | 257   | empty     | 304,058.6 / 299,675.2     |
| merton/small  | compiled  | 257   | plan      | 39,935.5 / 39,829.8       |
| merton/small  | compiled  | 257   | getter    | 43.8 / 43.6               |
| merton/small  | compiled  | 2057  | automatic | 1,596,576.3 / 1,580,293.0 |
| merton/small  | compiled  | 2057  | empty     | 1,584,217.0 / 1,587,797.7 |
| merton/small  | compiled  | 2057  | plan      | 42,463.0 / 41,355.3       |
| merton/small  | compiled  | 2057  | getter    | 69.0 / 64.0               |
| merton/medium | tree      | 257   | automatic | 882,368.9 / 875,579.9     |
| merton/medium | tree      | 257   | empty     | 870,872.8 / 882,332.8     |
| merton/medium | tree      | 257   | plan      | 181,185.1 / 181,325.8     |
| merton/medium | tree      | 257   | getter    | 44.7 / 41.7               |
| merton/medium | tree      | 2057  | automatic | 2,336,797.3 / 2,278,132.3 |
| merton/medium | tree      | 2057  | empty     | 2,284,295.0 / 2,252,861.7 |
| merton/medium | tree      | 2057  | plan      | 186,796.7 / 181,856.0     |
| merton/medium | tree      | 2057  | getter    | 69.3 / 70.0               |
| merton/medium | compiled  | 257   | automatic | 884,380.4 / 881,586.5     |
| merton/medium | compiled  | 257   | empty     | 925,870.8 / 894,182.8     |
| merton/medium | compiled  | 257   | plan      | 189,633.5 / 182,919.2     |
| merton/medium | compiled  | 257   | getter    | 44.4 / 43.2               |
| merton/medium | compiled  | 2057  | automatic | 2,289,033.0 / 2,232,130.0 |
| merton/medium | compiled  | 2057  | empty     | 2,270,763.3 / 2,273,767.3 |
| merton/medium | compiled  | 2057  | plan      | 192,403.7 / 178,508.0     |
| merton/medium | compiled  | 2057  | getter    | 68.3 / 68.7               |
