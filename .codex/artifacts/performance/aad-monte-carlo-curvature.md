# Scoped cost selection: Monte Carlo curvature

Status: selected before measurement; no timings or acceptance claimed yet.

| Case                       | Compared requests                                                   | Purpose                                                                    |
|----------------------------|---------------------------------------------------------------------|----------------------------------------------------------------------------|
| Spot Gamma                 | 65 common one-step paths, one spot direction, one worker            | Small financial-entry overhead against equivalent manual segmented secants |
| Mixed HVP                  | 65 common 16-step paths, three model/script directions, two workers | Streamed financial request/resource behavior without a large matrix        |
| Existing generic curvature | Accepted smooth polynomial driver and identical request             | Regression control for moving its shared numerical loop                    |

Both financial implementations must use the same preparation, RNG settings,
path interval, point, steps, mode guard and output consumption. Use accepted
master source and immutable archive-member provenance. Record source/object/
binary hashes, environment, raw interleaved observations and two rounds.
Existing caller threshold is +4% in both rounds under the established protocol;
financial-entry overhead is descriptive. Expand only a failed/noisy affected
case, never the unrelated full benchmark collection. Timing remains read-only.
