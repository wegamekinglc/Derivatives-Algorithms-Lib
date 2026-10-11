# DAL Component Boundaries

The dependency direction is `dal-cpp <- dal-public <- {dal-python, dal-excel}`.
It applies to implementation, examples, benchmarks, tests and their helpers.

- `dal-cpp/`, including `dal-cpp/examples/`, must not include public-facade
  headers or depend on `dal_public` / `DAL::public`, directly or through helpers,
  aliases, include directories or transitive target links.
- Core examples belong in `dal-cpp/examples/` and link `DAL::cpp` (or its
  legacy `dal_library` alias). They must configure and build with the public
  component disabled or absent.
- Examples using public builders, valuation or calibration facades belong in
  `dal-public/examples/`, link `DAL::public`, and are registered by that component.
  Public examples may use core types and helpers in the permitted direction.
- Enabling `DAL_BUILD_PUBLIC` conditionally inside a core example does not make
  a reverse dependency acceptable. Preserve existing core examples in their
  current directories and rewrite facade calls using core APIs, retaining their
  purpose and targets. Do not solve the violation by relocating those examples
  to dal-public. Add separate public examples when facade coverage is needed.

For example/build changes, inspect includes and the CMake target dependency
closure, then configure core examples without public and build the affected
core target. Build and run affected public examples separately. Verify example
options, CTest registration, installed targets and documentation paths. Keep
numerical and performance verification scoped to changed behavior; example-only
API rewrites do not require repeating unchanged library algorithm benchmarks.

The published [architecture](../../docs/architecture.md#components-and-dependency-direction)
describes the same component boundary for library users.
