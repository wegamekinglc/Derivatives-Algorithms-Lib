# dal-web Extraction Implementation Plan

> **Artifact status: implemented history.** All phases of this plan shipped (merged
> as PR #290, 2026-08-15); the standalone repository is live at
> [wegamekinglc/dal-web](https://github.com/wegamekinglc/dal-web). Task checkboxes,
> local paths, and commands below describe the execution baseline and are retained
> as historical evidence, not pending work.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move `dal-web/` out of Derivatives-Algorithms-Lib into a standalone `wegamekinglc/dal-web` GitHub repo that depends on PyPI `dal-python>=2026.8.14`, then remove all dal-web content from the parent repo.

**Architecture:** Three stages per the approved spec (`.claude/specs/2026-08-14-dal-web-extraction-design.md`): (1) build + validate the standalone repo locally at `workspace/dal-web/`, (2) publish it to GitHub, (3) one parent-repo cleanup PR on branch `chore/extract-dal-web` (already created, carries the spec + this plan).

**Tech Stack:** FastAPI + React/Vite, uv, npm, GitHub Actions, gh CLI.

**Path variables used throughout:**
- `PARENT=/home/wegamekinglc/dev/github/my-claude/workspace/Derivatives-Algorithms-Lib`
- `NEW=/home/wegamekinglc/dev/github/my-claude/workspace/dal-web`

---

## Phase 1 — Standalone repo skeleton

### Task 1: Snapshot dal-web into a fresh git repo

**Files:**
- Create: `$NEW/` (whole tree), `$NEW/LICENSE`, append to `$NEW/.gitignore`

- [ ] **Step 1: Export the tracked snapshot (147 files) and init the repo**

```bash
PARENT=/home/wegamekinglc/dev/github/my-claude/workspace/Derivatives-Algorithms-Lib
NEW=/home/wegamekinglc/dev/github/my-claude/workspace/dal-web
mkdir -p "$NEW"
cd "$PARENT"
git archive master dal-web | tar -x --strip-components=1 -C "$NEW"
cp LICENSE "$NEW/LICENSE"
cd "$NEW"
git init -b main
```

`git archive master dal-web | tar -x --strip-components=1` shifts `dal-web/backend/...` to `backend/...` etc. Only tracked files are exported (no `egg-info`, `node_modules`, `.data`).

- [ ] **Step 2: Verify the snapshot**

```bash
cd "$NEW"
find . -type f | wc -l          # expect 148 (147 + LICENSE)
ls                              # expect: LICENSE  backend  frontend  scripts  README.md  .gitignore
test -f backend/pyproject.toml && test -f frontend/package.json && test -f scripts/start.sh && echo OK
```

- [ ] **Step 3: Extend .gitignore with the Playwright Chrome dirs**

Append to `$NEW/.gitignore`:

```gitignore

# Playwright local Chrome (setup-playwright.sh)
chrome/
chrome-libs/
```

- [ ] **Step 4: Initial commit**

```bash
cd "$NEW"
git add -A
git commit -m "chore: initial import of dal-web from Derivatives-Algorithms-Lib

Snapshot of dal-web/ at Derivatives-Algorithms-Lib master (c6b17090),
re-rooted one level up. History intentionally not migrated."
```

---

## Phase 2 — Adaptation

### Task 2: Declare the PyPI dal-python dependency

**Files:**
- Modify: `$NEW/backend/pyproject.toml` (dependencies list)
- Modify: `$NEW/backend/requirements.txt`
- Regenerate: `$NEW/backend/uv.lock`

- [ ] **Step 1: Add dal-python to pyproject dependencies**

In `$NEW/backend/pyproject.toml`, change the `dependencies` block to:

```toml
dependencies = [
    "dal-python>=2026.8.14",
    "fastapi>=0.110",
    "uvicorn[standard]>=0.29",
    "pydantic>=2.6",
    "sqlalchemy>=2.0",
    "alembic",
]
```

- [ ] **Step 2: Add dal-python to requirements.txt**

In `$NEW/backend/requirements.txt`, add as the first dependency line (before `fastapi`):

```
dal-python>=2026.8.14
```

- [ ] **Step 3: Regenerate the lockfile and sync**

```bash
cd "$NEW/backend"
uv lock
uv sync --inexact
```

- [ ] **Step 4: Verify the wheel resolved from PyPI and imports**

```bash
cd "$NEW/backend"
uv run --no-sync python -c "import dal; print(dal.__version__)"   # expect 2026.8.14
uv run --no-sync python -m app.native_runtime                      # preflight passes, no output
uv pip show dal-python | head -3                                   # Name: dal-python / Version: 2026.8.14
```

- [ ] **Step 5: Commit**

```bash
cd "$NEW"
git add backend/pyproject.toml backend/requirements.txt backend/uv.lock
git commit -m "feat: depend on published dal-python from PyPI

The native binding is now a declared dependency (dal-python>=2026.8.14)
instead of a manual source build against a DAL stage directory."
```

### Task 3: Repoint native_runtime failure guidance at PyPI

**Files:**
- Modify: `$NEW/backend/app/native_runtime.py` (`_failure_message`)
- Modify: `$NEW/backend/tests/test_native_runtime.py` (message assertions)

- [ ] **Step 1: Rewrite `_failure_message`**

In `$NEW/backend/app/native_runtime.py`, replace the whole `_failure_message` function with:

```python
def _failure_message(reason: str) -> str:
    return (
        "Native DAL Python package is required by dal-web but could not be loaded.\n"
        "It is a declared dependency; install the environment and retry:\n"
        "  cd backend && uv sync\n"
        "To develop against an unreleased DAL build, install from a DAL source "
        "checkout instead:\n"
        "  uv pip install /path/to/Derivatives-Algorithms-Lib/dal-python "
        '"--config-settings=cmake.define.DAL_INSTALL_PREFIX='
        '/path/to/build/stage/<platform-preset>"\n'
        "See README.md#native-dal-package.\n"
        f"Underlying error: {reason}"
    )
```

- [ ] **Step 2: Update the message test**

In `$NEW/backend/tests/test_native_runtime.py`, replace the assertion block in `test_native_preflight_reports_install_command` (lines 23-33) with:

```python
    message = str(error.value)
    assert "Native DAL Python package is required" in message  # nosec B101
    assert "uv sync" in message  # nosec B101
    assert (  # nosec B101
        "--config-settings=cmake.define.DAL_INSTALL_PREFIX="
        "/path/to/build/stage/<platform-preset>"
    ) in message
    assert "Release-linux" not in message  # nosec B101
    assert "--no-build-isolation" not in message  # nosec B101
    assert "README.md#native-dal-package" in message  # nosec B101
    assert "No module named 'dal'" in message  # nosec B101
```

- [ ] **Step 3: Run the test**

```bash
cd "$NEW/backend"
uv run --no-sync pytest tests/test_native_runtime.py -v
```

Expected: 3 passed.

- [ ] **Step 4: Commit**

```bash
cd "$NEW"
git add backend/app/native_runtime.py backend/tests/test_native_runtime.py
git commit -m "docs: point native preflight failure guidance at the PyPI dependency"
```

### Task 4: Re-root the bash scripts

**Files:**
- Modify: `$NEW/scripts/start.sh`, `stop.sh`, `setup-playwright.sh`, `playwright-start.sh`, `playwright-test-backend-start.sh`

The scripts assume `scripts/` sits two levels below the repo root (`SCRIPT_DIR/../..`) and prefix app dirs with `dal-web/`. In the new layout `scripts/` is top-level.

- [ ] **Step 1: Apply the mechanical rewrite**

```bash
cd "$NEW/scripts"
sed -i 's|${SCRIPT_DIR}/\.\./\.\.|${SCRIPT_DIR}/..|g' start.sh stop.sh setup-playwright.sh playwright-start.sh playwright-test-backend-start.sh
sed -i 's|dal-web/scripts|scripts|g; s|dal-web/backend|backend|g; s|dal-web/frontend|frontend|g' start.sh stop.sh setup-playwright.sh playwright-start.sh playwright-test-backend-start.sh
```

- [ ] **Step 2: Verify no stale references and valid syntax**

```bash
cd "$NEW/scripts"
! grep -n "dal-web" start.sh stop.sh setup-playwright.sh playwright-start.sh playwright-test-backend-start.sh && echo NO-STALE-REFS
! grep -n '\.\./\.\.' start.sh stop.sh setup-playwright.sh playwright-start.sh playwright-test-backend-start.sh && echo NO-DOUBLE-DOT
for f in *.sh; do bash -n "$f" || exit 1; done && echo SYNTAX-OK
grep -n 'REPO_ROOT=' start.sh   # expect: REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
grep -n 'BACKEND_DIR=' start.sh # expect: BACKEND_DIR="backend"
```

- [ ] **Step 3: Commit**

```bash
cd "$NEW"
git add scripts/*.sh
git commit -m "refactor: re-root bash launchers for the standalone layout

scripts/ now sits at the repo root, so REPO_ROOT is SCRIPT_DIR/.. and the
app dirs are backend/ and frontend/ without a dal-web/ prefix."
```

### Task 5: Re-root the PowerShell scripts (comments/messages only)

**Files:**
- Modify: `$NEW/scripts/start.ps1`, `stop.ps1`

The path logic already ports: `$WebRoot = Split-Path -Parent $ScriptDir` resolves to the new repo root, and `backend`/`frontend` sit directly under it. Only comments and user-facing messages mention `dal-web/`.

- [ ] **Step 1: Rewrite references and the WebRoot comment**

```bash
cd "$NEW/scripts"
sed -i 's|dal-web/scripts|scripts|g; s|dal-web/backend|backend|g; s|dal-web/frontend|frontend|g' start.ps1 stop.ps1
sed -i 's|# dal-web$|# repo root|' start.ps1 stop.ps1
```

- [ ] **Step 2: Verify**

```bash
cd "$NEW/scripts"
! grep -n "dal-web" start.ps1 stop.ps1 && echo NO-STALE-REFS
grep -n 'WebRoot' start.ps1 | head -2
# expect: $WebRoot      = Split-Path -Parent $ScriptDir        # repo root
```

- [ ] **Step 3: Commit**

```bash
cd "$NEW"
git add scripts/*.ps1
git commit -m "refactor: re-root PowerShell launcher comments for the standalone layout"
```

### Task 6: Fix playwright.config.ts repoRoot

**Files:**
- Modify: `$NEW/frontend/playwright.config.ts`

- [ ] **Step 1: Apply the three edits**

In `$NEW/frontend/playwright.config.ts`:

1. `const repoRoot = resolve(frontendDir, "..", "..");` → `const repoRoot = resolve(frontendDir, "..");`
2. Comment `// \`dal-web/scripts/setup-playwright.sh\` downloads Chrome` → `// \`scripts/setup-playwright.sh\` downloads Chrome`
3. Error message `Run ./dal-web/scripts/setup-playwright.sh first.` → `Run ./scripts/setup-playwright.sh first.`

(`webServer.command` uses `../scripts/playwright-*.sh`, which still resolves: `frontend/` and `scripts/` remain siblings.)

- [ ] **Step 2: Verify**

```bash
cd "$NEW/frontend"
! grep -n "dal-web" playwright.config.ts && echo NO-STALE-REFS
grep -n 'repoRoot = ' playwright.config.ts   # expect resolve(frontendDir, "..")
node --input-type=module -e "console.log('parse skip')"  # config is type-checked by tsc in Task 11
```

- [ ] **Step 3: Commit**

```bash
cd "$NEW"
git add frontend/playwright.config.ts
git commit -m "fix: resolve repo root one level up from frontend/ in playwright config"
```

### Task 7: Port the CI gate scripts into the new repo

**Files:**
- Create: `$NEW/.github/scripts/check_frontend_quote_bump_boundary.py` (ported)
- Create: `$NEW/.github/scripts/generate_web_calibration_perf_reports.py` (ported)
- Create: `$NEW/.github/scripts/check_native_web_gateway.py` (ported)
- Create: `$NEW/.github/scripts/check_consistency.py` (new, small)

- [ ] **Step 1: Copy the three scripts and drop the `dal-web` path component**

```bash
PARENT=/home/wegamekinglc/dev/github/my-claude/workspace/Derivatives-Algorithms-Lib
NEW=/home/wegamekinglc/dev/github/my-claude/workspace/dal-web
mkdir -p "$NEW/.github/scripts"
cp "$PARENT/.github/scripts/check_frontend_quote_bump_boundary.py" \
   "$PARENT/.github/scripts/generate_web_calibration_perf_reports.py" \
   "$PARENT/.github/scripts/check_native_web_gateway.py" \
   "$NEW/.github/scripts/"
cd "$NEW/.github/scripts"
sed -i 's|ROOT / "dal-web" / "frontend" / "src"|ROOT / "frontend" / "src"|' check_frontend_quote_bump_boundary.py
sed -i 's|ROOT / "dal-web" / "backend" / "performance"|ROOT / "backend" / "performance"|' generate_web_calibration_perf_reports.py
sed -i 's|root / "dal-web" / "backend"|root / "backend"|' check_native_web_gateway.py
```

`ROOT`/`root` is `Path(__file__).resolve().parents[2]` in all three — `.github/scripts/x.py` → repo root, unchanged in the new layout.

- [ ] **Step 2: Verify no stale references remain in the ported scripts**

```bash
cd "$NEW/.github/scripts"
! grep -n "dal-web" *.py && echo NO-STALE-REFS
```

If any non-path `dal-web` strings remain (e.g. prose), leave them only if they name the project; the grep must show no `dal-web/` path forms.

- [ ] **Step 3: Write check_consistency.py**

Create `$NEW/.github/scripts/check_consistency.py` with exactly:

```python
#!/usr/bin/env python3
"""dal-web consistency gates: requirement-file sync and launcher portability."""

from __future__ import annotations

import re
import sys
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BACKEND = ROOT / "backend"


def _requirement_name(requirement: str) -> str:
    return re.split(r"[<>=!~;\[]", requirement, maxsplit=1)[0].strip().lower()


def _declared_requirements() -> set[str]:
    with (BACKEND / "pyproject.toml").open("rb") as stream:
        config = tomllib.load(stream)
    return {_requirement_name(d) for d in config["project"]["dependencies"]}


def _requirements_file() -> set[str]:
    names: set[str] = set()
    for line in (BACKEND / "requirements.txt").read_text(encoding="utf-8").splitlines():
        stripped = line.strip()
        if stripped and not stripped.startswith("#"):
            names.add(_requirement_name(stripped))
    return names


def check_requirement_sets(errors: list[str]) -> None:
    declared = _declared_requirements()
    recorded = _requirements_file()
    missing = declared - recorded
    extra = recorded - declared
    if missing:
        errors.append(
            "backend/requirements.txt: missing dependencies declared by "
            f"pyproject.toml: {sorted(missing)}"
        )
    if extra:
        errors.append(
            "backend/requirements.txt: dependencies absent from pyproject.toml: "
            f"{sorted(extra)}"
        )


def check_launcher_portability(errors: list[str]) -> None:
    start = (ROOT / "scripts" / "start.sh").read_text(encoding="utf-8")
    stop = (ROOT / "scripts" / "stop.sh").read_text(encoding="utf-8")
    if "ss -tln" in start and "lsof" not in start:
        errors.append("scripts: macOS launchers must not require Linux-only ss")
    if "xargs -r" in stop:
        errors.append("scripts/stop.sh: GNU-only xargs -r is not macOS portable")
    if re.search(r"\bseq\b", start + stop):
        errors.append("scripts: macOS launchers must not require GNU/Coreutils seq")


def main() -> int:
    errors: list[str] = []
    check_requirement_sets(errors)
    check_launcher_portability(errors)
    for error in errors:
        print(error, file=sys.stderr)
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
```

- [ ] **Step 4: Run the gates locally**

```bash
cd "$NEW"
python3 .github/scripts/check_consistency.py && echo CONSISTENCY-OK
python3 .github/scripts/check_frontend_quote_bump_boundary.py && echo BOUNDARY-OK
```

- [ ] **Step 5: Commit**

```bash
cd "$NEW"
git add .github/scripts/
git commit -m "ci: port web gate scripts from the parent repository

check_frontend_quote_bump_boundary, generate_web_calibration_perf_reports,
and check_native_web_gateway keep their logic with paths re-rooted;
check_consistency.py carries the requirement-sync and launcher-portability
checks formerly in the parent's check_docs.py."
```

### Task 8: New repo CI workflow

**Files:**
- Create: `$NEW/.github/workflows/ci.yml`

- [ ] **Step 1: Write the workflow**

Create `$NEW/.github/workflows/ci.yml` with exactly:

```yaml
name: CI

on:
  pull_request:
  push:
    branches: [main]
  workflow_dispatch:

permissions:
  contents: read

jobs:
  backend:
    name: Backend (pytest incl. native, lint, gates)
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@3d3c42e5aac5ba805825da76410c181273ba90b1

      - uses: astral-sh/setup-uv@c771a70e6277c0a99b617c7a806ffedaca235ff9 # @v9.0.0

      - name: Test and lint backend
        working-directory: backend
        run: |
          uv sync --locked --inexact
          uv run --no-sync pytest
          uv run --no-sync pytest -m native
          uv run --no-sync ruff check .
          uv run --no-sync ruff format --check .

      - name: Native DAL web gateway smoke
        env:
          DAL_NUM_THREADS: 2
        run: python .github/scripts/check_native_web_gateway.py

      - name: Consistency and boundary gates
        run: |
          python .github/scripts/check_consistency.py
          python .github/scripts/check_frontend_quote_bump_boundary.py
          uv run --project backend --no-sync \
            python .github/scripts/generate_web_calibration_perf_reports.py

      - name: OpenAPI snapshot is fresh
        working-directory: backend
        run: |
          uv run --no-sync python scripts/generate_openapi.py
          git diff --exit-code openapi/dal-web.openapi.json

  frontend:
    name: Frontend (build + unit tests)
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@3d3c42e5aac5ba805825da76410c181273ba90b1

      - uses: actions/setup-node@820762786026740c76f36085b0efc47a31fe5020 # @v7.0.0
        with:
          node-version: 20
          cache: npm
          cache-dependency-path: frontend/package-lock.json

      - name: Build and test
        working-directory: frontend
        run: |
          npm ci
          npm run build
          npm test

  e2e:
    name: Browser smoke (Playwright)
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@3d3c42e5aac5ba805825da76410c181273ba90b1

      - uses: astral-sh/setup-uv@c771a70e6277c0a99b617c7a806ffedaca235ff9 # @v9.0.0

      - uses: actions/setup-node@820762786026740c76f36085b0efc47a31fe5020 # @v7.0.0
        with:
          node-version: 20
          cache: npm
          cache-dependency-path: frontend/package-lock.json

      - name: Install dependencies
        run: |
          (cd backend && uv sync --locked --inexact)
          (cd frontend && npm ci)

      - name: Prepare Chrome
        run: ./scripts/setup-playwright.sh

      - name: Run browser smoke tests
        env:
          DAL_PLAYWRIGHT_TEST_BACKEND: 1
        working-directory: frontend
        run: npm run test:e2e
```

- [ ] **Step 2: Validate the YAML**

```bash
cd "$NEW"
python3 -c "import yaml; yaml.safe_load(open('.github/workflows/ci.yml'))" && echo YAML-OK
```

- [ ] **Step 3: Commit**

```bash
cd "$NEW"
git add .github/workflows/ci.yml
git commit -m "ci: add standalone CI (backend, frontend, browser smoke)

All jobs run on ubuntu-24.04 against the PyPI dal-python wheel; no DAL
source build is involved."
```

### Task 9: Move the .claude / .codex web assets

**Files:**
- Create: `$NEW/.claude/rules/dal-web-design.md`, `$NEW/.claude/rules/dal-web-code-style.md`
- Create: `$NEW/.claude/skills/dal-web-setup/SKILL.md`
- Create: `$NEW/.claude/specs/dal-web-db-persistence.md`
- Create: `$NEW/.codex/skills/dal-web/` (whole directory)

- [ ] **Step 1: Copy the assets**

```bash
PARENT=/home/wegamekinglc/dev/github/my-claude/workspace/Derivatives-Algorithms-Lib
NEW=/home/wegamekinglc/dev/github/my-claude/workspace/dal-web
mkdir -p "$NEW/.claude/rules" "$NEW/.claude/skills" "$NEW/.claude/specs" "$NEW/.codex/skills"
cp "$PARENT/.claude/rules/dal-web-design.md" "$PARENT/.claude/rules/dal-web-code-style.md" "$NEW/.claude/rules/"
cp -r "$PARENT/.claude/skills/dal-web-setup" "$NEW/.claude/skills/"
cp "$PARENT/.claude/specs/dal-web-db-persistence.md" "$NEW/.claude/specs/"
cp -r "$PARENT/.codex/skills/dal-web" "$NEW/.codex/skills/"
```

- [ ] **Step 2: Re-root path references**

```bash
cd "$NEW"
grep -rln "dal-web/" .claude .codex | while read -r f; do
  sed -i 's|dal-web/scripts|scripts|g; s|dal-web/backend|backend|g; s|dal-web/frontend|frontend|g' "$f"
done
! grep -rn "dal-web/" .claude .codex && echo NO-STALE-PATHS
grep -rn "dal-web" .claude .codex | grep -v "dal-web-design\|dal-web-code-style\|dal-web-setup\|dal-web-db-persistence\|dal-web skill\|dal-web repo" || true
```

The second grep lists remaining bare `dal-web` mentions (project name in prose is fine; review each hit).

- [ ] **Step 3: Fix the rules' self-references**

- `$NEW/.claude/rules/dal-web-design.md`: "All styles are defined in: `dal-web/frontend/src/styles.css`" was rewritten by sed to `frontend/src/styles.css` — verify:
  ```bash
  grep -n "styles.css" "$NEW/.claude/rules/dal-web-design.md"   # expect frontend/src/styles.css
  ```
- `$NEW/.claude/rules/dal-web-code-style.md`: "Governs: `dal-web/backend/app/`" → expect `backend/app/` after sed — verify:
  ```bash
  grep -n "Governs" "$NEW/.claude/rules/dal-web-code-style.md"
  ```

- [ ] **Step 4: Commit**

```bash
cd "$NEW"
git add .claude .codex
git commit -m "docs: move dal-web agent rules, skills, and specs into the standalone repo"
```

### Task 10: Standalone README + curve-lab doc

**Files:**
- Modify: `$NEW/README.md` (dependency section rewrite + top-matter)
- Create: `$NEW/docs/curve-lab.md` (moved from parent `docs/curve-lab.md`)

- [ ] **Step 1: Rewrite the "DAL dependency" section as "Native DAL package"**

In `$NEW/README.md`, replace the `### DAL dependency` subsection (the block starting "### DAL dependency" and ending before "Runtime configuration:") with a new top-level section placed right after the `## How DAL is used` section's closing line ("...requirement."):

```markdown
## Native DAL package

The backend imports the compiled `dal` package (the
[dal-python](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/tree/master/dal-python)
pybind11 bindings) directly -- it is the sole pricing engine, with no
pure-Python fallback. `dal-python>=2026.8.14` is a declared backend
dependency, so `uv sync` installs the published wheel from PyPI; no C++ build
is needed for normal development and deployment.

To develop against an unreleased DAL build, install from a DAL source
checkout into the backend environment instead:

```bash
cd backend
uv pip install /path/to/Derivatives-Algorithms-Lib/dal-python \
  "--config-settings=cmake.define.DAL_INSTALL_PREFIX=/path/to/build/stage/<platform-preset>"
```

`start.sh`/`start.ps1` run `uv sync --inexact`, which preserves such a
manually installed local binding. `uv run --no-sync python -m
app.native_runtime` preflights the import and required binding symbols.

The pytest suite registers a minimal fake `dal` module (see
`backend/tests/conftest.py`) so the FastAPI wiring can be exercised without
the native package; `native`-marked tests and production import the real
`dal`.
```

Note the section heading must be exactly `## Native DAL package` — `native_runtime.py` points at `README.md#native-dal-package`.

Also delete the now-duplicated fake-dal paragraph left inside `## How DAL is used` if it remains after the swap (the old "### DAL dependency" text contained it; the replacement above includes it — ensure it appears exactly once).

- [ ] **Step 2: Re-root remaining README paths**

```bash
cd "$NEW"
sed -i 's|dal-web/scripts|scripts|g; s|dal-web/backend|backend|g; s|dal-web/frontend|frontend|g' README.md
! grep -n "dal-web/" README.md && echo NO-STALE-PATHS
grep -n "../docs/installation.md\|repository root" README.md || true
```

Replace any surviving `../docs/installation.md#web-ui` link with `#native-dal-package`.

- [ ] **Step 3: Move curve-lab.md and fix its links**

```bash
PARENT=/home/wegamekinglc/dev/github/my-claude/workspace/Derivatives-Algorithms-Lib
NEW=/home/wegamekinglc/dev/github/my-claude/workspace/dal-web
mkdir -p "$NEW/docs"
cp "$PARENT/docs/curve-lab.md" "$NEW/docs/curve-lab.md"
cd "$NEW"
sed -i 's|dal-web/scripts|scripts|g; s|dal-web/backend|backend|g; s|dal-web/frontend|frontend|g' docs/curve-lab.md
sed -i 's|(\.\./dal-web/README.md)|(../README.md)|g' docs/curve-lab.md
sed -i 's|(public-api.md)|(https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/master/docs/public-api.md)|g' docs/curve-lab.md
sed -i 's|(methodology/yield_curve.md)|(https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/master/docs/methodology/yield_curve.md)|g; s|(methodology/yield_curve_jacobian.md)|(https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/master/docs/methodology/yield_curve_jacobian.md)|g' docs/curve-lab.md
grep -n "](\." docs/curve-lab.md   # review remaining relative links; expect only ../README.md
```

Also fix the `dal-python/.venv/bin/python -m pytest -q dal-python/tests` line (it documents running DAL's own tests from the parent checkout): replace it with a pointer to the DAL repo — `https://github.com/wegamekinglc/Derivatives-Algorithms-Lib` for running binding tests.

- [ ] **Step 4: Commit**

```bash
cd "$NEW"
git add README.md docs/
git commit -m "docs: standalone README and curve-lab guide

The native dependency section now installs dal-python from PyPI; cross-links
into DAL documentation use absolute GitHub URLs."
```

### Task 11: Local validation gate (must fully pass before publishing)

- [ ] **Step 1: Backend — fresh sync from PyPI, full test suite**

```bash
cd "$NEW/backend"
rm -rf .venv
uv sync --locked --inexact
uv run --no-sync pytest                 # non-native suite (fake dal)
uv run --no-sync pytest -m native       # native suite (PyPI wheel)
uv run --no-sync ruff check .
uv run --no-sync ruff format --check .
```

Expected: all pass; `uv sync --locked` proves the committed lockfile is in sync.

- [ ] **Step 2: Gate scripts**

```bash
cd "$NEW"
python3 .github/scripts/check_consistency.py
python3 .github/scripts/check_frontend_quote_bump_boundary.py
DAL_NUM_THREADS=2 python3 .github/scripts/check_native_web_gateway.py
(cd backend && uv run --no-sync python scripts/generate_openapi.py)
git -C "$NEW" diff --exit-code backend/openapi/dal-web.openapi.json && echo OPENAPI-FRESH
```

- [ ] **Step 3: Frontend**

```bash
cd "$NEW/frontend"
npm ci
npm run build
npm test
```

- [ ] **Step 4: Launcher smoke — real services**

```bash
cd "$NEW"
./scripts/start.sh        # expect "DAL web UI is running", URLs printed
curl -sf http://127.0.0.1:8001/api/health && echo BACKEND-OK
./scripts/stop.sh
```

- [ ] **Step 5: Browser e2e**

```bash
cd "$NEW"
./scripts/setup-playwright.sh   # skip if chrome/ already present
cd frontend
DAL_PLAYWRIGHT_TEST_BACKEND=1 npm run test:e2e
```

- [ ] **Step 6: Commit any drift found (e.g. regenerated openapi snapshot)**

```bash
cd "$NEW"
git status --short   # expect clean, or commit fixes before continuing
```

---

## Phase 3 — Publish

### Task 12: Create the GitHub repo and push

- [ ] **Step 1: Create and push**

```bash
cd "$NEW"
gh repo create wegamekinglc/dal-web --public --source=. --push
git log --oneline | tail -3
```

- [ ] **Step 2: Watch CI go green on main**

```bash
cd "$NEW"
gh run list --limit 3
RUN_ID=$(gh run list --branch main --limit 1 --json databaseId -q '.[0].databaseId')
gh run watch "$RUN_ID" --exit-status --interval 60
```

Expected: Backend / Frontend / Browser smoke all pass. If a job fails, fix in the local repo, commit, push, re-watch.

- [ ] **Step 3: Set the repo description**

```bash
gh repo edit wegamekinglc/dal-web --description "Portfolio management web UI for the DAL quantitative finance library (FastAPI + React), powered by the dal-python PyPI package"
```

---

## Phase 4 — Parent-repo cleanup PR

All Phase 4 work happens in `$PARENT` on branch `chore/extract-dal-web` (already exists with the spec + plan commits).

### Task 13: Delete dal-web and its CI

**Files:**
- Delete: `$PARENT/dal-web/` (147 tracked files)
- Delete: `$PARENT/.github/workflows/web-calibration-performance.yml`
- Delete: `$PARENT/.github/scripts/check_native_web_gateway.py`, `generate_web_calibration_perf_reports.py`, `generate_web_openapi.py`, `run_web_calibration_perf.py`, `check_frontend_quote_bump_boundary.py`
- Modify: `$PARENT/.github/workflows/cmake-linux.yml`

- [ ] **Step 1: Delete the tree and web-only scripts/workflows**

```bash
cd "$PARENT"
git rm -r --quiet dal-web
git rm --quiet .github/workflows/web-calibration-performance.yml \
  .github/scripts/check_native_web_gateway.py \
  .github/scripts/generate_web_calibration_perf_reports.py \
  .github/scripts/generate_web_openapi.py \
  .github/scripts/run_web_calibration_perf.py \
  .github/scripts/check_frontend_quote_bump_boundary.py
```

(`generate_web_openapi.py` is unreferenced — the documented generator is `backend/scripts/generate_openapi.py`, which moved with the snapshot. `run_web_calibration_perf.py` only served the deleted performance workflow.)

- [ ] **Step 2: Remove the web-quality job from cmake-linux.yml**

In `.github/workflows/cmake-linux.yml`, delete the entire `web-quality:` job (from the comment block `# Web product surfaces are backend-agnostic...` through the `Run browser smoke tests` step, ending right before `documentation:`).

- [ ] **Step 3: Remove the native web gateway step from build-extended**

In the same file's `build-extended:` job, delete the `Test native DAL web gateway` step:

```yaml
      - name: Test native DAL web gateway
        env:
          DAL_NUM_THREADS: 2
          PYTHONPATH: ${{ github.workspace }}/build/stage/Release-linux:${{ github.workspace }}/dal-web/backend
        run: |
          python .github/scripts/check_native_web_gateway.py
          python -m pytest dal-web/backend/tests/test_curve_reconstruction_process.py \
            -m native
```

Also delete its companion comment lines if any reference the web gateway, and the `uv pip install pytest numpy httpx2 -e dal-web/backend` line in the `Set up Python venv` step (replace with `uv pip install pytest numpy` — the venv still needs pytest for `dal_python_pytest`).

- [ ] **Step 4: Verify the workflow**

```bash
cd "$PARENT"
python3 -c "import yaml; yaml.safe_load(open('.github/workflows/cmake-linux.yml'))" && echo YAML-OK
! grep -n "dal-web" .github/workflows/cmake-linux.yml && echo NO-STALE-REFS
```

- [ ] **Step 5: Commit**

```bash
cd "$PARENT"
git add -A
git commit -m "chore: remove dal-web and its CI from the parent repository

dal-web now lives at github.com/wegamekinglc/dal-web against the published
dal-python PyPI package."
```

### Task 14: Excise dal-web checks from check_docs.py

**Files:**
- Modify: `$PARENT/.github/scripts/check_docs.py`
- Modify: `$PARENT/.github/scripts/tests/test_check_docs.py`

- [ ] **Step 1: Remove dal-web entries and checks from check_docs.py**

Concrete removals (verify each against current line content before deleting):

1. Line ~18: `ROOT / "dal-web/backend/README.md",` from the docs list.
2. Line ~81: `"dal-web/",` and line ~89: `"dal-web/backend/.data",` ignore entries.
3. `check_curve_lab_endpoint_inventory` (line ~412) — the whole function reads `dal-web/backend/openapi/dal-web.openapi.json`; delete the function and its call site in `main()`.
4. `requirement_name`, `backend_declared_requirements`, `backend_requirements_file`, `check_backend_requirement_sets` (lines ~446-489) — delete all four and the call site.
5. `linux_extended_test_packages`, `check_linux_extended_test_packages` (line ~491+) — delete both and the call site.
6. `check_web_launcher_portability` (line ~568) — delete the function and its call site.

After each deletion, grep for the deleted name to confirm no call site remains:

```bash
cd "$PARENT"
! grep -n "dal-web\|dal_web" .github/scripts/check_docs.py && echo CLEAN
python3 -c "import ast; ast.parse(open('.github/scripts/check_docs.py').read())" && echo PARSE-OK
```

- [ ] **Step 2: Update test_check_docs.py**

Remove/adjust the dal-web-coupled assertions:

1. Line ~21: `self.assertIn(".codex/skills/dal-web/SKILL.md", relative)` — delete (the skill is gone).
2. Line ~25: `self.assertIn("dal-web/backend/README.md", relative)` — delete.
3. Lines ~55-58: the `agent_referenced_paths` case asserting `dal-web/scripts/setup-playwright.sh` — replace the fixture path with a still-existing script (e.g. `dal-python/build_wheel.sh`) and update the expected value accordingly.
4. Line ~148: the message asserting `` `dal-web/backend/.data/` `` — read the surrounding test; it checks a docs-consistency message listing local state dirs. Remove the dal-web segment from the expected string (keep `.claude` portion).

```bash
cd "$PARENT"
python3 -m pytest .github/scripts/tests/test_check_docs.py -q  # or: python3 .github/scripts/tests/test_check_docs.py
```

- [ ] **Step 3: Run the docs gate**

```bash
cd "$PARENT"
python3 .github/scripts/check_docs.py && echo DOCS-OK
```

This will initially fail on Task-15 items (docs still referencing dal-web); complete Task 15, then re-run until green, then commit Tasks 14+15 together.

### Task 15: Scrub dal-web references across docs and agent config

**Files (all in `$PARENT`):**
- Modify: `CLAUDE.md`, `AGENTS.md`, `README.md`, `CONTRIBUTING.md`, `docs/README.md`, `docs/architecture.md`, `docs/installation.md`, `.claude/agents/dal-tester.md`, `.codex/README.md`, `.codex/skills/dal-agent-team/references/shared-rules.md`
- Delete: `.claude/rules/dal-web-design.md`, `.claude/rules/dal-web-code-style.md`, `.claude/skills/dal-web-setup/`, `.claude/specs/dal-web-db-persistence.md`, `.codex/skills/dal-web/`, `docs/curve-lab.md`

- [ ] **Step 1: Delete migrated assets**

```bash
cd "$PARENT"
git rm --quiet .claude/rules/dal-web-design.md .claude/rules/dal-web-code-style.md \
  .claude/specs/dal-web-db-persistence.md docs/curve-lab.md
git rm -r --quiet .claude/skills/dal-web-setup .codex/skills/dal-web
```

- [ ] **Step 2: CLAUDE.md**

- Remove the `└── dal-web/` line from the workspace tree.
- Remove the `**Web UI (dal-web/...**` paragraph (the FastAPI + React description including the Store/env-var details and the start-script instructions, through `npm run test:e2e`).
- Remove the two dal-web rule bullets: `**web UI design**: ...` and `**dal-web backend style**: ...`.
- Architecture tree at the top: remove the `dal-web/` row if present (check the ``` block listing sub-projects).

- [ ] **Step 3: AGENTS.md**

- Remove/rewrite: the `.codex/skills/` mention of `dal-web` (line ~16), the `dal-web/ ... not built by CMake` paragraph (line ~24), the three "Web operations / Web backend rules / Web UI design" link lines (~55-59), and the `Use dal-web for web work` bullet (~82).

- [ ] **Step 4: README.md**

- Remove the `dal-web/` row from the component table (line ~66) and the dal-web node from the dependency diagram (line ~51).
- Remove the Web UI section (lines ~149-170: start/stop/e2e commands and the `[dal-web/README.md]` link).

- [ ] **Step 5: CONTRIBUTING.md**

- Remove `dal-web` from the component list (line ~11) and the web check commands (lines ~81-85: the `dal-web/backend` pytest/ruff, `dal-web/frontend` build, setup-playwright, test:e2e block).

- [ ] **Step 6: docs/README.md, docs/architecture.md, docs/installation.md**

- `docs/README.md`: remove the `curve-lab.md` entry and any dal-web mention.
- `docs/architecture.md`: remove the dal-web component/flow descriptions; keep the C++/Python architecture intact.
- `docs/installation.md`: delete the entire `## Web UI` section (from `## Web UI` through the line before `## Verification`), and in `## Verification` delete the web check lines:
  ```
  (cd dal-web/backend && uv run --no-sync pytest)
  (cd dal-web/frontend && npm run build)
  (cd dal-web/frontend && npm test)
  ./dal-web/scripts/setup-playwright.sh
  (cd dal-web/frontend && npm run test:e2e)
  ```

- [ ] **Step 7: .claude/agents/dal-tester.md, .codex/README.md, shared-rules.md**

- `dal-tester.md`: remove dal-web e2e/Playwright scope sentences (the agent becomes C++/gtest-only).
- `.codex/README.md`: remove dal-web skill references.
- `shared-rules.md`: remove dal-web rule links.

- [ ] **Step 8: Global verify + run docs gate, then commit Tasks 14-15**

```bash
cd "$PARENT"
grep -rn "dal-web\|dal_web" --include="*.md" . | grep -v "^./.git" | grep -v worktrees | grep -v ".claude/specs/2026-08-14-dal-web-extraction"
```

Review remaining hits: legitimate mentions are only the extraction spec/plan, `CHANGELOG.md` history, and the new "dal-web moved to ..." pointer (add one line to `README.md`: the web UI now lives at `https://github.com/wegamekinglc/dal-web`).

```bash
python3 .github/scripts/check_docs.py && echo DOCS-OK
git add -A
git commit -m "docs: scrub dal-web references after extraction

Point the web UI pointer at the new wegamekinglc/dal-web repository; remove
migrated rules, skills, specs, and the curve-lab guide (now in dal-web)."
```

### Task 16: CHANGELOG, validation, PR

**Files:**
- Modify: `$PARENT/CHANGELOG.md`

- [ ] **Step 1: CHANGELOG entry**

Add at the top of `CHANGELOG.md` (follow the existing entry format):

```markdown
- 2026-08-14: Removed `dal-web/` from this repository. The portfolio
  management web UI now lives at
  [wegamekinglc/dal-web](https://github.com/wegamekinglc/dal-web) and depends
  on the published `dal-python` PyPI package. The
  `web-calibration-performance` workflow and the web CI jobs were removed
  with it.
```

- [ ] **Step 2: Full parent validation**

```bash
cd "$PARENT"
bash ./build_linux.sh          # configure + build + install + ctest
python3 .github/scripts/check_docs.py
```

Expected: build succeeds; `100% tests passed`; DOCS-OK.

- [ ] **Step 3: Push and open the PR**

```bash
cd "$PARENT"
git push -u origin chore/extract-dal-web
gh pr create --base master --title "chore: extract dal-web into its own repository" --body "## Summary
- Remove dal-web/ (moved to https://github.com/wegamekinglc/dal-web, depending on PyPI dal-python>=2026.8.14)
- Remove the web-quality CI job, the build-extended native web gateway step, and web-calibration-performance.yml
- Remove web-only .github/scripts gates and the dal-web checks in check_docs.py
- Scrub dal-web references across CLAUDE.md / AGENTS.md / README.md / CONTRIBUTING.md / docs / .claude / .codex; CHANGELOG entry

## Test plan
- [x] New repo CI green on main (backend incl. native, frontend, Playwright e2e)
- [x] \`bash ./build_linux.sh\` + ctest green locally
- [x] \`check_docs.py\` green
- [ ] CI green on the exact PR head"
```

- [ ] **Step 4: Watch CI, then hand off for merge (user merges)**

```bash
cd "$PARENT"
gh pr checks --watch --interval 60
```

Report ready-to-merge state to the user. Do not merge.

---

## Self-Review Notes (completed by plan author)

- Spec coverage: Stage 1 → Tasks 1-11; Stage 2 → Task 12; Stage 3 → Tasks 13-16. PyPI dependency rationale → Task 2 verification. Out-of-scope items (perf workflow migration, Windows CI, history) are only deleted, never built.
- Placeholder scan: every file creation contains full content; every edit has exact old→new or exact sed + verification grep. No TBD/TODO.
- Consistency: `REPO_ROOT` rewrite pattern is identical across the 5 bash scripts; the README anchor `native-dal-package` matches the string asserted in `test_native_runtime.py`; the new repo default branch is `main` (Task 1), matching the CI `push.branches` filter (Task 8) and the `gh run list --branch main` watch (Task 12).
