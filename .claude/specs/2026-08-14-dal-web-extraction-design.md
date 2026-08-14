# dal-web Extraction Design

Date: 2026-08-14
Status: approved by user (2026-08-14)
Approach: A — three-stage, validate before delete

## Goal

Extract `dal-web/` from Derivatives-Algorithms-Lib into a standalone GitHub
project `wegamekinglc/dal-web` that depends on the published PyPI package
`dal-python>=2026.8.14`, then remove all dal-web content from the parent repo.

Confirmed decisions (user, 2026-08-14):

- Target: new GitHub repository `wegamekinglc/dal-web`
- History: clean start (current master snapshot, no git filter-repo)
- Dependency: PyPI `dal-python>=2026.8.14` as a declared backend dependency;
  source-build override stays documented for development against unreleased DAL
- Parent repo: full removal — `dal-web/`, CI web jobs, and all doc/agent/skill
  references

## Why PyPI dependency is safe today

- dal-python 2026.8.14 on PyPI was cut from current master HEAD, so there is no
  unreleased-feature gap.
- The release ships cp313 wheels for Linux and Windows; the backend requires
  Python >= 3.13.
- `native_runtime.py` preflight symbols — including the private
  `_RequiredHistoricalRateTradeFixings` / `_RequiredHistoricalXccyFixings` — are
  bound in `dal-python/src/bindings/curve.cpp` and present in the published
  wheel.

## Stage 1 — build and validate the standalone repo locally

New repo location during construction: `workspace/dal-web/` (sibling of
Derivatives-Algorithms-Lib, inside the git-ignored workspace directory).

### Layout

```
dal-web/                          # new repo root = current dal-web/ content shifted up one level
├── .github/workflows/ci.yml      # new: backend + frontend + e2e
├── .claude/rules/                # dal-web-design.md, dal-web-code-style.md (moved)
├── .claude/skills/dal-web-setup/ # moved, paths adapted
├── .claude/specs/                # dal-web-db-persistence.md (moved)
├── backend/                      # snapshot (87 tracked files)
├── frontend/                     # snapshot (51 tracked files)
├── scripts/                      # snapshot (7 files)
├── .gitignore                    # current dal-web/.gitignore + chrome/ entries
├── LICENSE                       # MIT, same as parent repo
└── README.md                     # rewritten as a standalone project
```

Snapshot source: `git ls-files dal-web` (147 files; no build artifacts,
`egg-info`, `node_modules`, or `.data` are tracked).

### Core adaptations in the new repo

1. **Dependency declaration** — `backend/pyproject.toml` `dependencies` gains
   `dal-python>=2026.8.14`; `backend/requirements.txt` matches.
2. **`native_runtime.py`** — `_failure_message` points at
   `uv pip install "dal-python>=2026.8.14"`; README keeps a source-build
   override recipe (`--config-settings=cmake.define.DAL_INSTALL_PREFIX=...`)
   for development against unreleased DAL.
3. **Script path rewrite** — `scripts/start.sh`, `stop.sh`, `start.ps1`,
   `stop.ps1`, `setup-playwright.sh`, `playwright-start.sh`,
   `playwright-test-backend-start.sh` currently assume `scripts/` sits two
   levels below the repo root (`SCRIPT_DIR/../..`) and prefix app dirs with
   `dal-web/`. In the new layout `scripts/` is top-level: repo root becomes
   `SCRIPT_DIR/..`, app dirs become `backend` / `frontend`. The dal-package
   preflight changes from "check the DAL stage directory" to "the backend venv
   already contains `dal` after `uv sync`".
4. **Backend test harness** — conftest / subprocess fixtures referencing
   parent-repo paths are adapted; `native`-marked tests run by default since
   the PyPI wheel is the real package.
5. No backend/frontend business-code changes.

### New repo CI (`.github/workflows/ci.yml`)

Three jobs, all `ubuntu-24.04` (matches current parent-repo coverage; no
Windows/macOS jobs — the parent never had them for dal-web):

- **backend** — `uv sync` → `ruff check` → `pytest` (full suite incl. `native`)
- **frontend** — `npm ci` → `npm test` → `npm run build`
- **e2e** — `scripts/setup-playwright.sh` → start backend →
  `npm run test:e2e`

### Local validation gate (must pass before pushing)

1. `uv sync` resolves `dal-python==2026.8.14` from PyPI
2. `pytest` full suite (including `native` marker)
3. `npm ci`, `npm test`, `npm run build`
4. `scripts/start.sh` boots both services; `npm run test:e2e` passes;
   `scripts/stop.sh` cleans up

## Stage 2 — publish the new repo

- `gh repo create wegamekinglc/dal-web --public` (public, mirroring the parent
  repo), push `master`
- Confirm the new repo's CI is green on the pushed commit

## Stage 3 — parent-repo cleanup PR

One PR to `master`:

- Delete `dal-web/` (147 tracked files)
- Delete `.github/workflows/web-calibration-performance.yml`
- Remove the "Web backend, frontend, and browser" job from
  `.github/workflows/cmake-linux.yml`
- Delete migrated items: `.claude/rules/dal-web-design.md`,
  `.claude/rules/dal-web-code-style.md`, `.claude/skills/dal-web-setup/`,
  `.claude/specs/dal-web-db-persistence.md`, `.codex/skills/dal-web/`
- Scrub references in: `CLAUDE.md` (architecture map, web-UI section, rules
  links), `AGENTS.md`, `README.md`, `CONTRIBUTING.md`,
  `docs/installation.md` (backend-environment section),
  `.claude/agents/dal-tester.md` (dal-web e2e passages),
  `.codex/README.md`, `.codex/skills/dal-agent-team/references/shared-rules.md`
- `CHANGELOG.md` entry (removal of a public surface = fundamental change)

Cleanup PR validation: `bash ./build_linux.sh` + ctest green; CI green on the
exact PR head.

## Explicitly out of scope (YAGNI)

- No performance-regression workflow in the new repo
  (`web-calibration-performance.yml` depends on the parent source build and a
  self-hosted runner; it is deleted, not migrated). The
  `backend/performance/*.json` baseline files travel with the snapshot.
- No Windows/macOS CI for the new repo.
- No git history migration.
- No changes to dal-cpp / dal-public / dal-python / dal-excel.
